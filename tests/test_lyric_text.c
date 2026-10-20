/* lyric_text helper tests. Compile: gcc -Iinclude tests/test_lyric_text.c
 * source/lyrics/lyric_text.c -o /tmp/test_lyric_text && /tmp/test_lyric_text */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "lyric_text.h"

static int checks = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } checks++; } while (0)

int main(void) {
    /* 1. UTF-8 decode: ascii + 2-byte + 3-byte */
    {
        const char *p = "A\xC3\xA9\xE2\x80\x99"; /* A + e-acute + right quote */
        CHECK(utf8_decode(&p) == 0x41);
        CHECK(utf8_decode(&p) == 0xE9);
        CHECK(utf8_decode(&p) == 0x2019);
        CHECK(*p == '\0');
    }
    /* 2. French accents survive as Latin-1 fold targets */
    {
        const char *p = "\xC3\xA8\xC3\xAA\xC3\xA7"; /* e-grave e-circ c-cedilla */
        CHECK(utf8_decode(&p) == 0xE8);
        CHECK(utf8_decode(&p) == 0xEA);
        CHECK(utf8_decode(&p) == 0xE7);
    }
    /* 3. display_fold: punctuation folds, CJK -> '?' */
    {
        CHECK(display_fold(0xE9) == 0xE9);       /* latin-1 passthrough */
        CHECK(display_fold(0x2019) == 0x27);     /* ' */
        CHECK(display_fold(0x201C) == 0x22);     /* " */
        CHECK(display_fold(0x2014) == 0x2D);     /* - */
        CHECK(display_fold(0x2026) == 0x2E);     /* . */
        CHECK(display_fold(0x4E2D) == 0x3F);     /* CJK -> ? */
        CHECK(display_fold(0x1F600) == 0x3F);    /* emoji -> ? */
    }
    /* 4. wrap: short text stays one line */
    {
        char scratch[256];
        const char *lines[4];
        int n = wrap_lines("hello world", 28, 3, scratch, lines);
        CHECK(n == 1);
        CHECK(strcmp(lines[0], "hello world") == 0);
    }
    /* 5. wrap: long line breaks at word boundary */
    {
        char scratch[256];
        const char *lines[4];
        int n = wrap_lines("aaa bbb ccc ddd eee fff", 11, 4, scratch, lines);
        CHECK(n == 2);
        CHECK(strcmp(lines[0], "aaa bbb ccc") == 0);
        CHECK(strcmp(lines[1], "ddd eee fff") == 0);
    }
    /* 6. wrap: max_lines cap, rest dropped */
    {
        char scratch[256];
        const char *lines[2];
        int n = wrap_lines("aaa bbb ccc ddd eee", 7, 2, scratch, lines);
        CHECK(n == 2);
        CHECK(strcmp(lines[0], "aaa bbb") == 0);
        /* second line fits what it can */
        CHECK(lines[1][0] != '\0');
    }
    /* 7. wrap: overlong single word hard-splits on codepoints, UTF-8 safe */
    {
        char scratch[256];
        const char *lines[4];
        int n = wrap_lines("abcdefghij", 4, 4, scratch, lines);
        CHECK(n >= 2);
        CHECK(strlen(lines[0]) == 4);
        /* multibyte: "caf\xC3\xA9" + long word must not split mid-sequence */
        n = wrap_lines("caf\xC3\xA9t\xC3\xA9riaExtraLong", 8, 4, scratch, lines);
        CHECK(n >= 1);
        /* first line must be valid UTF-8: re-decode fully */
        const char *q = lines[0];
        int cps = 0;
        while (*q) { (void)utf8_decode(&q); cps++; }
        CHECK(cps == 8);
    }
    /* 8. wrap: empty / NULL */
    {
        char scratch[64];
        const char *lines[2];
        CHECK(wrap_lines("", 10, 2, scratch, lines) == 1);
        CHECK(wrap_lines(NULL, 10, 2, scratch, lines) == 1);
        CHECK(wrap_lines("abc", 0, 2, scratch, lines) == 0);
    }
    /* 9. wrap: long lyric string wraps completely without truncating */
    {
        char scratch[512];
        const char *lines[8];
        const char *long_lyric = "Et comme c'est chaud, ils feront tchi, et comme c'est chaud, ils feront tchi";
        int n = wrap_lines(long_lyric, 28, 8, scratch, lines);
        CHECK(n == 3);
        CHECK(strcmp(lines[0], "Et comme c'est chaud, ils") == 0);
        CHECK(strcmp(lines[1], "feront tchi, et comme c'est") == 0);
        CHECK(strcmp(lines[2], "chaud, ils feront tchi") == 0);
    }
    printf("test_lyric_text: %d checks passed\n", checks);
    return 0;
}
