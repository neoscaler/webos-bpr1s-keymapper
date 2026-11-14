#ifndef POINTER_COMMON_H
#define POINTER_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef LUNA_SEND
#define LUNA_SEND "/usr/bin/luna-send"
#endif

static int resolve_pointer_socket(char *out, size_t outsz) {
#ifdef POINTER_SOCK_PATH
    snprintf(out, outsz, "%s", POINTER_SOCK_PATH);
    return 0;
#else
    FILE *p = popen(LUNA_SEND " -n 1 luna://com.webos.service.networkinput/getPointerInputSocket {}", "r");
    if (!p)
        return -1;
    char buf[1024];
    size_t n = fread(buf, 1, sizeof buf - 1, p);
    pclose(p);
    buf[n] = 0;
    char *k = strstr(buf, "\"socketPath\"");
    if (!k)
        return -1;
    char *v = strchr(k + 11, '"');
    if (!v)
        return -1;
    v++;
    char *e = strchr(v, '"');
    if (!e || (size_t)(e - v) >= outsz)
        return -1;
    memcpy(out, v, (size_t)(e - v));
    out[e - v] = 0;
    return 0;
#endif
}

static int pointer_connect(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof addr.sun_path, "%s", path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int pointer_send_button(int fd, const char *name) {
    char buf[64];
    int n = snprintf(buf, sizeof buf, "type:button\nname:%s\n\n", name);
    if (n <= 0 || (size_t)n >= sizeof buf)
        return -1;
    return write(fd, buf, (size_t)n) == n ? 0 : -1;
}

#endif
