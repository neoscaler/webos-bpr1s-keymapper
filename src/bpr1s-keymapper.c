#define _GNU_SOURCE
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

#include "uinput_common.h"

#ifndef PROC_DEVICES_PATH
#define PROC_DEVICES_PATH "/proc/bus/input/devices"
#endif
#ifndef INPUT_DEV_PREFIX
#define INPUT_DEV_PREFIX "/dev/input/"
#endif

#define MAX_DEVICES 8
#define RESCAN_SLEEP_S 10
#define RETRY_SLEEP_S 2

enum action { ACT_NONE = 0, ACT_HOME, ACT_INJECT };

struct key_mapping {
    uint16_t code;
    enum action action;
    uint16_t target;
};

static const struct key_mapping mappings[] = {
    { KEY_HOMEPAGE, ACT_HOME, 0 },
    { KEY_SELECT, ACT_INJECT, KEY_ENTER },
    { KEY_BACK, ACT_INJECT, KEY_BACK },
};

static int verbose;
static int uinput_fd = -1;
static volatile sig_atomic_t stop_requested;

static void logmsg(const char *fmt, ...) {
    if (!verbose)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static void on_signal(int sig) {
    (void)sig;
    stop_requested = 1;
}

static void action_home(void) {
    pid_t pid = fork();
    if (pid < 0)
        return;
    if (pid == 0) {
        execl("/usr/bin/luna-send", "luna-send", "-n", "1",
              "luna://com.webos.applicationManager/launch",
              "{\"id\":\"com.webos.app.home\"}", (char *)0);
        _exit(127);
    }
}

static void action_inject(uint16_t code, int32_t value) {
    if (uinput_fd < 0)
        return;
    if (emit_event32(uinput_fd, EV_KEY, code, value) == 0)
        emit_event32(uinput_fd, EV_SYN, SYN_REPORT, 0);
}

static void handle_event(const struct input_event32 *ev) {
    if (ev->type != EV_KEY)
        return;
    for (size_t i = 0; i < sizeof mappings / sizeof mappings[0]; i++) {
        if (mappings[i].code != ev->code)
            continue;
        switch (mappings[i].action) {
        case ACT_HOME:
            if (ev->value == 1) {
                logmsg("KEY_HOMEPAGE press -> launch com.webos.app.home");
                action_home();
            }
            break;
        case ACT_INJECT:
            logmsg("key 0x%x value=%d -> inject 0x%x", ev->code, (int)ev->value, mappings[i].target);
            action_inject(mappings[i].target, ev->value);
            break;
        default:
            break;
        }
        return;
    }
}

static int drain_fd(int fd) {
    struct input_event32 ev[16];
    for (;;) {
        ssize_t n = read(fd, ev, sizeof ev);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return 0;
            return -1;
        }
        if (n == 0 || n % (ssize_t)sizeof ev[0] != 0)
            return -1;
        ssize_t count = n / (ssize_t)sizeof ev[0];
        for (ssize_t i = 0; i < count; i++)
            handle_event(&ev[i]);
    }
}

static int open_dongle_devices(int *fds, int max) {
    FILE *f = fopen(PROC_DEVICES_PATH, "r");
    if (!f)
        return 0;
    char line[512];
    int count = 0;
    int in_target = 0;
    int handlers_seen = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '\n') {
            in_target = 0;
            handlers_seen = 0;
            continue;
        }
        if (!strncmp(line, "I: ", 3)) {
            in_target = strcasestr(line, "vendor=12ea") != NULL &&
                        strcasestr(line, "product=0001") != NULL;
            handlers_seen = 0;
            continue;
        }
        if (!in_target || handlers_seen)
            continue;
        if (!strncmp(line, "H: Handlers=", 12)) {
            handlers_seen = 1;
            char *tok = strtok(line + 12, " \t\n");
            while (tok && count < max) {
                if (!strncmp(tok, "event", 5)) {
                    char path[64];
                    snprintf(path, sizeof path, INPUT_DEV_PREFIX "%s", tok);
                    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                    if (fd >= 0) {
                        fds[count++] = fd;
                        logmsg("opened %s", path);
                    }
                }
                tok = strtok(NULL, " \t\n");
            }
        }
    }
    fclose(f);
    return count;
}

int main(int argc, char **argv) {
    verbose = argc > 1 && !strcmp(argv[1], "--verbose");

    signal(SIGCHLD, SIG_IGN);
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    setpriority(PRIO_PROCESS, 0, 19);

    uinput_fd = uinput_create_keyboard();
    if (uinput_fd < 0)
        logmsg("uinput unavailable (%s), OK mapping disabled", strerror(errno));
    else
        logmsg("virtual uinput keyboard created");

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0)
        return 1;

    int fds[MAX_DEVICES];
    for (;;) {
        if (stop_requested)
            break;
        int n = open_dongle_devices(fds, MAX_DEVICES);
        if (n == 0) {
            logmsg("BPR1S dongle not found, retrying in %ds", RESCAN_SLEEP_S);
            sleep(RESCAN_SLEEP_S);
            continue;
        }
        for (int i = 0; i < n; i++) {
            struct epoll_event ee;
            memset(&ee, 0, sizeof ee);
            ee.events = EPOLLIN;
            ee.data.fd = fds[i];
            epoll_ctl(epfd, EPOLL_CTL_ADD, fds[i], &ee);
        }
        int dead = 0;
        while (!dead && !stop_requested) {
            struct epoll_event events[MAX_DEVICES];
            int nev = epoll_wait(epfd, events, MAX_DEVICES, -1);
            if (nev < 0) {
                if (errno == EINTR)
                    continue;
                dead = 1;
                break;
            }
            for (int i = 0; i < nev && !dead; i++) {
                if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    dead = 1;
                    break;
                }
                if ((events[i].events & EPOLLIN) && drain_fd(events[i].data.fd) < 0)
                    dead = 1;
            }
        }
        for (int i = 0; i < n; i++) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, fds[i], NULL);
            close(fds[i]);
        }
        if (dead)
            logmsg("dongle lost, rescanning in %ds", RETRY_SLEEP_S);
        if (dead && !stop_requested)
            sleep(RETRY_SLEEP_S);
    }

    if (uinput_fd >= 0) {
        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
    }
    close(epfd);
    return 0;
}
