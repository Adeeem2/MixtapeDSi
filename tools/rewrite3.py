#!/usr/bin/env python3
"""Add queue/compact views, latin1 library, dual-screen render loop."""
P = "source/nds/main_nds.c"
src = open(P).read()
ESC = chr(0x5C) + "x1b"


def rep(old, new, count=1):
    global src
    assert src.count(old) == count, "anchor: " + old[:70]
    src = src.replace(old, new)


# 1. library rows: latin-1 titles
rep("""    for (int i = start; i < g_lib.count && i < start + 14; i++) {
        if (i == g_sel) iprintf("\\x1b[32;1m> %-22.22s\\x1b[39m\\n", g_lib.tracks[i].meta.title);
        else iprintf("  %-22.22s\\n", g_lib.tracks[i].meta.title);
    }""".replace("\\x1b", ESC),
    """    for (int i = start; i < g_lib.count && i < start + 14; i++) {
        char nm[64];
        to_latin1(nm, g_lib.tracks[i].meta.title, sizeof(nm));
        nm[22] = '\\0';
        if (i == g_sel) iprintf("}""" + ESC + """[32;1m> %-22.22s""" + ESC + """[39m
", nm);
        else iprintf("  %-22.22s
", nm);
    }""")

open(P, "w").write(src)
print("lib latin1 ok")
