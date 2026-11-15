#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "keymap.h"

#ifndef PROC_DEVICES_PATH
#define PROC_DEVICES_PATH "/proc/bus/input/devices"
#endif
#ifndef SYSFS_INPUT_DIR
#define SYSFS_INPUT_DIR "/sys/class/input"
#endif
#ifndef INPUT_DEV_PREFIX
#define INPUT_DEV_PREFIX "/dev/input/"
#endif
#ifndef LUNA_SEND
#define LUNA_SEND "/usr/bin/luna-send"
#endif
#ifndef APP_STATUS_PATH
#define APP_STATUS_PATH "/media/developer/apps/usr/palm/applications/com.neoscaler.bpr1s-keymapper/status.json"
#endif
#ifndef KEYMAPPER_VERSION
#define KEYMAPPER_VERSION "dev"
#endif
#ifndef KEYMAP_PATH
#define KEYMAP_PATH "/var/lib/webosbrew/bpr1s-keymapper/keymap.conf"
#endif

#define MAX_DEVICES 8
#define MAX_EVENTS (MAX_DEVICES + 1)
#define RESCAN_SLEEP_S 10
#define RETRY_SLEEP_S 2
#define HEARTBEAT_S 30

#define EV_SYN 0x00u
#define EV_KEY 0x01u
#define SYN_REPORT 0u
#define KEY_ENTER 28u
#define KEY_BACK 158u
#define KEY_HOMEPAGE 172u
#define KEY_SELECT 353u
#define KEY_COMPOSE 127u
#define KEY_VOICECOMMAND 582u
#define ECHO_PENDING_MAX 768
#define KEY_MAX 0x2ffu
#define REPEAT_MIN_MS 150

#ifndef EVIOCGBIT
#define EVIOCGBIT(ev, len) _IOC(_IOC_READ, 'E', 0x20 + (ev), len)
#endif

struct input_event32 {
    int32_t tv_sec;
    int32_t tv_usec;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

_Static_assert(sizeof(struct input_event32) == 16, "compat input_event size");

enum action { ACT_NONE = 0, ACT_LAUNCH, ACT_SPECIAL_KEY, ACT_INJECT };

struct key_mapping {
    uint16_t code;
    enum action action;
    uint16_t target;
    const char *arg;
    const char *label;
};

static const struct key_mapping mappings[] = {
    { KEY_SELECT, ACT_INJECT, KEY_ENTER, NULL, "OK" },
    { KEY_BACK, ACT_SPECIAL_KEY, 0, "BACK", "Back" },
};

static const uint16_t cfg_codes[KM_BUTTONS] = {
    KEY_COMPOSE, KEY_HOMEPAGE, KEY_VOICECOMMAND,
};
static const char *const cfg_labels[KM_BUTTONS] = {
    "Square", "Triangle", "Circle",
};

static struct km_entry cfg[KM_BUTTONS];
static long long cfg_last_ms[KM_BUTTONS];
static char st_cfg_error[128] = "";

static int verbose;
static int write_fd = -1;
static uint8_t echo_pending[ECHO_PENDING_MAX];
static uint8_t key_down[ECHO_PENDING_MAX];
static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t reload_requested;

static int st_dongle;
static int st_running = 1;
static unsigned long st_events;
static char st_target[288] = "";
static char st_lastkey[48] = "";
static char st_path[288];
static const char *status_path = APP_STATUS_PATH;

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void write_status(int force) {
    static long long last_ms;
    static char last_json[1024];
    long long now = now_ms();
    if (!force && now - last_ms < 2000)
        return;
    char m0[KM_ARG_MAX + 16], m1[KM_ARG_MAX + 16], m2[KM_ARG_MAX + 16];
    keymap_format(&cfg[0], m0, sizeof m0);
    keymap_format(&cfg[1], m1, sizeof m1);
    keymap_format(&cfg[2], m2, sizeof m2);
    char json[1024];
    int len = snprintf(json, sizeof json,
                       "{\"version\":\"%s\",\"running\":%s,\"updatedAt\":%lld,\"pid\":%d,"
                       "\"dongle\":%s,\"writeTarget\":\"%s\",\"events\":%lu,\"lastKey\":\"%s\","
                       "\"mapping\":{\"square\":\"%s\",\"triangle\":\"%s\",\"circle\":\"%s\"},"
                       "\"configError\":\"%s\"}",
                       KEYMAPPER_VERSION, st_running ? "true" : "false",
                       (long long)time(NULL), (int)getpid(),
                       st_dongle ? "true" : "false", st_target,
                       st_events, st_lastkey, m0, m1, m2, st_cfg_error);
    if (len < 0 || len >= (int)sizeof json)
        return;
    if (!strcmp(json, last_json))
        return;
    char tmp[320];
    snprintf(tmp, sizeof tmp, "%s.tmp", status_path);
    FILE *f = fopen(tmp, "w");
    if (!f)
        return;
    fprintf(f, "%s\n", json);
    if (fclose(f) == 0 && rename(tmp, status_path) == 0) {
        snprintf(last_json, sizeof last_json, "%s", json);
        last_ms = now;
    }
}

static void resolve_status_path(void) {
    const char *env = getenv("BPR1S_STATUS_PATH");
    if (env && *env) {
        status_path = env;
        return;
    }
    char exe[288];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0)
        return;
    exe[n] = 0;
    /* <appdir>/bin/bpr1s-keymapper -> <appdir> */
    for (int i = 0; i < 2; i++) {
        char *slash = strrchr(exe, '/');
        if (!slash)
            return;
        *slash = 0;
    }
    /* Only an app directory gets a status file; a binary run from a build tree
       must not drop one next to the sources. */
    char probe[300];
    if (snprintf(probe, sizeof probe, "%s/appinfo.json", exe) >= (int)sizeof probe)
        return;
    if (access(probe, R_OK) != 0)
        return;
    if (snprintf(st_path, sizeof st_path, "%s/status.json", exe) >= (int)sizeof st_path)
        return;
    status_path = st_path;
}

static void logmsg(const char *fmt, ...) {
    if (!verbose)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static void load_config(void) {
    char err[128] = "";
    int bad = keymap_load(KEYMAP_PATH, cfg, err, sizeof err);
    if (bad < 0)
        snprintf(err, sizeof err, "config unreadable");
    snprintf(st_cfg_error, sizeof st_cfg_error, "%s", err);
    if (err[0])
        logmsg("config: %s", err);
    for (int i = 0; i < KM_BUTTONS; i++) {
        char text[KM_ARG_MAX + 16];
        keymap_format(&cfg[i], text, sizeof text);
        logmsg("%s: %s", cfg_labels[i], text);
    }
}

static void on_signal(int sig) {
    (void)sig;
    stop_requested = 1;
}

static void on_reload(int sig) {
    (void)sig;
    reload_requested = 1;
}

static void maybe_reload(void) {
    if (!reload_requested)
        return;
    reload_requested = 0;
    load_config();
    write_status(1);
}

static void action_luna(const char *uri, const char *payload) {
    pid_t pid = fork();
    if (pid < 0)
        return;
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execl(LUNA_SEND, "luna-send", "-n", "1", uri, payload, (char *)0);
        _exit(127);
    }
}

static void action_launch(const char *app) {
    char payload[256];
    snprintf(payload, sizeof payload, "{\"id\":\"%s\"}", app);
    action_luna("luna://com.webos.applicationManager/launch", payload);
}

static void action_special_key(const char *key) {
    char payload[64];
    snprintf(payload, sizeof payload, "{\"key\":\"%s\"}", key);
    action_luna("luna://com.webos.service.networkinput/sendSpecialKey", payload);
}

static void inject_key(uint16_t code, int32_t value) {
    if (write_fd < 0)
        return;
    struct input_event32 ev[2];
    memset(ev, 0, sizeof ev);
    ev[0].type = EV_KEY;
    ev[0].code = code;
    ev[0].value = value;
    ev[1].type = EV_SYN;
    ev[1].code = SYN_REPORT;
    ev[1].value = 0;
    ssize_t n;
    do {
        n = write(write_fd, ev, sizeof ev);
    } while (n < 0 && errno == EINTR);
    if (n == (ssize_t)sizeof ev && code < ECHO_PENDING_MAX) {
        echo_pending[code]++;
        key_down[code] = value != 0;
    }
    logmsg("inject key 0x%x value %d (%s)", code, (int)value,
           n == (ssize_t)sizeof ev ? "ok" : "FEHLER");
}

static void release_stuck_keys(void) {
    for (size_t i = 0; i < sizeof key_down; i++)
        if (key_down[i])
            inject_key((uint16_t)i, 0);
}

static void cycle_next(struct km_entry *entry, uint16_t code) {
    unsigned count = 1;
    for (const char *p = entry->arg; *p; p++)
        if (*p == ',')
            count++;
    if (entry->idx >= count)
        entry->idx = 0;
    const char *start = entry->arg;
    for (unsigned n = 0; n < entry->idx; n++)
        start = strchr(start, ',') + 1;
    char id[KM_ARG_MAX];
    size_t len = strcspn(start, ",");
    if (len >= sizeof id)
        len = sizeof id - 1;
    memcpy(id, start, len);
    id[len] = 0;
    logmsg("key 0x%x -> cycle %s (%u/%u)", code, id, entry->idx + 1, count);
    action_launch(id);
    entry->idx = (entry->idx + 1) % count;
}

static void run_config_action(int i, const struct input_event32 *ev) {
    struct km_entry *entry = &cfg[i];
    if (entry->action == KM_NONE || ev->value == 0)
        return;
    long long now = now_ms();
    if (ev->value == 2) {
        if (entry->action != KM_KEY || now - cfg_last_ms[i] < REPEAT_MIN_MS)
            return;
    }
    cfg_last_ms[i] = now;
    st_events++;
    snprintf(st_lastkey, sizeof st_lastkey, "%s (0x%x)", cfg_labels[i], ev->code);
    write_status(0);
    switch (entry->action) {
    case KM_LAUNCH:
        logmsg("key 0x%x -> launch %s", ev->code, entry->arg);
        action_launch(entry->arg);
        break;
    case KM_KEY:
        logmsg("key 0x%x -> special key %s", ev->code, entry->arg);
        action_special_key(entry->arg);
        break;
    case KM_CYCLE:
        cycle_next(entry, ev->code);
        break;
    default:
        break;
    }
}

static void handle_event(int fd, const struct input_event32 *ev) {
    if (ev->type != EV_KEY)
        return;
    if (fd == write_fd && ev->code < ECHO_PENDING_MAX && echo_pending[ev->code] > 0) {
        echo_pending[ev->code]--;
        return;
    }
    for (int i = 0; i < KM_BUTTONS; i++) {
        if (cfg_codes[i] != ev->code)
            continue;
        run_config_action(i, ev);
        return;
    }
    for (size_t i = 0; i < sizeof mappings / sizeof mappings[0]; i++) {
        if (mappings[i].code != ev->code)
            continue;
        st_events++;
        snprintf(st_lastkey, sizeof st_lastkey, "%s (0x%x)", mappings[i].label, ev->code);
        write_status(0);
        switch (mappings[i].action) {
        case ACT_LAUNCH:
            if (ev->value == 1) {
                logmsg("key 0x%x -> launch %s", ev->code, mappings[i].arg);
                action_launch(mappings[i].arg);
            }
            break;
        case ACT_SPECIAL_KEY:
            if (ev->value == 1) {
                logmsg("key 0x%x -> special key %s", ev->code, mappings[i].arg);
                action_special_key(mappings[i].arg);
            }
            break;
        case ACT_INJECT:
            if (ev->value == 0 || ev->value == 1 || ev->value == 2)
                inject_key(mappings[i].target, ev->value);
            break;
        default:
            break;
        }
        return;
    }
    if (verbose)
        logmsg("unmapped key 0x%x value %d", ev->code, (int)ev->value);
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
        if (n == 0 || n % (ssize_t)sizeof ev[0] != 0) {
            /* 16-byte assumption (32-bit kernel) violated: wrong binary variant,
               a rescan cannot fix that. */
            logmsg("unexpected event size %zd, expected 16 bytes", n);
            exit(1);
        }
        ssize_t count = n / (ssize_t)sizeof ev[0];
        for (ssize_t i = 0; i < count; i++)
            handle_event(fd, &ev[i]);
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
    if (vendor && *vendor && product && *product) {
        int v = !strcasecmp(vendor, "12ea") || !strcasecmp(vendor, "4842");
        int p = !strcasecmp(product, "0001") || !strcasecmp(product, "1");
        return v && p;
    }
    return name && (strcasestr(name, "xing wei") || strcasestr(name, "2.4g usb"));
}

static int event_device_has_mapped_key(int fd) {
    uint8_t evbits[8];
    memset(evbits, 0, sizeof evbits);
    if (ioctl(fd, EVIOCGBIT(0, sizeof evbits), evbits) < 0)
        return 1;
    if (!(evbits[EV_KEY / 8] & (1u << (EV_KEY % 8))))
        return 0;
    uint8_t keybits[(KEY_MAX + 8) / 8];
    memset(keybits, 0, sizeof keybits);
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keybits), keybits) < 0)
        return 1;
    for (size_t i = 0; i < sizeof mappings / sizeof mappings[0]; i++) {
        uint16_t code = mappings[i].code;
        if (keybits[code / 8] & (1u << (code % 8)))
            return 1;
    }
    for (int i = 0; i < KM_BUTTONS; i++) {
        uint16_t code = cfg_codes[i];
        if (keybits[code / 8] & (1u << (code % 8)))
            return 1;
    }
    return 0;
}

static void open_event_handler(const char *event_name, int writable, int *fds, int *count, int max) {
    if (*count >= max)
        return;
    char path[288];
    snprintf(path, sizeof path, INPUT_DEV_PREFIX "%s", event_name);
    int fd = open(path, (writable ? O_RDWR : O_RDONLY) | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return;
    if (!writable && !event_device_has_mapped_key(fd)) {
        logmsg("skipped %s (no mapped keys, pointer/gyro interface)", path);
        close(fd);
        return;
    }
    fds[(*count)++] = fd;
    if (writable && write_fd < 0) {
        write_fd = fd;
        snprintf(st_target, sizeof st_target, "%s", path);
    }
    logmsg("opened %s%s", path, writable ? " (O_RDWR, write target)" : "");
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
            int kbd = strstr(line, "leds") != NULL;
            char *tok = strtok(line + 12, " \t\n");
            while (tok) {
                if (!strncmp(tok, "event", 5))
                    open_event_handler(tok, kbd, fds, &count, max);
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
        /* sysfs exposes no leds handler; the write target is only set by the proc scan. */
        open_event_handler(de->d_name, 0, fds, &count, max);
    }
    closedir(d);
    logmsg("sysfs scan: %d handlers", count);
    return count;
}

int main(int argc, char **argv) {
    verbose = argc > 1 && !strcmp(argv[1], "--verbose");
    if (argc > 1 && !strcmp(argv[1], "--version")) {
        printf("bpr1s-keymapper %s\n", KEYMAPPER_VERSION);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--selftest"))
        return 0;

    int lock_fd = open("/tmp/bpr1s-keymapper.lock",
                       O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (lock_fd < 0) {
        logmsg("lock file unusable: %s", strerror(errno));
        return 1;
    }
    struct stat lst;
    if (fstat(lock_fd, &lst) < 0 || !S_ISREG(lst.st_mode) || lst.st_uid != 0) {
        logmsg("lock file is not a regular file owned by root");
        return 1;
    }
    if (flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
        logmsg("another instance is already running (flock), exiting");
        return 0;
    }

    signal(SIGCHLD, SIG_IGN);
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_reload;
    sigaction(SIGHUP, &sa, NULL);

    setpriority(PRIO_PROCESS, 0, 19);

    resolve_status_path();
    load_config();
    write_status(1);

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0)
        return 1;

    int hb_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (hb_fd >= 0) {
        struct itimerspec its;
        memset(&its, 0, sizeof its);
        its.it_interval.tv_sec = HEARTBEAT_S;
        its.it_value.tv_sec = HEARTBEAT_S;
        timerfd_settime(hb_fd, 0, &its, NULL);
        struct epoll_event ee;
        memset(&ee, 0, sizeof ee);
        ee.events = EPOLLIN;
        ee.data.fd = hb_fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, hb_fd, &ee);
    }

    int fds[MAX_DEVICES];
    for (;;) {
        if (stop_requested)
            break;
        maybe_reload();
        write_fd = -1;
        memset(echo_pending, 0, sizeof echo_pending);
        memset(key_down, 0, sizeof key_down);
        int n = open_dongle_devices(fds, MAX_DEVICES);
        if (n == 0)
            n = open_dongle_devices_sysfs(fds, MAX_DEVICES);
        st_dongle = n > 0;
        if (!st_dongle)
            st_target[0] = 0;
        write_status(1);
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
            maybe_reload();
            struct epoll_event events[MAX_EVENTS];
            int nev = epoll_wait(epfd, events, MAX_EVENTS, -1);
            if (nev < 0) {
                if (errno == EINTR)
                    continue;
                dead = 1;
                break;
            }
            for (int i = 0; i < nev && !dead; i++) {
                if (events[i].data.fd == hb_fd) {
                    uint64_t ticks;
                    while (read(hb_fd, &ticks, sizeof ticks) > 0)
                        ;
                    write_status(1);
                    continue;
                }
                if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    dead = 1;
                    break;
                }
                if ((events[i].events & EPOLLIN) && drain_fd(events[i].data.fd) < 0)
                    dead = 1;
            }
        }
        release_stuck_keys();
        for (int i = 0; i < n; i++) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, fds[i], NULL);
            close(fds[i]);
        }
        if (dead && !stop_requested) {
            logmsg("dongle lost, rescanning in %ds", RETRY_SLEEP_S);
            st_dongle = 0;
            st_target[0] = 0;
            write_status(1);
            sleep(RETRY_SLEEP_S);
        }
    }

    st_running = 0;
    st_dongle = 0;
    st_target[0] = 0;
    write_status(1);
    if (hb_fd >= 0)
        close(hb_fd);
    close(epfd);
    return 0;
}
