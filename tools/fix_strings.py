#!/usr/bin/env python3
"""Fix split string literals left by rewrite3/rewrite4."""
P = "source/nds/main_nds.c"
src = open(P).read()
ESC = chr(0x5C) + "x1b"
BS = chr(0x5C)


def rep(old, new):
    global src
    assert src.count(old) == 1, "anchor: " + old[:60].replace(chr(10), "\\n")
    src = src.replace(old, new)


# --- library selection block ---
rep('        if (i == g_sel) iprintf("}' + ESC + '[32;1m> %-22.22s' + ESC + '[39m\n", nm);',
    '        if (i == g_sel) iprintf("' + ESC + '[32;1m> %-22.22s' + ESC + '[39m' + BS + 'n", nm);')
rep('        else iprintf("  %-22.22s\n", nm);',
    '        else iprintf("  %-22.22s' + BS + 'n", nm);')

# --- queue selection block ---
rep('        if (i == g_qsel) iprintf("}' + ESC + '[32;1m>%s%-21.21s' + ESC + '[39m\n", mark, nm);',
    '        if (i == g_qsel) iprintf("' + ESC + '[32;1m>%s%-21.21s' + ESC + '[39m' + BS + 'n", mark, nm);')
rep('        else iprintf(" %s%-21.21s\n", mark, nm);',
    '        else iprintf(" %s%-21.21s' + BS + 'n", mark, nm);')

open(P, "w").write(src)
print("strings fixed")
