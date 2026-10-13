#ifndef MIXTAPEDSI_MUSIC_LIBRARY_H
#define MIXTAPEDSI_MUSIC_LIBRARY_H

#include <stddef.h>
#include "metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LIB_MAX_TRACKS 512
#define LIB_MAX_PATH 256

typedef struct {
    char path[LIB_MAX_PATH];      /* full path to .mp3 */
    char lrc_path[LIB_MAX_PATH];  /* sibling .lrc ("" if absent) */
    char cover_path[LIB_MAX_PATH];/* sibling cover.bmp ("" if absent) */
    TrackMeta meta;
    int has_lrc;
    int has_cover;
    int valid;                    /* 0 = unreadable/unsupported entry */
} LibraryTrack;

typedef struct {
    LibraryTrack tracks[LIB_MAX_TRACKS];
    int count;
    int truncated;
    char root[LIB_MAX_PATH];
} MusicLibrary;

void library_init(MusicLibrary *lib, const char *root);

/* Insert one mp3 path (used by scanner + tests). Returns 0 on success,
 * -1 when full. Derives lrc/cover sibling paths & filename fallback. */
int library_add_track(MusicLibrary *lib, const char *mp3_path);

/* Sort tracks by path (filename order). */
void library_sort_by_filename(MusicLibrary *lib);

/* Build sibling .lrc path for an .mp3 path. out must hold LIB_MAX_PATH. */
void library_sibling_lrc(const char *mp3_path, char *out, size_t out_len);

/* Filesystem scan. On host uses opendir/readdir recursively (bounded).
 * On NDS (ARM9) uses libfat dirent API via source/nds port. Returns
 * number of tracks found. Never loads audio/artwork into RAM. */
int library_scan(MusicLibrary *lib);

#ifdef __cplusplus
}
#endif

#endif
