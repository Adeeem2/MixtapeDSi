#!/usr/bin/env python3
"""Add queue + compact-top views and dual-screen render loop."""
P = "source/nds/main_nds.c"
src = open(P).read()
ESC = chr(0x5C) + "x1b"
Q = chr(0x5C) + '"'


def rep(old, new, count=1):
    global src
    assert src.count(old) == count, "anchor: " + old[:70]
    src = src.replace(old, new)


# 1. queue + compact views before load_track
rep("static void load_track(int idx) {",
    """static void view_queue(void) {
    use_screen(0);
    draw_header("Queue");
    if (!g_pb.order_len) { iprintf("(empty)\\n"); return; }
    int start = g_qsel > 8 ? g_qsel - 8 : 0;
    for (int i = start; i < g_pb.order_len && i < start + 14; i++) {
        int li = g_pb.order[i];
        char nm[64];
        to_latin1(nm, g_lib.tracks[li].meta.title, sizeof(nm));
        nm[22] = '\\0';
        const char *mark = (i == g_pb.order_pos) ? "*" : " ";
        if (i == g_qsel) iprintf("}""" + ESC + """[32;1m>%s%-21.21s""" + ESC + """[39m
", mark, nm);
        else iprintf(" %s%-21.21s
", mark, nm);
    }
    iprintf("\\nA:play B:home\\n");
}

static void view_compact_top(void) {
    use_screen(1);
    int idx = pb_current_index(&g_pb);
    consoleClear();
    if (idx < 0 || !g_have_track) {
        g_con_top->cursorX = 0; g_con_top->cursorY = 11;
        print_center("MixtapeDSi", 28, 2);
        return;
    }
    LibraryTrack *t = &g_lib.tracks[idx];
    uint32_t pos = now_pos_ms();
    g_con_top->cursorX = 0; g_con_top->cursorY = 9;
    iprintf("}""" + ESC + """[32;1m");
    print_center(t->meta.title[0] ? t->meta.title : "Unknown", 28, 2);
    iprintf("}""" + ESC + """[39m");
    g_con_top->cursorX = 0; g_con_top->cursorY = 10;
    iprintf("}""" + ESC + """[30;1m");
    print_center(t->meta.artist[0] ? t->meta.artist : "", 28, 2);
    iprintf("}""" + ESC + """[39m");
    g_con_top->cursorX = 0; g_con_top->cursorY = 12;
    if (g_pb.state == PB_PLAYING) iprintf("}""" + ESC + """[32mPLAY");
    else iprintf("}""" + ESC + """[33mPAUSE");
    iprintf("}""" + ESC + """[39m %02lu:%02lu", (unsigned long)(pos / 60000),
            (unsigned long)((pos / 1000) % 60));
}

static void load_track(int idx) {""")

# 2. render loop: dual screen + ui repaints + queue case + extra gating
rep("""        static int l_menu = -1, l_lyr = -1, l_vol = -1, l_eqt = -1;""",
    """        static int l_menu = -1, l_lyr = -1, l_vol = -1, l_eqt = -1;
        static int l_qsel = -1, l_trk = -2, l_like = -1, l_topm = -1;""")
rep("""        int eqt_now = (int)(pos_now / 500);
        if (g_view != l_view || g_sel != l_sel || sec_now != l_sec ||""",
    """        int eqt_now = (int)(pos_now / 500);
        int trk_now = pb_current_index(&g_pb);
        int ci = (trk_now >= 0 && g_have_track) ? ui_liked(g_lib.tracks[trk_now].path) : 0;
        int topm = (g_cfg.lyrics_screen == LYRICS_TOP) ? 1 : 0;
        if (g_view != l_view || g_sel != l_sel || sec_now != l_sec ||""")
rep("""            g_pb.shuffle != l_sh || g_menu != l_menu ||
            (int)g_cfg.lyrics_screen != l_lyr || g_cfg.volume != l_vol ||
            (g_view == VIEW_NOW && eqt_now != l_eqt)) {""",
    """            g_pb.shuffle != l_sh || g_menu != l_menu ||
            (int)g_cfg.lyrics_screen != l_lyr || g_cfg.volume != l_vol ||
            g_qsel != l_qsel || trk_now != l_trk || ci != l_like || topm != l_topm ||
            ((g_view == VIEW_NOW || g_cfg.lyrics_screen == LYRICS_TOP) && eqt_now != l_eqt)) {""")
rep("""        switch (g_view) {
            case VIEW_HOME: view_home(); break;
            case VIEW_NOW: view_now_playing(); break;
            case VIEW_LYRICS: view_lyrics(); break;
            case VIEW_LIB: view_library(); break;""",
    """        /* repaint dynamic tiles to match state */
        ui_transport(g_pb.state == PB_PLAYING, g_pb.shuffle, g_pb.repeat != REPEAT_OFF);
        ui_heart(ci);
        ui_tabs(g_view == VIEW_LIB ? 0 : g_view == VIEW_NOW ? 1 : g_view == VIEW_QUEUE ? 2 : -1);
        if (g_view == VIEW_NOW)
            ui_progress(pos_now, g_lrc.meta.has_length ? (uint32_t)g_lrc.meta.length_ms : 0);
        ui_eq(audio_hw_level(), eqt_now, g_pb.state == PB_PLAYING && g_have_track);
        switch (g_view) {
            case VIEW_HOME: view_home(); break;
            case VIEW_NOW: view_now_playing(); break;
            case VIEW_LYRICS: view_lyrics(); break;
            case VIEW_LIB: view_library(); break;
            case VIEW_QUEUE: view_queue(); break;""")
rep("""        l_menu = g_menu; l_lyr = (int)g_cfg.lyrics_screen; l_vol = g_cfg.volume;
        l_eqt = eqt_now;
        }""",
    """        if (g_cfg.lyrics_screen == LYRICS_TOP) view_lyrics();
        else view_compact_top();
        l_menu = g_menu; l_lyr = (int)g_cfg.lyrics_screen; l_vol = g_cfg.volume;
        l_eqt = eqt_now;
        l_qsel = g_qsel; l_trk = trk_now; l_like = ci; l_topm = topm;
        }""")

open(P, "w").write(src)
print("render ok")
