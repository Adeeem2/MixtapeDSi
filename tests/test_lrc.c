/* LRC parser tests. Compile: gcc -Iinclude tests/test_lrc.c
 * source/lyrics/lrc_parser.c -o /tmp/test_lrc && /tmp/test_lrc */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "lrc_parser.h"

static int checks = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } checks++; } while (0)

int main(void) {
    LrcSong s;

    /* 1. standard tags + timestamps */
    lrc_parse("[ti:Tout donner]\n[ar:Naza & SDM]\n[al:Tout donner - Single]\n"
              "[length:02:25]\n[00:03.86]N-A-Z-A, tu connais le name\n"
              "[00:06.31]Another lyric line\n", &s);
    CHECK(strcmp(s.meta.title, "Tout donner") == 0);
    CHECK(strcmp(s.meta.artist, "Naza & SDM") == 0);
    CHECK(strcmp(s.meta.album, "Tout donner - Single") == 0);
    CHECK(s.meta.has_length && s.meta.length_ms == (2*60+25)*1000);
    CHECK(s.count == 2);
    CHECK(s.lines[0].timestamp_ms == 3860);
    CHECK(strcmp(s.lines[0].text, "N-A-Z-A, tu connais le name") == 0);
    CHECK(s.lines[1].timestamp_ms == 6310);

    /* 2. multiple timestamps one line */
    lrc_parse("[00:01.00][00:05.00]Hello\n", &s);
    CHECK(s.count == 2);
    CHECK(s.lines[0].timestamp_ms == 1000 && s.lines[1].timestamp_ms == 5000);
    CHECK(strcmp(s.lines[0].text, "Hello") == 0);

    /* 3. inline metadata + timestamp same physical line */
    lrc_parse("[ti:Song][00:10.00]Line one\n", &s);
    CHECK(strcmp(s.meta.title, "Song") == 0);
    CHECK(s.count == 1 && s.lines[0].timestamp_ms == 10000);

    /* 4. UTF-8 / French accents passthrough */
    lrc_parse("[00:01.00]C'est l'ete cafe creme\n", &s);
    CHECK(s.count == 1);
    CHECK(strstr(s.lines[0].text, "C'est") != NULL);

    /* 5. malformed ignored */
    lrc_parse("[00:01.00]ok\n[bad tag]\n[99:99.99]nope\nno brackets\n[00:02.00]ok2\n", &s);
    CHECK(s.count == 2);

    /* 6. unsorted input gets sorted */
    lrc_parse("[00:10.00]b\n[00:01.00]a\n", &s);
    CHECK(s.count == 2 && s.lines[0].timestamp_ms == 1000);
    CHECK(strcmp(s.lines[0].text, "a") == 0);

    /* 7. empty file */
    lrc_parse("", &s);
    CHECK(s.count == 0);

    /* 8. 3-digit millis + by/re/ve tags */
    lrc_parse("[by:DJ][re:Ed][ve:1.0]\n[01:23.456]x\n", &s);
    CHECK(strcmp(s.meta.by, "DJ") == 0);
    CHECK(s.lines[0].timestamp_ms == 83456);

    /* 9. repeated timestamps identical text */
    lrc_parse("[00:01.00]hey\n[00:01.00]hey\n", &s);
    CHECK(s.count == 2);

    /* 10. truncation bound */
    {
        static char big[32768];
        char *w = big;
        for (int i = 0; i < 400; i++)
            w += sprintf(w, "[%02d:%02d.00]line %d\n", (i/60)%60, i%60, i);
        lrc_parse(big, &s);
        CHECK(s.count == MAX_LYRIC_LINES && s.truncated == 1);
    }

    /* 11. blank lines + mm:ss without fraction */
    lrc_parse("\n\n[00:05]plain seconds\n", &s);
    CHECK(s.count == 1 && s.lines[0].timestamp_ms == 5000);

    printf("test_lrc: ALL %d checks passed\n", checks);
    return 0;
}
