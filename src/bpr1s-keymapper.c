#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

#include "pointer_common.h"

#ifndef PROC_DEVICES_PATH
#define PROC_DEVICES_PATH "/proc/bus/input/devices"
#endif
#ifndef SYSFS_INPUT_DIR
#define SYSFS_INPUT_DIR "/sys/class/input"
#endif
#ifndef INPUT_DEV_PREFIX
#define INPUT_DEV_PREFIX "/dev/input/"
#endif

#define MAX_DEVICES 8
#define RESCAN_SLEEP_S 10
#define RETRY_SLEEP_S 2

#define EV_KEY 0x01u
#define KEY_ENTER 28u
#define KEY_BACK 158u
#define KEY_HOMEPAGE 172u
#define KEY_SELECT 353u

struct input_event32 {
    int32_t tv_sec;
    int32_t tv_usec;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

_Static_assert(sizeof(struct input_event32) == 16, "compat input_event size");

enum action { ACT_NONE = 0, ACT_HOME, ACT_BUTTON };

struct key_mapping {
    uint16_t code;
    enum action action;
    const char *button;
};

static const struct key_mapping mappings[] = {
    { KEY_HOMEPAGE, ACT_HOME, NULL },
    { KEY_SELECT, ACT_BUTTON, "ENTER" },
    { KEY_BACK, ACT_BUTTON, "BACK" },
};

static int verbose;
static int ptr_fd = -1;
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
        execl(LUNA_SEND, "luna-send", "-n", "1",
              "luna://com.webos.applicationManager/launch",
              "{\"id\":\"com.webos.app.home\"}", (char *)0);
        _exit(127);
    }
}

static void pointer_reconnect(void) {
    if (ptr_fd >= 0)
        close(ptr_fd);
    ptr_fd = -1;
    char path[108];
    if (resolve_pointer_socket(path, sizeof path) != 0) {
        logmsg("pointer socket: kein socketPath");
        return;
    }
    ptr_fd = pointer_connect(path);
    logmsg("pointer socket %s %s", path, ptr_fd >= 0 ? "verbunden" : "FEHLER");
}

static void action_button(const char *name) {
    if (ptr_fd < 0)
        pointer_reconnect();
    if (ptr_fd >= 0 && pointer_send_button(ptr_fd, name) < 0) {
        pointer_reconnect();
        if (ptr_fd >= 0)
            pointer_send_button(ptr_fd, name);
    }
    logmsg("button %s gesendet", name);
}

static void handle_event(const struct input_event32 *ev) {
    if (ev->type != EV_KEY || ev->value != 1)
        return;
    for (size_t i = 0; i < sizeof mappings / sizeof mappings[0]; i++) {
        if (mappings[i].code != ev->code)
            continue;
        switch (mappings[i].action) {
        case ACT_HOME:
            logmsg("KEY_HOMEPAGE -> launch com.webos.app.home");
            action_home();
            break;
        case ACT_BUTTON:
            action_button(mappings[i].button);
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

static int read_small_file(const char *path, char *buf, size_t bufsz) {
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    size_t n = fread(buf, 1, bufsz - 1, f);
    fclose(f);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' ||
                     buf[n - 1] == ' ' || buf[n - 1] == '\t'))
        n--;
    buf[n] = 0;
    return 0;
}

static int device_matches(const char *name, const char *vendor, const char *product) {
    if (name && strcasestr(name, "bpr1s-keymapper"))
        return 0;
    int vp = vendor && product &&
             (!strcasecmp(vendor, "12ea") || !strcasecmp(vendor, "4842")) &&
             (!strcasecmp(product, "0001") || !strcasecmp(product, "1"));
    int nm = name && (strcasestr(name, "2.4g usb") || strcasestr(name, "xing wei"));
    return vp || nm;
}

static void open_event_handler(const char *event_name, int *fds, int *count, int max) {
    if (*count >= max)
        return;
    char path[288];
    snprintf(path, sizeof path, INPUT_DEV_PREFIX "%s", event_name);
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
        fds[(*count)++] = fd;
        logmsg("opened %s", path);
    }
}

static int open_dongle_devices(int *fds, int max) {
    FILE *f = fopen(PROC_DEVICES_PATH, "r");
    if (!f) {
        logmsg("cannot open %s", PROC_DEVICES_PATH);
        return 0;
    }
    char line[512];
    char name[512] = "";
    char vendor[16] = "";
    char product[16] = "";
    int count = 0;
    int blocks = 0;
    int in_target = 0;
    int handlers_seen = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '\n') {
            in_target = 0;
            handlers_seen = 0;
            name[0] = vendor[0] = product[0] = 0;
            continue;
        }
        if (!strncmp(line, "I: ", 3)) {
            blocks++;
            const char *v = strcasestr(line, "vendor=");
            const char *p = strcasestr(line, "product=");
            if (v)
                snprintf(vendor, sizeof vendor, "%.4s", v + 7);
            if (p)
                snprintf(product, sizeof product, "%.4s", p + 8);
            in_target = device_matches(NULL, vendor, product);
            handlers_seen = 0;
            continue;
        }
        if (!strncmp(line, "N: Name=", 8)) {
            snprintf(name, sizeof name, "%s", line + 8);
            in_target = device_matches(name, vendor, product);
            continue;
        }
        if (!in_target || handlers_seen)
            continue;
        if (!strncmp(line, "H: Handlers=", 12)) {
            handlers_seen = 1;
            char *tok = strtok(line + 12, " \t\n");
            while (tok) {
                if (!strncmp(tok, "event", 5))
                    open_event_handler(tok, fds, &count, max);
                tok = strtok(NULL, " \t\n");
            }
        }
    }
    fclose(f);
    logmsg("proc scan: %d device blocks, %d handlers", blocks, count);
    return count;
}

static int open_dongle_devices_sysfs(int *fds, int max) {
    DIR *d = opendir(SYSFS_INPUT_DIR);
    if (!d)
        return 0;
    struct dirent *de;
    int count = 0;
    while ((de = readdir(d)) && count < max) {
        if (strncmp(de->d_name, "event", 5))
            continue;
        char base[320];
        char name[256] = "";
        char vendor[16] = "";
        char product[16] = "";
        char p[384];
        snprintf(base, sizeof base, SYSFS_INPUT_DIR "/%s/device", de->d_name);
        snprintf(p, sizeof p, "%s/name", base);
        read_small_file(p, name, sizeof name);
        snprintf(p, sizeof p, "%s/id/vendor", base);
        read_small_file(p, vendor, sizeof vendor);
        snprintf(p, sizeof p, "%s/id/product", base);
        read_small_file(p, product, sizeof product);
        if (!device_matches(name, vendor, product))
            continue;
        open_event_handler(de->d_name, fds, &count, max);
    }
    closedir(d);
    logmsg("sysfs scan: %d handlers", count);
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

    pointer_reconnect();

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0)
        return 1;

    int fds[MAX_DEVICES];
    for (;;) {
        if (stop_requested)
            break;
        int n = open_dongle_devices(fds, MAX_DEVICES);
        if (n == 0)
            n = open_dongle_devices_sysfs(fds, MAX_DEVICES);
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
        if (dead && !stop_requested) {
            logmsg("dongle lost, rescanning in %ds", RETRY_SLEEP_S);
            sleep(RETRY_SLEEP_S);
        }
    }

    if (ptr_fd >= 0)
        close(ptr_fd);
    close(epfd);
    return 0;
}
