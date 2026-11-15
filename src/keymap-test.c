#include "keymap.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int failures;
static char path[64];

static void write_file(const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) {
        printf("cannot write %s\n", path);
        failures++;
        return;
    }
    fputs(content, f);
    fclose(f);
}

static void check(int cond, const char *what) {
    if (!cond) {
        printf("FAIL %s\n", what);
        failures++;
    }
}

static void expect(const struct km_entry *e, enum km_action action, const char *arg,
                   const char *what) {
    if (e->action != action || strcmp(e->arg, arg)) {
        char got[256];
        keymap_format(e, got, sizeof got);
        printf("FAIL %s: got %s, want %s %s\n", what, got,
               action == KM_NONE ? "none" : action == KM_LAUNCH ? "launch" :
               action == KM_KEY ? "key" : "cycle", arg);
        failures++;
    }
}

static void expect_defaults(const struct km_entry *cfg, const char *what) {
    expect(&cfg[0], KM_CYCLE,
           "com.webos.app.hdmi1,com.webos.app.hdmi2,com.webos.app.hdmi3,com.webos.app.hdmi4", what);
    expect(&cfg[1], KM_LAUNCH, "com.webos.app.home", what);
    expect(&cfg[2], KM_LAUNCH, "com.webos.app.livetv", what);
}

int main(void) {
    snprintf(path, sizeof path, "/tmp/keymap-test-%d.conf", (int)getpid());
    unlink(path);

    struct km_entry cfg[KM_BUTTONS];
    char err[128];

    check(keymap_load(path, cfg, err, sizeof err) == 0, "missing file is not an error");
    check(err[0] == 0, "missing file sets no error");
    expect_defaults(cfg, "missing file");

    write_file("# comment\n"
               "\n"
               "square   = launch:com.webos.app.browser\n"
               "triangle = key:INFO\n"
               "circle   = none\n");
    check(keymap_load(path, cfg, err, sizeof err) == 0, "valid file has no rejected lines");
    check(err[0] == 0, "valid file sets no error");
    expect(&cfg[0], KM_LAUNCH, "com.webos.app.browser", "square launch");
    expect(&cfg[1], KM_KEY, "INFO", "triangle key");
    expect(&cfg[2], KM_NONE, "", "circle none");

    write_file("square = cycle:a.b,c.d,e.f\r\n"
               "triangle = key:BACK\r\n");
    check(keymap_load(path, cfg, err, sizeof err) == 0, "CRLF file has no rejected lines");
    expect(&cfg[0], KM_CYCLE, "a.b,c.d,e.f", "cycle list");
    expect(&cfg[1], KM_KEY, "BACK", "CRLF key");
    expect(&cfg[2], KM_LAUNCH, "com.webos.app.livetv", "unset button keeps default");

    write_file("home = launch:com.webos.app.home\n"
               "square = launch:com.webos.app.music\n");
    check(keymap_load(path, cfg, err, sizeof err) == 0, "unknown button is ignored");
    expect(&cfg[0], KM_LAUNCH, "com.webos.app.music", "known button still applies");
    expect(&cfg[1], KM_LAUNCH, "com.webos.app.home", "unknown button changes nothing");

    write_file("square = launch:com.webos.app.music\n"
               "triangle = explode:now\n");
    check(keymap_load(path, cfg, err, sizeof err) == 1, "unknown action is rejected");
    check(strstr(err, "line 2") != NULL, "error names the line");
    expect(&cfg[0], KM_LAUNCH, "com.webos.app.music", "valid line before the error applies");
    expect(&cfg[1], KM_LAUNCH, "com.webos.app.home", "rejected line keeps default");

    write_file("square launch:com.webos.app.home\n");
    check(keymap_load(path, cfg, err, sizeof err) == 1, "missing '=' is rejected");
    expect_defaults(cfg, "missing '='");

    write_file("square = launch:foo;rm -rf /\n");
    check(keymap_load(path, cfg, err, sizeof err) == 1, "shell metacharacters are rejected");
    expect(&cfg[0], KM_CYCLE,
           "com.webos.app.hdmi1,com.webos.app.hdmi2,com.webos.app.hdmi3,com.webos.app.hdmi4",
           "rejected app id keeps default");

    write_file("square = launch:../../etc/passwd\n");
    check(keymap_load(path, cfg, err, sizeof err) == 1, "path traversal is rejected");

    write_file("square = launch:'quoted'\n");
    check(keymap_load(path, cfg, err, sizeof err) == 1, "quotes are rejected");

    write_file("square = key:INPUT\n");
    check(keymap_load(path, cfg, err, sizeof err) == 1, "unknown key name is rejected");

    write_file("square = key:volumeup\n");
    check(keymap_load(path, cfg, err, sizeof err) == 1, "lowercase key name is rejected");

    write_file("square = cycle:\n");
    check(keymap_load(path, cfg, err, sizeof err) == 1, "empty cycle is rejected");

    write_file("square = cycle:a,b,c,d,e\n");
    check(keymap_load(path, cfg, err, sizeof err) == 1, "five cycle entries are rejected");

    write_file("square = cycle:a,,b\n");
    check(keymap_load(path, cfg, err, sizeof err) == 1, "empty cycle entry is rejected");

    char longline[600];
    memset(longline, 'x', sizeof longline - 1);
    longline[0] = 's';
    longline[sizeof longline - 1] = 0;
    FILE *f = fopen(path, "w");
    if (f) {
        fputs("square = launch:com.webos.app.home\n", f);
        fputs(longline, f);
        fputc('\n', f);
        fputs("circle = none\n", f);
        fclose(f);
    }
    check(keymap_load(path, cfg, err, sizeof err) == 1, "overlong line is rejected");
    check(strstr(err, "too long") != NULL, "overlong line says so");
    expect(&cfg[0], KM_LAUNCH, "com.webos.app.home", "line before the overlong one applies");
    expect(&cfg[2], KM_NONE, "", "line after the overlong one still applies");

    struct km_entry e;
    int button = -1;
    check(keymap_parse_line("", &button, &e, err, sizeof err) == 0, "empty line ignored");
    check(keymap_parse_line("   # note", &button, &e, err, sizeof err) == 0, "comment ignored");

    char fmt[KM_ARG_MAX + 16];
    e.action = KM_CYCLE;
    snprintf(e.arg, sizeof e.arg, "a.b,c.d");
    keymap_format(&e, fmt, sizeof fmt);
    check(!strcmp(fmt, "cycle:a.b,c.d"), "cycle formats back");
    e.action = KM_NONE;
    e.arg[0] = 0;
    keymap_format(&e, fmt, sizeof fmt);
    check(!strcmp(fmt, "none"), "none formats back");

    unlink(path);

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("keymap-test: all checks passed\n");
    return 0;
}
