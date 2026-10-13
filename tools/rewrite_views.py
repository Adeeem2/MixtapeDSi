#!/usr/bin/env python3
"""One-shot rewrite of lyric/library/queue/compact views (raw strings)."""
import sys

P = "source/nds/main_nds.c"
src = open(P).read()

ESC = chr(0x5C) + "x1b"  # literal backslash-x-1-b as in the C source

# --- 1. replace lyric body lines (keep header/title part already added? no:
# current body after 'consoleClear();' is still the old 3 lines). ---
old_body = (
    '    if (prv >= 0) iprintf("' + ESC + '[30;1m  %s' + ESC + '[39m\\n", g_lrc.lines[prv].text);\n'
    '    if (cur >= 0) iprintf("' + ESC + '[32;1m> %s' + ESC + '[39m\\n", g_lrc.lines[cur].text);\n'
    '    else iprintf("(intro...)\\n");\n'
    '    if (nxt >= 0) iprintf("' + ESC + '[37m  %s' + ESC + '[39m\\n", g_lrc.lines[nxt].text);\n'
)
assert src.count(old_body) == 1, "body anchor not unique/found"
new_body = (
    '    if (idx >= 0 && g_have_track) {\n'
    '        LibraryTrack *t = &g_lib.tracks[idx];\n'
    '        char tmp[64];\n'
    '        to_latin1(tmp, t->meta.title[0] ? t->meta.title : "Unknown", sizeof(tmp));\n'
    '        con->cursorX = 0; con->cursorY = 1;\n'
    '        iprintf("' + ESC + '[32;1m");\n'
    '        print_center(tmp, 28, 2);\n'
    '        iprintf("' + ESC + '[39m");\n'
    '        to_latin1(tmp, t->meta.artist[0] ? t->meta.artist : "", sizeof(tmp));\n'
    '        con->cursorX = 0; con->cursorY = 2;\n'
    '        iprintf("' + ESC + '[30;1m");\n'
    '        print_center(tmp, 28, 2);\n'
    '        iprintf("' + ESC + '[39m");\n'
    '    }\n'
    '    if (prv >= 0) {\n'
    '        con->cursorX = 0; con->cursorY = 9;\n'
    '        iprintf("' + ESC + '[30;1m");\n'
    '        print_center(g_lrc.lines[prv].text, 28, 2);\n'
    '        iprintf("' + ESC + '[39m");\n'
    '    }\n'
    '    con->cursorX = 0; con->cursorY = 11;\n'
    '    if (cur >= 0) {\n'
    '        iprintf("' + ESC + '[32;1m");\n'
    '        print_center(g_lrc.lines[cur].text, 28, 2);\n'
    '        iprintf("' + ESC + '[39m");\n'
    '    } else {\n'
    '        iprintf("' + ESC + '[30;1m");\n'
    '        print_center(g_have_track ? "(intro...)" : "No track: open Library", 28, 2);\n'
    '        iprintf("' + ESC + '[39m");\n'
    '    }\n'
    '    if (nxt >= 0) {\n'
    '        con->cursorX = 0; con->cursorY = 13;\n'
    '        print_center(g_lrc.lines[nxt].text, 28, 2);\n'
    '    }\n'
    '    {\n'
    '        uint32_t total = g_lrc.meta.has_length ? (uint32_t)g_lrc.meta.length_ms : 0;\n'
    '        con->cursorX = 0; con->cursorY = 22;\n'
    '        if (total > 0)\n'
    '            iprintf("   %02lu:%02lu / %02lu:%02lu",\n'
    '                    (unsigned long)(pos / 60000), (unsigned long)((pos / 1000) % 60),\n'
    '                    (unsigned long)(total / 60000), (unsigned long)((total / 1000) % 60));\n'
    '        else if (g_have_track)\n'
    '            iprintf("   %02lu:%02lu", (unsigned long)(pos / 60000), (unsigned long)((pos / 1000) % 60));\n'
    '    }\n'
)
src = src.replace(old_body, new_body)

# --- 2. add 'int idx' to view_lyrics after use_screen(top); ---
old_head = (
    '    use_screen(top);\n'
    '    uint32_t pos = now_pos_ms();\n'
    '    int prv, cur, nxt;\n'
)
assert src.count(old_head) == 1, "head anchor"
new_head = (
    '    use_screen(top);\n'
    '    int idx = pb_current_index(&g_pb);\n'
    '    uint32_t pos = now_pos_ms();\n'
    '    int prv, cur, nxt;\n'
)
src = src.replace(old_head, new_head)

open(P, "w").write(src)
print("views rewritten")
