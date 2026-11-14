#include "uinput_common.h"

int main(void) {
    int fd = uinput_create_keyboard();
    if (fd < 0) {
        fprintf(stderr, "FAIL: cannot create uinput device: %s\n", strerror(errno));
        return 1;
    }
    fprintf(stderr, "uinput keyboard created, waiting 5 s (focus a menu entry with the BPR1S arrows)...\n");
    sleep(5);
    emit_event32(fd, EV_KEY, KEY_ENTER, 1);
    emit_event32(fd, EV_SYN, SYN_REPORT, 0);
    usleep(50000);
    emit_event32(fd, EV_KEY, KEY_ENTER, 0);
    emit_event32(fd, EV_SYN, SYN_REPORT, 0);
    fprintf(stderr, "sent KEY_ENTER press+release, destroying device\n");
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    return 0;
}
