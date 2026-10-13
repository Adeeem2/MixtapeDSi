#include "music_library.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef NDS_BUILD
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#endif

void library_init(MusicLibrary *lib, const char *root) {
    memset(lib, 0, sizeof(*lib));
    strncpy(lib->root, root ? root : "/music/", sizeof(lib->root) - 1);
}

#ifndef NDS_BUILD
static int has_mp3_ext(const char *name) {
    size_t n = strlen(name);
    if (n < 4) return 0;
    return strcasecmp(name + n - 4, ".mp3") == 0;
}
#endif

void library_sibling_lrc(const char *mp3_path, char *out, size_t out_len) {
    if (out_len == 0) return;
    strncpy(out, mp3_path, out_len - 1);
    out[out_len - 1] = '\0';
    size_t n = strlen(out);
    if (n >= 4 && strcasecmp(out + n - 4, ".mp3") == 0) {
        out[n - 4] = '\0';
        strncat(out, ".lrc", out_len - strlen(out) - 1);
    } else {
        strncat(out, ".lrc", out_len - strlen(out) - 1);
    }
}

static void sibling_cover(const char *mp3_path, char *out, size_t out_len) {
    /* cover.bmp next to the track (folder art), not per-track file. */
    if (out_len == 0) return;
    strncpy(out, mp3_path, out_len - 1);
    out[out_len - 1] = '\0';
    char *slash = strrchr(out, '/');
    char *bslash = strrchr(out, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
    if (slash) {
        *(slash + 1) = '\0';
        strncat(out, "cover.bmp", out_len - strlen(out) - 1);
    } else {
        strncpy(out, "cover.bmp", out_len - 1);
    }
}

int library_add_track(MusicLibrary *lib, const char *mp3_path) {
    if (!lib || !mp3_path) return -1;
    if (lib->count >= LIB_MAX_TRACKS) { lib->truncated = 1; return -1; }
    LibraryTrack *t = &lib->tracks[lib->count];
    memset(t, 0, sizeof(*t));
    strncpy(t->path, mp3_path, sizeof(t->path) - 1);
    library_sibling_lrc(mp3_path, t->lrc_path, sizeof(t->lrc_path));
    sibling_cover(mp3_path, t->cover_path, sizeof(t->cover_path));
    t->valid = 1;
    metadata_fallback_from_filename(mp3_path, &t->meta);
#ifndef NDS_BUILD
    /* Probe siblings on host so has_lrc/has_cover are meaningful. */
    FILE *f = fopen(t->lrc_path, "rb");
    if (f) { t->has_lrc = 1; fclose(f); }
    f = fopen(t->cover_path, "rb");
    if (f) { t->has_cover = 1; fclose(f); }
#endif
    lib->count++;
    return 0;
}

static int cmp_track(const void *a, const void *b) {
    return strcasecmp(((const LibraryTrack*)a)->path,
                      ((const LibraryTrack*)b)->path);
}

void library_sort_by_filename(MusicLibrary *lib) {
    if (lib->count > 1)
        qsort(lib->tracks, (size_t)lib->count, sizeof(LibraryTrack), cmp_track);
}

#ifndef NDS_BUILD
static void scan_dir(MusicLibrary *lib, const char *dir, int depth) {
    if (depth > 8) return;
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    /* Collect names first for deterministic order, then process. */
    char subdirs[256][LIB_MAX_PATH];
    int nsub = 0;
    char files[1024][LIB_MAX_PATH];
    int nfiles = 0;
    while ((e = readdir(d)) != NULL && lib->count < LIB_MAX_TRACKS) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char full[LIB_MAX_PATH * 2];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (nsub < 256) { strncpy(subdirs[nsub], full, LIB_MAX_PATH-1); subdirs[nsub][LIB_MAX_PATH-1]='\0'; nsub++; }
        } else if (S_ISREG(st.st_mode)) {
            if (nfiles < 1024) { strncpy(files[nfiles], full, LIB_MAX_PATH-1); files[nfiles][LIB_MAX_PATH-1]='\0'; nfiles++; }
        }
    }
    closedir(d);
    for (int i = 0; i < nfiles && lib->count < LIB_MAX_TRACKS; i++)
        if (has_mp3_ext(files[i])) library_add_track(lib, files[i]);
    if (lib->count >= LIB_MAX_TRACKS) lib->truncated = 1;
    for (int i = 0; i < nsub && lib->count < LIB_MAX_TRACKS; i++)
        scan_dir(lib, subdirs[i], depth + 1);
}

int library_scan(MusicLibrary *lib) {
    if (!lib) return -1;
    int base = lib->count;
    scan_dir(lib, lib->root, 0);
    library_sort_by_filename(lib);
    return lib->count - base;
}
#else
/* NDS_BUILD: libfat-based scanner lives in source/nds/filesystem_nds.c
 * and calls library_add_track() per file. This stub keeps the portable
 * translation unit linkable. */
int library_scan(MusicLibrary *lib) {
    (void)lib;
    return 0;
}
#endif
