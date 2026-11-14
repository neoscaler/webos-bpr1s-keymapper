#ifndef UINPUT_COMMON_H
#define UINPUT_COMMON_H

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define EV_SYN 0x00u
#define EV_KEY 0x01u
#define SYN_REPORT 0u

#define KEY_ENTER 28u
#define KEY_BACK 158u
#define KEY_HOMEPAGE 172u
#define KEY_SELECT 353u

#define USB_VENDOR 0x12ea
#define USB_PRODUCT 0x0001

#define UI_SET_EVBIT 0x40045564u
#define UI_SET_KEYBIT 0x40045565u
#define UI_DEV_CREATE 0x5501u
#define UI_DEV_DESTROY 0x5502u
#define BUS_USB 0x03u
#define UINPUT_MAX_NAME_SIZE 80
#define ABS_CNT 0x40

struct input_event32 {
    int32_t tv_sec;
    int32_t tv_usec;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

struct input_id32 {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
};

struct uinput_user_dev32 {
    struct input_id32 id;
    char name[UINPUT_MAX_NAME_SIZE];
    uint32_t ff_effects_max;
    int32_t absmax[ABS_CNT];
    int32_t absmin[ABS_CNT];
    int32_t absfuzz[ABS_CNT];
    int32_t absflat[ABS_CNT];
};

_Static_assert(sizeof(struct input_event32) == 16, "compat input_event size");
_Static_assert(sizeof(struct uinput_user_dev32) == 1116, "uinput_user_dev size");

static int emit_event32(int fd, uint16_t type, uint16_t code, int32_t value) {
    struct input_event32 ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type;
    ev.code = code;
    ev.value = value;
    return write(fd, &ev, sizeof ev) == (ssize_t)sizeof ev ? 0 : -1;
}

static int uinput_create_keyboard(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        fd = open("/dev/misc/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return -1;
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
        goto fail;
    if (ioctl(fd, UI_SET_KEYBIT, KEY_ENTER) < 0)
        goto fail;
    if (ioctl(fd, UI_SET_KEYBIT, KEY_BACK) < 0)
        goto fail;
    struct uinput_user_dev32 dev;
    memset(&dev, 0, sizeof dev);
    snprintf(dev.name, sizeof dev.name, "bpr1s-keymapper");
    dev.id.bustype = BUS_USB;
    dev.id.vendor = USB_VENDOR;
    dev.id.product = USB_PRODUCT;
    dev.id.version = 1;
    if (write(fd, &dev, sizeof dev) != (ssize_t)sizeof dev)
        goto fail;
    if (ioctl(fd, UI_DEV_CREATE) < 0)
        goto fail;
    return fd;
fail:
    close(fd);
    return -1;
}

#endif
