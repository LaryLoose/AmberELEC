#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define GPIO_PATH "/sys/class/gpio/gpio77"
#define BATTERY_CAPACITY "/sys/class/power_supply/battery/capacity"
#define BATTERY_STATUS "/sys/class/power_supply/battery/status"
#define ROMS_PATH "/roms"

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

    while (1) {
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

        if (strcmp(stat, "Discharging") == 0) {
            if (cap <= 10) {
                for (int ctr = 0; ctr < 5; ctr++) {
                    set_led(r3xs_yellow);
                    usleep(500000); // 0.5s
                    set_led(r3xs_red);
                    usleep(500000); // 0.5s
                }
                continue;
            } else if (cap <= 20) {
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

    return 0;
}