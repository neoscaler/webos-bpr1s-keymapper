#include "pointer_common.h"

int main(void) {
    char path[108];
    if (resolve_pointer_socket(path, sizeof path) != 0) {
        fprintf(stderr, "FAIL: getPointerInputSocket lieferte keinen socketPath\n");
        return 1;
    }
    fprintf(stderr, "socketPath: %s\n", path);
    int fd = pointer_connect(path);
    if (fd < 0) {
        perror("connect");
        return 1;
    }
    fprintf(stderr, "verbunden, 5 s warten (Menueeintrag mit BPR1S-Pfeilen fokussieren)...\n");
    sleep(5);
    if (pointer_send_button(fd, "ENTER") < 0)
        fprintf(stderr, "FAIL: send ENTER\n");
    else
        fprintf(stderr, "ENTER gesendet\n");
    close(fd);
    return 0;
}
