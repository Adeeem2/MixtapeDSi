#!/usr/bin/env python3
"""Rewire input/render for graphical UI: queue, touch, likes, dual-screen."""
P = "source/nds/main_nds.c"
src = open(P).read()

NL = "\n"


def rep(old, new, count=1):
    global src
    assert src.count(old) == count, "anchor: " + old[:60]
    src = src.replace(old, new)


# 1. home menu: 5 items incl Queue
rep("""            if (down & KEY_DOWN) g_menu = (g_menu + 1) % 4;
            if (down & KEY_UP) g_menu = (g_menu + 3) % 4;
            if (down & KEY_A) {
                static const int dest[4] = {VIEW_NOW, VIEW_LYRICS, VIEW_LIB, VIEW_SETTINGS};
                g_view = dest[g_menu];
            }""",
    """            if (down & KEY_DOWN) g_menu = (g_menu + 1) % 5;
            if (down & KEY_UP) g_menu = (g_menu + 4) % 5;
            if (down & KEY_A) {
                static const int dest[5] = {VIEW_NOW, VIEW_LYRICS, VIEW_LIB, VIEW_QUEUE, VIEW_SETTINGS};
                g_view = dest[g_menu];
            }""")

# 2. queue branch after library branch
rep("""            if (down & KEY_B) g_view = VIEW_HOME;
        } else if (g_view == VIEW_SETTINGS) {""",
    """            if (down & KEY_B) g_view = VIEW_HOME;
        } else if (g_view == VIEW_QUEUE) {
            if (g_pb.order_len) {
                if (down & KEY_DOWN) g_qsel = (g_qsel + 1) % g_pb.order_len;
                if (down & KEY_UP) g_qsel = (g_qsel - 1 + g_pb.order_len) % g_pb.order_len;
                if (down & KEY_A) {
                    pb_play_order_pos(&g_pb, g_qsel);
                    load_track(pb_current_index(&g_pb));
                    g_view = VIEW_NOW;
                }
            }
            if (down & KEY_B) g_view = VIEW_HOME;
        } else if (g_view == VIEW_SETTINGS) {""")

# 3. touch + SELECT-like in the generic (player) branch: insert before KEY_B line
rep("""            if (down & KEY_B) g_view = VIEW_HOME;
            if (g_have_track && g_pb.state == PB_PLAYING) {""",
    """            if (down & KEY_B) g_view = VIEW_HOME;
            if (down & KEY_SELECT) {
                int ci = pb_current_index(&g_pb);
                if (ci >= 0 && g_have_track) {
                    ui_like_toggle(g_lib.tracks[ci].path);
                    ui_heart(ui_liked(g_lib.tracks[ci].path));
                }
            }
            if ((down & KEY_TOUCH) && g_view == VIEW_NOW) {
                touchPosition tch;
                touchRead(&tch);
                int ci = pb_current_index(&g_pb);
                if ((tch.px - UI_B_PLAY_X) * (tch.px - UI_B_PLAY_X) +
                    (tch.py - UI_B_Y) * (tch.py - UI_B_Y) < UI_B_R * UI_B_R * 4) {
                    if (g_have_track) {
                        if (g_pb.state == PB_PLAYING) do_pause();
                        else do_resume();
                    }
                } else if ((tch.px - UI_B_PREV_X) * (tch.px - UI_B_PREV_X) +
                           (tch.py - UI_B_Y) * (tch.py - UI_B_Y) < UI_B_R * UI_B_R * 4) {
                    if (pb_prev(&g_pb)) { load_track(pb_current_index(&g_pb)); do_resume(); }
                } else if ((tch.px - UI_B_NEXT_X) * (tch.px - UI_B_NEXT_X) +
                           (tch.py - UI_B_Y) * (tch.py - UI_B_Y) < UI_B_R * UI_B_R * 4) {
                    if (pb_next(&g_pb)) { load_track(pb_current_index(&g_pb)); do_resume(); }
                } else if ((tch.px - UI_B_SHUF_X) * (tch.px - UI_B_SHUF_X) +
                           (tch.py - UI_B_Y) * (tch.py - UI_B_Y) < UI_B_R * UI_B_R * 4) {
                    pb_set_shuffle(&g_pb, !g_pb.shuffle);
                    g_cfg.shuffle_default = g_pb.shuffle;
                    save_config();
                } else if ((tch.px - UI_B_REP_X) * (tch.px - UI_B_REP_X) +
                           (tch.py - UI_B_Y) * (tch.py - UI_B_Y) < UI_B_R * UI_B_R * 4) {
                    pb_cycle_repeat(&g_pb);
                    g_cfg.repeat_default = (int)g_pb.repeat;
                    save_config();
                } else if (tch.px >= UI_HEART_X0 && tch.px <= UI_HEART_X1 &&
                           tch.py >= UI_HEART_Y0 && tch.py <= UI_HEART_Y1) {
                    if (ci >= 0 && g_have_track) {
                        ui_like_toggle(g_lib.tracks[ci].path);
                        ui_heart(ui_liked(g_lib.tracks[ci].path));
                    }
                } else if (tch.py >= UI_TAB_Y0 && tch.py <= UI_TAB_Y1) {
                    if (tch.px >= UI_TAB0_X0 && tch.px <= UI_TAB0_X1) g_view = VIEW_LIB;
                    else if (tch.px >= UI_TAB1_X0 && tch.px <= UI_TAB1_X1) g_view = VIEW_NOW;
                    else if (tch.px >= UI_TAB2_X0 && tch.px <= UI_TAB2_X1) g_view = VIEW_QUEUE;
                }
            }
            if (g_have_track && g_pb.state == PB_PLAYING) {""")

# 4. cover load on track change
rep("""    audio_hw_set_volume(g_cfg.volume);
    /* prime the ring so playback starts with a full buffer */""",
    """    audio_hw_set_volume(g_cfg.volume);
    ui_cover_show(t->cover_path);
    /* prime the ring so playback starts with a full buffer */""")

open(P, "w").write(src)
print("rewire ok")
