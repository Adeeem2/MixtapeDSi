#ifndef MIXTAPEDSI_LYRIC_TEXT_H
#define MIXTAPEDSI_LYRIC_TEXT_H

/* Portable lyric text helpers: UTF-8 decode + word wrap. No HW dependency. */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decode one UTF-8 char; returns codepoint, advances *p past it. */
uint32_t utf8_decode(const char **p);

/* Map a codepoint to a font slot/comment for display:
 * ASCII direct; Latin-1 direct; common punctuation folded;
 * returns '?' slot otherwise. Folded value returned. */
uint32_t display_fold(uint32_t cp);

/* Greedy word wrap into at most max_lines lines of max_chars display
 * columns. Returns line count; lines[i] points into scratch (caller
 * provides scratch of at least strlen(text)+1). Words longer than
 * max_chars are hard-split. */
int wrap_lines(const char *text, int max_chars, int max_lines,
               char *scratch, const char **lines);

#ifdef __cplusplus
}
#endif

#endif
