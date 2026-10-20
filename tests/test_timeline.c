/* Timeline + playback + metadata + library tests. */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "lrc_parser.h"
#include "lyric_timeline.h"
#include "playback_controller.h"
#include "metadata.h"
#include "music_library.h"
#include "audio_stream.h"
#include "app_config.h"

static int checks = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } checks++; } while (0)

int main(void) {
    LrcSong s;
    lrc_parse("[00:01.00]a\n[00:05.00]b\n[00:10.00]c\n", &s);

    /* before first */
    CHECK(lyric_active_index(&s, 0) == -1);
    CHECK(lyric_active_index(&s, 999) == -1);
    /* exact match */
    CHECK(lyric_active_index(&s, 1000) == 0);
    /* between */
    CHECK(lyric_active_index(&s, 4999) == 0);
    CHECK(lyric_active_index(&s, 5000) == 1);
    /* seeking backward */
    CHECK(lyric_active_index(&s, 11000) == 2);
    CHECK(lyric_active_index(&s, 2000) == 0);
    /* window */
    { int p, c, n; lyric_window(&s, 6000, &p, &c, &n);
      CHECK(p == 0 && c == 1 && n == 2); }
    { int p, c, n; lyric_window(&s, 0, &p, &c, &n);
      CHECK(p == -1 && c == -1 && n == 0); }
    CHECK(lyric_index_changed(&s, 1000, 6000) == 1);
    CHECK(lyric_index_changed(&s, 1000, 1500) == 0);

    /* karaoke sweep: line 0 runs 1s..5s, line 1 runs 5s..10s */
    CHECK(lyric_line_duration_ms(&s, 0, 0) == 4000);
    CHECK(lyric_line_duration_ms(&s, 1, 0) == 5000);
    CHECK(lyric_line_duration_ms(&s, -1, 0) == 0);
    CHECK(lyric_sweep_chars(&s, 0, 0, 10, 0) == 0);      /* before start */
    CHECK(lyric_sweep_chars(&s, 0, 1000, 10, 0) == 0);   /* exact start */
    CHECK(lyric_sweep_chars(&s, 0, 3000, 10, 0) == 5);   /* halfway */
    CHECK(lyric_sweep_chars(&s, 0, 4999, 10, 0) == 9);
    CHECK(lyric_sweep_chars(&s, 0, 5000, 10, 0) == 10);  /* clamped */
    CHECK(lyric_sweep_chars(&s, 0, 9000, 10, 0) == 10);
    CHECK(lyric_sweep_chars(&s, -1, 3000, 10, 0) == 0);
    CHECK(lyric_sweep_chars(&s, 0, 3000, 0, 0) == 0);
    /* very short gap clamps to 800 ms */
    { LrcSong q; lrc_parse("[00:01.00]a\n[00:01.20]b\n", &q);
      CHECK(lyric_line_duration_ms(&q, 0, 0) == 800);
      CHECK(lyric_sweep_chars(&q, 0, 1600, 10, 0) == 7); }
    /* last line uses [length:] when available, else 4 s */
    { LrcSong q; lrc_parse("[00:01.00]a\n", &q);
      CHECK(lyric_line_duration_ms(&q, 0, 11000) == 10000);
      CHECK(lyric_line_duration_ms(&q, 0, 0) == 4000); }

    /* empty song */
    { LrcSong e; memset(&e, 0, sizeof(e)); CHECK(lyric_active_index(&e, 9999) == -1); }

    /* playback controller */
    MusicLibrary lib;
    library_init(&lib, "/music/");
    library_add_track(&lib, "/music/b.mp3");
    library_add_track(&lib, "/music/a.mp3");
    library_sort_by_filename(&lib);
    CHECK(strstr(lib.tracks[0].path, "a.mp3") != NULL);
    /* sibling lrc */
    char lrc[256]; library_sibling_lrc("/music/Daft Punk/01 - One More Time.mp3", lrc, sizeof(lrc));
    CHECK(strcmp(lrc, "/music/Daft Punk/01 - One More Time.lrc") == 0);
    CHECK(strlen(lib.tracks[0].lrc_path) > 4);

    PlaybackController pb;
    pb_init(&pb, &lib);
    CHECK(pb_current_index(&pb) == 0);
    pb_play_index(&pb, 1);
    CHECK(pb_current_index(&pb) == 1 && pb.state == PB_PLAYING);
    pb_advance_samples(&pb, 44100);
    CHECK(pb_position_ms(&pb) == 1000);
    pb_pause(&pb); CHECK(pb.state == PB_PAUSED);
    pb_resume(&pb); CHECK(pb.state == PB_PLAYING);
    pb_seek_ms(&pb, 5000); CHECK(pb_position_ms(&pb) == 5000);
    CHECK(pb_next(&pb) == 0 && pb.state == PB_STOPPED); /* end, repeat off */
    pb_set_repeat(&pb, REPEAT_ALL);
    pb_play_index(&pb, 1);
    CHECK(pb_next(&pb) == 1 && pb_current_index(&pb) == 0); /* wrap */
    pb_set_repeat(&pb, REPEAT_ONE);
    pb_play_index(&pb, 0);
    CHECK(pb_on_track_end(&pb) == 1 && pb_current_index(&pb) == 0);
    /* shuffle keeps all tracks, changes order deterministically */
    pb_set_repeat(&pb, REPEAT_OFF);
    pb_set_shuffle(&pb, 1);
    CHECK(pb.order_len == 2);
    { int seen[2] = {0,0}; for (int i=0;i<2;i++) seen[pb.order[i]] = 1;
      CHECK(seen[0] && seen[1]); }

    /* metadata: ID3v1 + fallback */
    { uint8_t buf[256]; memset(buf, 0, sizeof(buf));
      memset(buf, ' ', sizeof(buf));
      memcpy(buf + 128, "TAG", 3);
      memcpy(buf + 131, "My Title", 8);
      TrackMeta m; metadata_parse_buffer(buf, sizeof(buf), &m);
      CHECK(strcmp(m.title, "My Title") == 0);
      TrackMeta m2; memset(&m2, 0, sizeof(m2));
      metadata_fallback_from_filename("/music/Daft Punk/01 - One More Time.mp3", &m2);
      CHECK(strcmp(m2.title, "01 - One More Time") == 0); }
    /* ID3v2 TIT2 */
    { uint8_t tag[64]; memset(tag, 0, sizeof(tag));
      memcpy(tag, "ID3\x03\x00\x00\x00\x00\x00\x0F", 10);
      memcpy(tag + 10, "TIT2", 4);
      tag[14] = 0; tag[15] = 0; tag[16] = 0; tag[17] = 5;
      tag[20] = 3; memcpy(tag + 21, "Hey!", 4);
      TrackMeta m; memset(&m, 0, sizeof(m));
      metadata_parse_buffer(tag, 10 + 10 + 5, &m);
      CHECK(strcmp(m.title, "Hey!") == 0);
      CHECK(metadata_audio_offset(tag, 10 + 10 + 5) == 10 + 0x0F); }

    /* audio stream clock (host stub synthesizes PCM) */
    { AudioStream st; memset(&st, 0, sizeof(st));
      /* use this test binary path as a fake file (exists, readable) */
      CHECK(audio_stream_open(&st, "tests/test_timeline.c") == 0);
      int got = audio_stream_pump(&st);
      CHECK(got > 0);
      audio_stream_consumed(&st, (uint32_t)got);
      (void)audio_stream_position_ms(&st);
      audio_stream_close(&st); }

    /* config */
    { AppConfig c; config_defaults(&c);
      CHECK(strcmp(c.music_dir, "/music/") == 0 && c.auto_follow == 1);
      config_parse(&c, "lyrics_screen=1\nanim=1\nvolume=80\n");
      CHECK(c.lyrics_screen == LYRICS_BOTTOM && c.anim == ANIM_REDUCED && c.volume == 80); }

    /* flat folder: songs + .lrc side by side, matched by basename */
    {
        MusicLibrary flat;
        library_init(&flat, "tests/data/flat");
        int found = library_scan(&flat);
        CHECK(found == 2 && flat.count == 2);
        int with_lrc = 0;
        for (int i = 0; i < flat.count; i++) {
            if (strstr(flat.tracks[i].path, "Close to you.mp3")) {
                CHECK(flat.tracks[i].has_lrc == 1);
                CHECK(strstr(flat.tracks[i].lrc_path, "Close to you.lrc") != NULL);
                with_lrc++;
            } else {
                CHECK(flat.tracks[i].has_lrc == 0);
            }
        }
        CHECK(with_lrc == 1);
    }

    printf("test_timeline: ALL %d checks passed\n", checks);
    return 0;
}
