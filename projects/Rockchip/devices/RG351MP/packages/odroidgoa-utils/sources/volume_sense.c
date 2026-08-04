#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>
#include <libevdev/libevdev.h>
#include <alsa/asoundlib.h>

#define RG351_DEVICE "/dev/input/by-path/platform-rg351-keys-event"
#define RG351_CONTROLLER_DEVICE "/dev/input/event2"
#define BRIGHTNESS_PATH "/sys/class/backlight/backlight/brightness"
#define VOLUME_STATE_PATH "/run/volume_sense.volume"
#define GET_EE_SETTING_CMD "sh -c '. /etc/profile 2>/dev/null; get_ee_setting \"%s\"'"
#define SET_EE_SETTING_CMD "sh -c '. /etc/profile 2>/dev/null; set_ee_setting \"%s\" \"%s\"'"
#define WRITE_WAIT_MS 5000
#define MAX_BRIGHTNESS 100
#define VOLUME_STEP 1
#define BRIGHTNESS_STEP 1
#define CLAMP(v, min, max) ((v) < (min) ? (min) : (v) > (max) ? (max) : (v))

static int func_pressed = 0;
static int volume_dirty = 0;
static int brightness_dirty = 0;
static long last_volume_change = 0;
static long last_brightness_change = 0;
static int pending_volume_percent = -1;
static int pending_brightness = -1;
static int current_volume_percent = -1;
static snd_mixer_t *mixer_handle = NULL;
static snd_mixer_elem_t *mixer_elem = NULL;

static int init_mixer(void) {
    snd_mixer_selem_id_t *sid;
    int err = snd_mixer_open(&mixer_handle, 0);
    if (err < 0) return err;
    snd_mixer_attach(mixer_handle, "default");
    snd_mixer_selem_register(mixer_handle, NULL, NULL);
    snd_mixer_load(mixer_handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, "Playback");
    mixer_elem = snd_mixer_find_selem(mixer_handle, sid);
    if (!mixer_elem) {
        snd_mixer_selem_id_set_name(sid, "Master");
        mixer_elem = snd_mixer_find_selem(mixer_handle, sid);
    }
    if (!mixer_elem) {
        for (snd_mixer_elem_t *elem = snd_mixer_first_elem(mixer_handle);
             elem;
             elem = snd_mixer_elem_next(elem)) {
            if (snd_mixer_selem_is_active(elem) && snd_mixer_selem_has_playback_volume(elem)) {
                mixer_elem = elem;
                break;
            }
        }
    }
    return mixer_elem ? 0 : -ENOENT;
}

static void close_mixer(void) {
    if (mixer_handle) {
        snd_mixer_close(mixer_handle);
        mixer_handle = NULL;
        mixer_elem = NULL;
    }
}

static int get_ee_setting(const char *key, char *value, size_t size) {
    if (!key || !value || size == 0) return -1;

    char cmd[300];
    snprintf(cmd, sizeof(cmd), GET_EE_SETTING_CMD, key);
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    int ret = -1;
    if (fgets(value, size, fp)) {
        size_t len = strlen(value);
        if (len > 0 && value[len-1] == '\n') value[len-1] = '\0';
        ret = (strlen(value) > 0) ? 0 : -1;
    }

    pclose(fp);
    return ret;
}

static int set_ee_setting(const char *key, const char *value) {
    if (!key || !value) return -1;

    size_t vlen = strlen(value);
    if (strlen(key) > 256 || vlen > 2048) return -1;

    char safe_value[2100];
    int pos = 0;
    for (size_t i = 0; i < vlen && pos < (int)sizeof(safe_value) - 2; i++) {
        if (value[i] == '"' || value[i] == '\\' || value[i] == '`' || value[i] == '$') {
            safe_value[pos++] = '\\';
        }
        safe_value[pos++] = value[i];
    }
    safe_value[pos] = '\0';

    char cmd[2600];
    snprintf(cmd, sizeof(cmd), SET_EE_SETTING_CMD, key, safe_value);
    return (system(cmd) == 0) ? 0 : -1;
}

static int get_current_volume_percent(void) {
    if (!mixer_elem) return -1;

    long min_v, max_v, vol;
    if (snd_mixer_selem_get_playback_volume_range(mixer_elem, &min_v, &max_v) < 0) return -1;
    if (snd_mixer_selem_get_playback_volume(mixer_elem, SND_MIXER_SCHN_FRONT_LEFT, &vol) < 0) return -1;

    if (max_v == min_v) return 0;
    int percent = (int)((vol - min_v) * 100 / (max_v - min_v));
    return CLAMP(percent, 0, 100);
}

static int set_volume_to_percent(int percent) {
    if (!mixer_elem) return -1;

    long min_v, max_v;
    if (snd_mixer_selem_get_playback_volume_range(mixer_elem, &min_v, &max_v) < 0) return -1;

    percent = CLAMP(percent, 0, 100);
    long vol = min_v + ((max_v - min_v) * percent + 50) / 100;
    return snd_mixer_selem_set_playback_volume_all(mixer_elem, vol) < 0 ? -1 : 0;
}

static void publish_volume(int percent) {
    percent = CLAMP(percent, 0, 100);

    char buf[5];
    int len = snprintf(buf, sizeof(buf), "%d\n", percent);
    if (len <= 0) return;

    int fd = open(VOLUME_STATE_PATH ".tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    if (write(fd, buf, len) == len) {
        close(fd);
        rename(VOLUME_STATE_PATH ".tmp", VOLUME_STATE_PATH);
    } else {
        close(fd);
    }
}

static void persist_volume(int percent) {
    if ((unsigned)percent <= 100) {
        char buf[5];
        snprintf(buf, sizeof(buf), "%d", percent);
        set_ee_setting("audio.volume", buf);
    }
}

static void persist_brightness(int brightness) {
    if ((unsigned)brightness <= MAX_BRIGHTNESS) {
        char buf[5];
        snprintf(buf, sizeof(buf), "%d", brightness);
        set_ee_setting("brightness.level", buf);
    }
}

static long get_monotonic_time_msec(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        return time(NULL) * 1000L;
    }
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void flush_pending_settings(void) {
    long now = get_monotonic_time_msec();
    if (volume_dirty && now - last_volume_change >= WRITE_WAIT_MS) {
        persist_volume(pending_volume_percent);
        volume_dirty = 0;
    }
    if (brightness_dirty && now - last_brightness_change >= WRITE_WAIT_MS) {
        persist_brightness(pending_brightness);
        brightness_dirty = 0;
    }
}

static void sync_volume_setting(void) {
    char setting[5];
    int persisted = get_ee_setting("audio.volume", setting, sizeof(setting)) == 0 ? atoi(setting) : -1;
    if ((unsigned)persisted <= 100) {
        if (set_volume_to_percent(persisted) == 0) current_volume_percent = persisted;
    } else {
        int current = get_current_volume_percent();
        if (current >= 0) {
            current_volume_percent = current;
            persist_volume(current);
        }
    }
    if (current_volume_percent >= 0) publish_volume(current_volume_percent);
}

static void sync_brightness_setting(void) {
    char setting[5];
    int persisted = get_ee_setting("brightness.level", setting, sizeof(setting)) == 0 ? atoi(setting) : -1;
    
    if ((unsigned)persisted <= MAX_BRIGHTNESS) {
        int fd = open(BRIGHTNESS_PATH, O_WRONLY);
        if (fd >= 0) {
            int len = snprintf(setting, sizeof(setting), "%d", persisted);
            write(fd, setting, len);
            close(fd);
        }
    } else {
        int fd = open(BRIGHTNESS_PATH, O_RDONLY);
        if (fd >= 0) {
            ssize_t len = read(fd, setting, sizeof(setting) - 1);
            close(fd);
            if (len > 0) {
                setting[len] = '\0';
                if (setting[len-1] == '\n') setting[len-1] = '\0';
                int brightness = atoi(setting);
                if ((unsigned)brightness <= MAX_BRIGHTNESS) persist_brightness(brightness);
            }
        }
    }
}

static void set_volume(int direction, int amount) {
    if (!mixer_elem) return;

    int current = current_volume_percent >= 0 ? current_volume_percent : get_current_volume_percent();
    if (current < 0) return;

    int new_percent = CLAMP(current + direction * amount, 0, 100);
    if (set_volume_to_percent(new_percent) == 0) {
        current_volume_percent = new_percent;
        publish_volume(new_percent);
        pending_volume_percent = new_percent;
        last_volume_change = get_monotonic_time_msec();
        volume_dirty = 1;
    }
}

static void set_brightness(int direction, int amount) {
    int fd = open(BRIGHTNESS_PATH, O_RDONLY);
    if (fd < 0) return;

    char buf[5];
    ssize_t len = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (len <= 0) return;

    buf[len] = '\0';
    int brightness = CLAMP(atoi(buf) + direction * amount, 1, MAX_BRIGHTNESS);

    fd = open(BRIGHTNESS_PATH, O_WRONLY);
    if (fd < 0) return;
    int out_len = snprintf(buf, sizeof(buf), "%d", brightness);
    if (write(fd, buf, out_len) == out_len) {
        pending_brightness = brightness;
        last_brightness_change = get_monotonic_time_msec();
        brightness_dirty = 1;
    }
    close(fd);
}

int main(void) {
    struct libevdev *dev1 = NULL, *dev2 = NULL;
    int fd1 = -1, fd2 = -1;

    while (1) {
        fd1 = open(RG351_DEVICE, O_RDONLY | O_NONBLOCK);
        fd2 = open(RG351_CONTROLLER_DEVICE, O_RDONLY | O_NONBLOCK);
        if (fd1 >= 0 && fd2 >= 0) break;
        if (fd1 >= 0) close(fd1);
        if (fd2 >= 0) close(fd2);
        sleep(1);
    }

    if (init_mixer() < 0) {
        close(fd1);
        close(fd2);
        return 1;
    }

    sync_volume_setting();
    sync_brightness_setting();

    libevdev_new_from_fd(fd1, &dev1);
    libevdev_new_from_fd(fd2, &dev2);

    struct input_event ev;
    struct pollfd fds[2] = {
        { .fd = fd1, .events = POLLIN },
        { .fd = fd2, .events = POLLIN }
    };

    while (1) {
        int timeout = (volume_dirty || brightness_dirty) ? WRITE_WAIT_MS : -1;
        int poll_ret = poll(fds, 2, timeout);
        if (poll_ret < 0) continue;
        if (poll_ret == 0) {
            flush_pending_settings();
            continue;
        }

        for (int i = 0; i < 2; ++i) {
            if (!(fds[i].revents & POLLIN)) continue;

            int ret = libevdev_next_event(i == 0 ? dev1 : dev2, LIBEVDEV_READ_FLAG_NORMAL, &ev);
            if (ret != LIBEVDEV_READ_STATUS_SUCCESS) continue;
            if (ev.type != EV_KEY) continue;
            if (ev.code == KEY_VOLUMEDOWN || ev.code == KEY_VOLUMEUP) {
                if (ev.value == 0) continue;

                int direction = (ev.code == KEY_VOLUMEUP) ? 1 : -1;

                if (func_pressed) {
                    set_brightness(direction, BRIGHTNESS_STEP);
                } else {
                    set_volume(direction, VOLUME_STEP);
                }
            } else if (ev.code == BTN_TRIGGER_HAPPY4) {
                func_pressed = (ev.value == 1);
            }
        }

        flush_pending_settings();
    }

    libevdev_free(dev1);
    libevdev_free(dev2);
    close_mixer();
    close(fd1);
    close(fd2);

    return 0;
}