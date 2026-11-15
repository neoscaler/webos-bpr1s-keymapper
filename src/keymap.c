#include "keymap.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define LINE_MAX 512
#define CYCLE_MAX 4
#define APP_ID_MAX 128

static const char *const button_names[KM_BUTTONS] = {
    "square", "triangle", "circle",
};

static const char *const key_names[] = {
    "BACK", "INFO", "MENU", "EXIT", "GUIDE", "CC",
    "RED", "GREEN", "YELLOW", "BLUE",
    "PLAY", "PAUSE", "STOP", "REWIND", "FASTFORWARD",
    "CHANNELUP", "CHANNELDOWN",
    NULL,
};

static void seterr(char *err, size_t errsz, const char *msg) {
    if (err && errsz)
        snprintf(err, errsz, "%s", msg);
}

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s))
        s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        *--end = 0;
    return s;
}

int keymap_valid_app_id(const char *id) {
    size_t n = strlen(id);
    if (n == 0 || n >= APP_ID_MAX)
        return 0;
    for (size_t i = 0; i < n; i++) {
        char c = id[i];
        if (!(isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

int keymap_valid_key(const char *name) {
    for (size_t i = 0; key_names[i]; i++)
        if (!strcmp(key_names[i], name))
            return 1;
    return 0;
}

static int valid_cycle(const char *list) {
    int count = 0;
    const char *p = list;
    while (*p) {
        size_t len = strcspn(p, ",");
        if (len == 0 || len >= APP_ID_MAX)
            return 0;
        char id[APP_ID_MAX];
        memcpy(id, p, len);
        id[len] = 0;
        if (!keymap_valid_app_id(id))
            return 0;
        if (++count > CYCLE_MAX)
            return 0;
        p += len;
        if (*p == ',')
            p++;
    }
    return count >= 1;
}

void keymap_defaults(struct km_entry out[KM_BUTTONS]) {
    memset(out, 0, sizeof(struct km_entry) * KM_BUTTONS);
    out[0].action = KM_CYCLE;
    snprintf(out[0].arg, sizeof out[0].arg,
             "com.webos.app.hdmi1,com.webos.app.hdmi2,com.webos.app.hdmi3,com.webos.app.hdmi4");
    out[1].action = KM_LAUNCH;
    snprintf(out[1].arg, sizeof out[1].arg, "com.webos.app.home");
    out[2].action = KM_LAUNCH;
    snprintf(out[2].arg, sizeof out[2].arg, "com.webos.app.livetv");
}

int keymap_parse_line(const char *line, int *button, struct km_entry *entry,
                      char *err, size_t errsz) {
    char buf[LINE_MAX];
    snprintf(buf, sizeof buf, "%s", line);
    char *s = trim(buf);
    if (!*s || *s == '#')
        return 0;

    char *eq = strchr(s, '=');
    if (!eq) {
        seterr(err, errsz, "missing '='");
        return -1;
    }
    *eq = 0;
    char *name = trim(s);
    char *value = trim(eq + 1);

    int idx = -1;
    for (int i = 0; i < KM_BUTTONS; i++)
        if (!strcmp(button_names[i], name))
            idx = i;
    if (idx < 0)
        return 0;

    memset(entry, 0, sizeof *entry);
    if (!strcmp(value, "none")) {
        entry->action = KM_NONE;
    } else if (!strncmp(value, "launch:", 7)) {
        const char *id = value + 7;
        if (!keymap_valid_app_id(id)) {
            seterr(err, errsz, "invalid app id");
            return -1;
        }
        entry->action = KM_LAUNCH;
        snprintf(entry->arg, sizeof entry->arg, "%s", id);
    } else if (!strncmp(value, "key:", 4)) {
        const char *key = value + 4;
        if (!keymap_valid_key(key)) {
            seterr(err, errsz, "invalid key name");
            return -1;
        }
        entry->action = KM_KEY;
        snprintf(entry->arg, sizeof entry->arg, "%s", key);
    } else if (!strncmp(value, "cycle:", 6)) {
        const char *list = value + 6;
        if (!valid_cycle(list)) {
            seterr(err, errsz, "invalid cycle list");
            return -1;
        }
        entry->action = KM_CYCLE;
        snprintf(entry->arg, sizeof entry->arg, "%s", list);
    } else {
        seterr(err, errsz, "unknown action");
        return -1;
    }

    *button = idx;
    return 1;
}

int keymap_load(const char *path, struct km_entry out[KM_BUTTONS], char *err, size_t errsz) {
    keymap_defaults(out);
    if (err && errsz)
        err[0] = 0;

    FILE *f = fopen(path, "r");
    if (!f)
        return errno == ENOENT ? 0 : -1;

    char line[LINE_MAX];
    int lineno = 0;
    int bad = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        size_t len = strlen(line);
        if (len == sizeof line - 1 && line[len - 1] != '\n') {
            int c;
            while ((c = fgetc(f)) != '\n' && c != EOF)
                ;
            bad++;
            if (err && errsz && !err[0])
                snprintf(err, errsz, "line %d: too long", lineno);
            continue;
        }

        int button = -1;
        struct km_entry entry;
        char lerr[96] = "";
        int rc = keymap_parse_line(line, &button, &entry, lerr, sizeof lerr);
        if (rc < 0) {
            bad++;
            if (err && errsz && !err[0])
                snprintf(err, errsz, "line %d: %s", lineno, lerr);
            continue;
        }
        if (rc == 0)
            continue;
        out[button] = entry;
    }
    fclose(f);
    return bad;
}

void keymap_format(const struct km_entry *entry, char *buf, size_t bufsz) {
    switch (entry->action) {
    case KM_LAUNCH:
        snprintf(buf, bufsz, "launch:%s", entry->arg);
        break;
    case KM_KEY:
        snprintf(buf, bufsz, "key:%s", entry->arg);
        break;
    case KM_CYCLE:
        snprintf(buf, bufsz, "cycle:%s", entry->arg);
        break;
    default:
        snprintf(buf, bufsz, "none");
        break;
    }
}
