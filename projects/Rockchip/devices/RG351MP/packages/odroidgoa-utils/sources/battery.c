#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#define GPIO_PATH "/sys/class/gpio/gpio77"
#define BATTERY_CAPACITY "/sys/class/power_supply/battery/capacity"
#define BATTERY_STATUS "/sys/class/power_supply/battery/status"
#define ROMS_PATH "/roms"
#define LOG_DIR "/storage/.config/battery"

static volatile sig_atomic_t terminate = 0;

static void on_signal(int sig) {
    (void)sig;
    terminate = 1;
}

void log_message(const char *msg) {
    struct stat st;
    if (stat(LOG_DIR, &st) != 0 || !S_ISDIR(st.st_mode)) return;

    time_t now = time(NULL);
    struct tm tm;
    char day[16];
    char ts[32];
    char path[64];
    localtime_r(&now, &tm);
    strftime(day, sizeof(day), "%Y-%m-%d", &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
    snprintf(path, sizeof(path), "%s/battery-%s.log", LOG_DIR, day);

    FILE *fp = fopen(path, "a");
    if (!fp) return;
    fprintf(fp, "%s %s\n", ts, msg);
    fclose(fp);
}

void log_level(int cap, const char *status) {
    char line[64];
    snprintf(line, sizeof(line), "level=%d status=%s", cap, status);
    log_message(line);
}

enum Color { RED, GREEN, YELLOW, PURPLE };

enum Color r3xs_green = GREEN;
enum Color r3xs_red = RED;
enum Color r3xs_yellow = YELLOW;

void set_led(enum Color color) {
    int fd;
    char path[256];

    switch (color) {
        case RED:
            // direction out, value 1
            snprintf(path, sizeof(path), "%s/direction", GPIO_PATH);
            fd = open(path, O_WRONLY);
            if (fd >= 0) {
                write(fd, "out", 3);
                close(fd);
            }
            snprintf(path, sizeof(path), "%s/value", GPIO_PATH);
            fd = open(path, O_WRONLY);
            if (fd >= 0) {
                write(fd, "1", 1);
                close(fd);
            }
            break;
        case GREEN:
            // direction out, value 0
            snprintf(path, sizeof(path), "%s/direction", GPIO_PATH);
            fd = open(path, O_WRONLY);
            if (fd >= 0) {
                write(fd, "out", 3);
                close(fd);
            }
            snprintf(path, sizeof(path), "%s/value", GPIO_PATH);
            fd = open(path, O_WRONLY);
            if (fd >= 0) {
                write(fd, "0", 1);
                close(fd);
            }
            break;
        case YELLOW:
            // direction in
            snprintf(path, sizeof(path), "%s/direction", GPIO_PATH);
            fd = open(path, O_WRONLY);
            if (fd >= 0) {
                write(fd, "in", 2);
                close(fd);
            }
            break;
        case PURPLE:
            // direction out, value 0 then 1 (but this is tricky, perhaps toggle)
            // For simplicity, set to out and 0
            snprintf(path, sizeof(path), "%s/direction", GPIO_PATH);
            fd = open(path, O_WRONLY);
            if (fd >= 0) {
                write(fd, "out", 3);
                close(fd);
            }
            snprintf(path, sizeof(path), "%s/value", GPIO_PATH);
            fd = open(path, O_WRONLY);
            if (fd >= 0) {
                write(fd, "0", 1);
                close(fd);
            }
            break;
    }
}

int check_led_files() {
    DIR *dir = opendir(ROMS_PATH);
    if (!dir) return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "led") != NULL) {
            closedir(dir);
            return 1;
        }
    }
    closedir(dir);
    return 0;
}

int main() {
    if (check_led_files()) {
        r3xs_green = RED;
        r3xs_red = GREEN;
        r3xs_yellow = PURPLE;
    }

    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    int prev_cap = -1;
    char prev_stat[32] = {0};

    while (!terminate) {
        int cap = 0;
        char stat[32] = {0};

        FILE *fp = fopen(BATTERY_CAPACITY, "r");
        if (fp) {
            fscanf(fp, "%d", &cap);
            fclose(fp);
        }

        fp = fopen(BATTERY_STATUS, "r");
        if (fp) {
            fscanf(fp, "%s", stat);
            fclose(fp);
        }

        if (cap != prev_cap || strcmp(stat, prev_stat) != 0) {
            log_level(cap, stat);
            prev_cap = cap;
            strncpy(prev_stat, stat, sizeof(prev_stat) - 1);
            prev_stat[sizeof(prev_stat) - 1] = '\0';
        }

        if (strcmp(stat, "Discharging") == 0) {
            if (cap <= 5) {
                for (int ctr = 0; ctr < 5; ctr++) {
                    set_led(r3xs_yellow);
                    usleep(500000); // 0.5s
                    set_led(r3xs_red);
                    usleep(500000); // 0.5s
                }
                continue;
            } else if (cap <= 10) {
                set_led(r3xs_red);
            } else if (cap <= 30) {
                set_led(r3xs_yellow);
            } else {
                set_led(r3xs_green);
            }
        } else if (cap >= 95) {
            set_led(r3xs_green);
        } else {
            set_led(r3xs_red);
        }

        usleep(5000000); // 5s
    }

    log_message("Shutdown");
    return 0;
}