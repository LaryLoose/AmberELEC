// SPDX-License-Identifier: GPL-2.0-or-later
// Live power monitor for the Anbernic RG351MP.

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

namespace {

volatile sig_atomic_t g_running = 1;

void on_signal(int) { g_running = 0; }

constexpr char kClearScreen[] = "\x1b[2J";
constexpr char kCursorHome[] = "\x1b[H";
constexpr char kClearEol[] = "\x1b[K";
constexpr char kHideCursor[] = "\x1b[?25l";
constexpr char kShowCursor[] = "\x1b[?25h";

void write_all(int fd, const char* buffer, size_t length) {
    while (length > 0) {
        ssize_t written = write(fd, buffer, length);
        if (written <= 0) {
            if (written < 0 && errno == EINTR) continue;
            return;
        }
        buffer += written;
        length -= static_cast<size_t>(written);
    }
}

void write_str(int fd, const char* string) { write_all(fd, string, strlen(string)); }

void restore_terminal() {
    write_str(STDOUT_FILENO, kShowCursor);
    write_str(STDOUT_FILENO, "\n");
}

struct SysNode {
    int fd = -1;

    void open_at(int directory_fd, const char* name) {
        fd = openat(directory_fd, name, O_RDONLY | O_CLOEXEC);
    }

    ssize_t read_raw(char* buffer, size_t capacity) const {
        if (fd < 0) return -1;
        if (lseek(fd, 0, SEEK_SET) < 0) return -1;
        ssize_t read_count = read(fd, buffer, capacity - 1);
        if (read_count < 0) return -1;
        buffer[read_count] = '\0';
        return read_count;
    }
};

long long read_int(const SysNode& node, bool& ok) {
    char buffer[64];
    if (node.read_raw(buffer, sizeof buffer) < 0) {
        ok = false;
        return 0;
    }
    char* end = nullptr;
    long long value = strtoll(buffer, &end, 10);
    ok = (end != buffer);
    return value;
}

struct Fcc {
    long long capacity_mah = 0;
    long long learned_mah = 0;
    long long valid = 0;
};

Fcc read_fcc(const SysNode& node, bool& ok) {
    char buffer[64];
    Fcc value;
    if (node.read_raw(buffer, sizeof buffer) < 0 ||
        sscanf(buffer, "%lld %lld %lld", &value.capacity_mah,
               &value.learned_mah, &value.valid) != 3) {
        ok = false;
        return {};
    }
    ok = true;
    return value;
}

void read_text(const SysNode& node, char* output, size_t capacity) {
    char buffer[64];
    if (node.read_raw(buffer, sizeof buffer) < 0) {
        snprintf(output, capacity, "n/a");
        return;
    }
    size_t length = strcspn(buffer, "\r\n");
    if (length >= capacity) length = capacity - 1;
    memcpy(output, buffer, length);
    output[length] = '\0';
    if (length == 0) snprintf(output, capacity, "n/a");
}

struct Stats {
    bool has = false;
    double ema = 0.0;
    double min = 0.0;
    double max = 0.0;
    double alpha;

    explicit Stats(double smoothing_factor) : alpha(smoothing_factor) {}

    void update(double watts) {
        if (!has) {
            ema = min = max = watts;
            has = true;
            return;
        }
        ema += alpha * (watts - ema);
        if (watts < min) min = watts;
        if (watts > max) max = watts;
    }
};

void render_bar(char* output, size_t capacity, double value, double scale, int width) {
    if (width > static_cast<int>(capacity) - 3) width = static_cast<int>(capacity) - 3;
    int filled = 0;
    if (scale > 0.0) {
        filled = static_cast<int>((value / scale) * width + 0.5);
        if (filled < 0) filled = 0;
        if (filled > width) filled = width;
    }
    size_t position = 0;
    output[position++] = '[';
    for (int column = 0; column < width; ++column)
        output[position++] = column < filled ? '#' : '-';
    output[position++] = ']';
    output[position] = '\0';
}

double mono_now() {
    struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return static_cast<double>(timestamp.tv_sec) +
           static_cast<double>(timestamp.tv_nsec) * 1e-9;
}

void format_timestamp(char* output, size_t capacity) {
    struct timespec timestamp;
    clock_gettime(CLOCK_REALTIME, &timestamp);
    struct tm local_time;
    localtime_r(&timestamp.tv_sec, &local_time);
    strftime(output, capacity, "%Y-%m-%d %H:%M:%S", &local_time);
}

void print_usage(const char* argv0) {
    fprintf(stderr,
            "usage: %s [-i seconds] [-p sysfs_base_path] [-l file] [-d secs]\n"
            "  -i  refresh interval in seconds (default 2.5)\n"
            "  -p  power_supply base path\n"
            "      (default /sys/class/power_supply/battery)\n"
            "  -l  log mode: append one CSV line per update to <file>\n"
            "  -d  run for <secs> seconds, then exit (0 = until Ctrl-C)\n",
            argv0);
}

}  // namespace

int main(int argc, char** argv) {
    double interval = 2.5;
    const char* base = "/sys/class/power_supply/battery";
    const char* logfile = nullptr;
    double duration = 0.0;

    int option;
    while ((option = getopt(argc, argv, "i:p:l:d:h")) != -1) {
        switch (option) {
            case 'i': {
                char* end = nullptr;
                double value = strtod(optarg, &end);
                if (end == optarg || value <= 0.0) {
                    fprintf(stderr, "invalid interval: %s\n", optarg);
                    return 1;
                }
                interval = value;
                break;
            }
            case 'p': base = optarg; break;
            case 'l': logfile = optarg; break;
            case 'd': {
                char* end = nullptr;
                double value = strtod(optarg, &end);
                if (end == optarg || value < 0.0) {
                    fprintf(stderr, "invalid duration: %s\n", optarg);
                    return 1;
                }
                duration = value;
                break;
            }
            case 'h':
            default:
                print_usage(argv[0]);
                return option == 'h' ? 0 : 1;
        }
    }

    int directory_fd = open(base, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory_fd < 0) {
        fprintf(stderr, "cannot open sysfs base '%s': %s\n", base, strerror(errno));
        return 1;
    }

    // capacity is the learned Coulomb SoC.
    SysNode current, voltage, capacity, fcc, status;
    current.open_at(directory_fd, "current_now");
    voltage.open_at(directory_fd, "voltage_now");
    capacity.open_at(directory_fd, "capacity");
    fcc.open_at(directory_fd, "fcc");
    status.open_at(directory_fd, "status");

    struct sigaction signal_action {};
    signal_action.sa_handler = on_signal;
    sigaction(SIGINT, &signal_action, nullptr);
    sigaction(SIGTERM, &signal_action, nullptr);

    struct timespec period;
    period.tv_sec = static_cast<time_t>(interval);
    period.tv_nsec = static_cast<long>((interval - period.tv_sec) * 1e9);

    Stats stats(0.2);
    int log_fd = -1;
    bool log_mode = logfile != nullptr;
    if (log_mode) {
        log_fd = open(logfile, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
        if (log_fd < 0) {
            fprintf(stderr, "cannot open log file '%s': %s\n", logfile, strerror(errno));
            return 1;
        }
        struct stat file_status;
        if (fstat(log_fd, &file_status) == 0 && file_status.st_size == 0) {
            const char* header = "timestamp,power_w,current_ma,voltage_v,capacity_coulomb_pct,fcc_mah,fcc_learning,status\n";
            write_all(log_fd, header, strlen(header));
        }
    } else {
        write_str(STDOUT_FILENO, kHideCursor);
        write_str(STDOUT_FILENO, kClearScreen);
    }

    char frame[2048];
    char bar[80];
    char status_text[32];
    long long previous_current = 0;
    bool have_previous_current = false;
    double start = mono_now();

    while (g_running) {
        bool current_ok = false, voltage_ok = false, capacity_ok = false;
        bool fcc_ok = false;
        long long current_ua = read_int(current, current_ok);
        long long voltage_uv = read_int(voltage, voltage_ok);
        long long capacity_pct = read_int(capacity, capacity_ok);
        Fcc fcc_status = read_fcc(fcc, fcc_ok);
        read_text(status, status_text, sizeof status_text);

        double now = mono_now();
        bool power_ok = current_ok && voltage_ok;
        double watts = 0.0;
        bool changed = false;
        if (power_ok) {
            watts = std::fabs(static_cast<double>(current_ua)) * 1e-6 *
                    static_cast<double>(voltage_uv) * 1e-6;
            if (!have_previous_current || current_ua != previous_current) {
                previous_current = current_ua;
                have_previous_current = true;
                changed = true;
                stats.update(watts);
            }
        }

        if (log_mode) {
            if (changed) {
                char timestamp[32];
                format_timestamp(timestamp, sizeof timestamp);
                char line[256];
                int written = snprintf(line, sizeof line, "%s,%.3f,%.0f,%.3f,", timestamp,
                                       watts, static_cast<double>(current_ua) / 1000.0,
                                       static_cast<double>(voltage_uv) / 1e6);
                written += snprintf(line + written, sizeof line - written, capacity_ok ? "%lld," : "n/a,", capacity_pct);
                if (fcc_ok)
                    written += snprintf(line + written, sizeof line - written,
                                        "%lld,%lld,", fcc_status.capacity_mah,
                                        fcc_status.valid);
                else
                    written += snprintf(line + written, sizeof line - written,
                                        "n/a,n/a,");
                written += snprintf(line + written, sizeof line - written, "%s\n", status_text);
                write_all(log_fd, line, static_cast<size_t>(written));
            }
        } else {
            double scale = stats.has ? (stats.max > 1.0 ? stats.max : 1.0) : 1.0;
            render_bar(bar, sizeof bar, watts, scale, 24);
            int written = 0;
            written += snprintf(frame + written, sizeof frame - written, "%s", kCursorHome);
            written += snprintf(frame + written, sizeof frame - written, " ptop - live power monitor%s\r\n%s\r\n", kClearEol, kClearEol);
            if (power_ok)
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s: %6.2f W  %s%s\r\n", "Power", watts, bar,
                                    kClearEol);
            else
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s:    n/a%s\r\n", "Power", kClearEol);
            if (current_ok)
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s: %6.0f mA%s\r\n", "Current",
                                    static_cast<double>(current_ua) / 1000.0, kClearEol);
            else
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s:    n/a%s\r\n", "Current", kClearEol);
            if (voltage_ok)
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s: %6.3f V%s\r\n", "Voltage",
                                    static_cast<double>(voltage_uv) / 1e6, kClearEol);
            else
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s:    n/a%s\r\n", "Voltage", kClearEol);
            if (capacity_ok)
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s: %6lld %%%s\r\n", "SoC", capacity_pct,
                                    kClearEol);
            else
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s:    n/a%s\r\n", "SoC", kClearEol);
            if (fcc_ok) {
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s: %6lld mAh%s\r\n", "FCC",
                                    fcc_status.capacity_mah, kClearEol);
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s: %6s%s\r\n", "Learning",
                                    fcc_status.valid ? "yes" : "no", kClearEol);
            } else {
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s:    n/a%s\r\n %-11s:    n/a%s\r\n", "FCC",
                                    kClearEol, "Learning", kClearEol);
            }
            written += snprintf(frame + written, sizeof frame - written,
                                " %-11s: %6s%s\r\n", "Status", status_text, kClearEol);
            if (stats.has) {
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s: %6.2f W%s\r\n", "Avg (EMA)", stats.ema,
                                    kClearEol);
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s: %6.2f / %6.2f W%s\r\n", "Min/Max", stats.min,
                                    stats.max, kClearEol);
            } else {
                written += snprintf(frame + written, sizeof frame - written,
                                    " %-11s:    n/a%s\r\n %-11s:    n/a%s\r\n", "Avg (EMA)",
                                    kClearEol, "Min/Max", kClearEol);
            }
            written += snprintf(frame + written, sizeof frame - written,
                                "%s\r\n SoC = State of Charge   FCC = Full Charge Capacity%s\r\n interval %.2fs - Ctrl-C to quit%s\r\n",
                                kClearEol, kClearEol, interval, kClearEol);
            if (written > static_cast<int>(sizeof frame)) written = sizeof frame;
            write_all(STDOUT_FILENO, frame, static_cast<size_t>(written));
        }

        if (!g_running || (duration > 0.0 && now - start >= duration)) break;
        if (clock_nanosleep(CLOCK_MONOTONIC, 0, &period, nullptr) == EINTR) break;
    }

    if (!log_mode) restore_terminal();
    if (log_fd >= 0) close(log_fd);
    close(directory_fd);
    return 0;
}