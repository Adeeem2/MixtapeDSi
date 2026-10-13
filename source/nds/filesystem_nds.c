/* libfat directory scanner for NDS (ARM9 only). */
#ifdef ARM9
#include <nds.h>
#include <fat.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "music_library.h"

static int endswith_mp3(const char *n) {
    size_t l = strlen(n);
    return l > 4 && strcasecmp(n + l - 4, ".mp3") == 0;
}

static void scan_rec(MusicLibrary *lib, const char *dir, int depth) {
    if (depth > 8 || lib->count >= LIB_MAX_TRACKS) return;
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char full[300];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        if (e->d_type == DT_DIR) scan_rec(lib, full, depth + 1);
        else if (endswith_mp3(e->d_name)) {
            library_add_track(lib, full);
            if (lib->count >= LIB_MAX_TRACKS) { lib->truncated = 1; break; }
        }
    }
    closedir(d);
}

int nds_scan(MusicLibrary *lib) {
    int base = lib->count;
    scan_rec(lib, lib->root, 0);
    library_sort_by_filename(lib);
    return lib->count - base;
}
#endif
