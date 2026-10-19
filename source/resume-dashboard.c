/* Steam Frame Clean Resume: one immediate, guarded close per suspend/resume.
 * No network requests, polling, inhibitors, settings edits or shell execution.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>
#include <time.h>
#include <unistd.h>

extern char **environ;
#define VERSION "native-0.2.0"
#define VR_COMMAND "/opt/steamvr/bin/linuxarm64/vrcmd"
#define MAX_LOG_UPDATE 131072U

struct helper {
    const char *log_path;
    int checkpoint;
    bool dry_run;
    int (*close_action)(void);
};

static void disarm(struct helper *h) {
    if (h->checkpoint >= 0)
        close(h->checkpoint);
    h->checkpoint = -1;
}

static bool same_log(int fd, const char *path, off_t minimum_size) {
    struct stat opened, named;
    return fstat(fd, &opened) == 0 && lstat(path, &named) == 0 &&
        S_ISREG(opened.st_mode) && S_ISREG(named.st_mode) &&
        opened.st_dev == named.st_dev && opened.st_ino == named.st_ino &&
        opened.st_size >= minimum_size;
}

/* Bounded streaming scan, including markers split across reads. Keep only
 * the last 63 bytes between reads; no permanent log buffer or file watcher.
 * A partial marker line is conservatively treated as dashboard use too.
 */
static bool dashboard_untouched(int fd, const char *path) {
    static const char toggle[] = "[Dashboard] [ToggleDashboard]";
    static const char hide[] = "[Dashboard] [HideDashboard]";
    off_t offset = lseek(fd, 0, SEEK_CUR);
    if (offset < 0 || !same_log(fd, path, offset))
        return false;
    char buffer[4096 + 64];
    size_t carry = 0, total = 0;
    for (;;) {
        size_t remaining = MAX_LOG_UPDATE + 1U - total;
        size_t count = remaining < 4096 ? remaining : 4096;
        ssize_t n = read(fd, buffer + carry, count);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
            return false;
        if (n == 0)
            break;
        total += (size_t)n;
        if (total > MAX_LOG_UPDATE)
            return false;
        size_t length = carry + (size_t)n;
        if (memmem(buffer, length, toggle, sizeof(toggle) - 1) ||
            memmem(buffer, length, hide, sizeof(hide) - 1))
            return false;
        carry = length < 63 ? length : 63;
        memmove(buffer, buffer + length - carry, carry);
    }
    return same_log(fd, path, offset + (off_t)total);
}

static int64_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/* Wait on the exact child process, with a deadline, only while it is running.
 * pidfd polling blocks in the kernel; this is not a periodic background check.
 */
static int run_bounded(const char *path, char *const argv[], int timeout_ms) {
    posix_spawn_file_actions_t actions;
    int error = posix_spawn_file_actions_init(&actions);
    if (error)
        return -error;
    error = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO,
                                           "/dev/null", O_WRONLY, 0);
    if (!error)
        error = posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    pid_t child = -1;
    if (!error)
        error = posix_spawn(&child, path, &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (error)
        return -error;

    int pidfd = (int)syscall(SYS_pidfd_open, child, 0);
    int result = pidfd < 0 ? -errno : 0;
    int64_t started = monotonic_ms();
    if (started < 0 && result == 0)
        result = -EIO;
    while (result == 0) {
        int64_t now = monotonic_ms();
        if (now < 0) {
            result = -EIO;
            break;
        }
        int64_t remaining = started + timeout_ms - now;
        if (remaining <= 0) {
            result = -ETIMEDOUT;
            break;
        }
        struct pollfd watched = { .fd = pidfd, .events = POLLIN };
        int n = poll(&watched, 1, remaining > INT_MAX ? INT_MAX : (int)remaining);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            result = -errno;
            break;
        }
        if (n == 0) {
            result = -ETIMEDOUT;
            break;
        }
        if (watched.revents & POLLIN)
            break;
        result = -EIO;
    }
    if (result < 0)
        /* An unreaped child PID cannot be reused for an unrelated process. */
        kill(child, SIGKILL);
    if (pidfd >= 0)
        close(pidfd);
    int status = 0;
    pid_t reaped;
    do {
        reaped = waitpid(child, &status, 0);
    } while (reaped < 0 && errno == EINTR);
    if (result < 0)
        return result;
    if (reaped < 0)
        return -errno;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -ECHILD;
}

static int close_dashboard(void) {
    char *const args[] = { VR_COMMAND, "--background", "--hidedashboard", NULL };
    return run_bounded(VR_COMMAND, args, 3000);
}

static void sleep_changed(struct helper *h, bool sleeping) {
    if (sleeping) {
        disarm(h);
        int fd = open(h->log_path, O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW);
        struct stat st;
        if (fd < 0 || fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) ||
            lseek(fd, 0, SEEK_END) < 0) {
            if (fd >= 0)
                close(fd);
            puts("Skipping this cycle: SteamVR log unavailable.");
            return;
        }
        h->checkpoint = fd;
        return;
    }
    /* Starting/restarting the service, screen-off and duplicate resume signals
     * never close the dashboard. Disarm before attempting any close request.
     */
    if (h->checkpoint < 0)
        return;
    int fd = h->checkpoint;
    h->checkpoint = -1;
    bool clear = dashboard_untouched(fd, h->log_path);
    close(fd);
    if (!clear) {
        puts("Skipped: dashboard was used, or SteamVR log changed unexpectedly.");
        return;
    }
    if (h->dry_run) {
        puts("Dry run: resume accepted; no dashboard command sent.");
        return;
    }
    puts("Resume detected; sending one dashboard-close request immediately.");
    int result = h->close_action();
    if (result == 0)
        puts("Close request completed. Idle until the next suspend/resume.");
    else
        printf("Close request failed (%d); no retry.\n", result);
}

static int bus_signal(sd_bus_message *message, void *userdata, sd_bus_error *error) {
    (void)error;
    if (strcmp(sd_bus_message_get_signature(message, true), "b") != 0)
        return 0;
    int sleeping;
    if (sd_bus_message_read(message, "b", &sleeping) > 0)
        sleep_changed(userdata, sleeping != 0);
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    bool dry_run = argc == 2 && strcmp(argv[1], "--dry-run") == 0;
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts("Steam Frame Clean Resume " VERSION);
        return 0;
    }
    if (argc != 1 && !dry_run) {
        fputs("Usage: resume-dashboard [--dry-run|--version]\n", stderr);
        return 2;
    }
    const char *user_home = getenv("HOME");
    char *log_path = NULL;
    if (!user_home || user_home[0] != '/' ||
        asprintf(&log_path, "%s/.local/share/Steam/logs/vrwebhelper_systemui.txt", user_home) < 0) {
        fputs("Cannot locate the user's SteamVR log.\n", stderr);
        return 1;
    }
    struct helper h = { .log_path = log_path, .checkpoint = -1,
                       .dry_run = dry_run, .close_action = close_dashboard };
    sd_bus *bus = NULL;
    sd_bus_slot *slot = NULL;
    int result = sd_bus_open_system(&bus);
    if (result < 0)
        goto done;
    result = sd_bus_match_signal(bus, &slot,
        "org.freedesktop.login1", "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager", "PrepareForSleep", bus_signal, &h);
    if (result < 0)
        goto done;
    puts("Ready: native helper waiting for suspend/resume; no polling or inhibitor.");
    (void)sd_notify(0, "READY=1\nSTATUS=Waiting for suspend/resume");
    for (;;) {
        result = sd_bus_process(bus, NULL);
        if (result < 0)
            break;
        if (result > 0)
            continue;
        result = sd_bus_wait(bus, UINT64_MAX);
        if (result < 0 && result != -EINTR)
            break;
    }
done:
    if (result < 0)
        fprintf(stderr, "System bus error: %s\n", strerror(-result));
    disarm(&h);
    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);
    free(log_path);
    return result < 0;
}
