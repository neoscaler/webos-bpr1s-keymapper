#ifndef KEYMAP_H
#define KEYMAP_H

#include <stddef.h>

enum km_action { KM_NONE = 0, KM_LAUNCH, KM_KEY, KM_CYCLE };

#define KM_BUTTONS 3
#define KM_ARG_MAX 192

struct km_entry {
    enum km_action action;
    char arg[KM_ARG_MAX];
    unsigned idx;
};

/* Button order: 0 = square (MENU), 1 = triangle (HOME), 2 = circle (MIC). */
void keymap_defaults(struct km_entry out[KM_BUTTONS]);

/* Fills out with the defaults first, then applies every valid line of path.
   Returns the number of rejected lines, -1 when the file exists but cannot be
   read. err receives the first problem, empty when there was none. */
int keymap_load(const char *path, struct km_entry out[KM_BUTTONS], char *err, size_t errsz);

/* 1 = parsed, 0 = line ignored (blank, comment, unknown button), -1 = rejected. */
int keymap_parse_line(const char *line, int *button, struct km_entry *entry,
                      char *err, size_t errsz);

void keymap_format(const struct km_entry *entry, char *buf, size_t bufsz);

int keymap_valid_app_id(const char *id);
int keymap_valid_key(const char *name);

#endif
