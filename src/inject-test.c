#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define EV_SYN 0x00u
#define EV_KEY 0x01u

struct input_event32 {
    int32_t tv_sec;
    int32_t tv_usec;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

static int find_kbd_node(char *path, size_t pathsz) {
    FILE *f = fopen("/proc/bus/input/devices", "r");
    if (!f)
        return -1;
    char line[512];
    int target = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '\n') {
            target = 0;
            continue;
        }
        if (!strncmp(line, "I: ", 3)) {
            target = strcasestr(line, "vendor=4842") || strcasestr(line, "vendor=12ea");
            continue;
        }
        if (target && !strncmp(line, "H: Handlers=", 12) && strstr(line, "leds")) {
            char *tok = strtok(line + 12, " \t\n");
            while (tok) {
                if (!strncmp(tok, "event", 5)) {
                    snprintf(path, pathsz, "/dev/input/%s", tok);
                    fclose(f);
                    return 0;
                }
                tok = strtok(NULL, " \t\n");
            }
        }
    }
    fclose(f);
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <keycode-dezimal>\n", argv[0]);
        return 1;
    }
    unsigned int code = (unsigned int)strtoul(argv[1], NULL, 0);
    char path[64];
    if (find_kbd_node(path, sizeof path) != 0) {
        fprintf(stderr, "FAIL: Dongle-Tastatur-Node nicht gefunden\n");
        return 1;
    }
    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    fprintf(stderr, "sende key 0x%x auf %s ...\n", code, path);
    struct input_event32 ev[2];
    memset(ev, 0, sizeof ev);
    ev[0].type = EV_KEY;
    ev[0].code = (uint16_t)code;
    ev[0].value = 1;
    ev[1].type = EV_SYN;
    if (write(fd, ev, sizeof ev) != (ssize_t)sizeof ev) {
        perror("write press");
        return 1;
    }
    usleep(50000);
    ev[0].value = 0;
    if (write(fd, ev, sizeof ev) != (ssize_t)sizeof ev) {
        perror("write release");
        return 1;
    }
    fprintf(stderr, "fertig (press+release)\n");
    close(fd);
    return 0;
}
