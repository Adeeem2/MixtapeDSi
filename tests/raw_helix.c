/* Raw Helix reference loop (testwrap logic) vs problem file. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "mp3dec.h"

#define READBUF_SIZE (1024 * 16)

int main(int argc, char **argv) {
    static unsigned char readBuf[READBUF_SIZE];
    unsigned char *readPtr = readBuf;
    static short outBuf[2304];
    FILE *f = fopen(argv[1], "rb");
    int bytesLeft = 0, eof = 0, nFrames = 0, errs = 0;
    HMP3Decoder h = MP3InitDecoder();
    for (int iter = 0; iter < 20000; iter++) {
        if (bytesLeft < 2 * MAINBUF_SIZE && !eof) {
            memmove(readBuf, readPtr, bytesLeft);
            int n = fread(readBuf + bytesLeft, 1, READBUF_SIZE - bytesLeft, f);
            if (n < READBUF_SIZE - bytesLeft)
                memset(readBuf + bytesLeft + n, 0, READBUF_SIZE - bytesLeft - n);
            bytesLeft += n;
            readPtr = readBuf;
            if (n == 0) eof = 1;
        }
        int off = MP3FindSyncWord(readPtr, bytesLeft);
        if (off < 0) { printf("no sync, done\n"); break; }
        readPtr += off; bytesLeft -= off;
        int err = MP3Decode(h, &readPtr, &bytesLeft, outBuf, 0);
        if (err == 0) {
            MP3FrameInfo fi;
            MP3GetLastFrameInfo(h, &fi);
            if (nFrames < 5 || fi.outputSamps != 2304)
                printf("frame %d: out=%d ch=%d rate=%d br=%d\n",
                       nFrames, fi.outputSamps, fi.nChans, fi.samprate, fi.bitrate);
            nFrames++;
        } else {
            if (errs++ < 5) printf("err %d at frame %d\n", err, nFrames);
            if (errs > 20) { printf("too many errors, stop\n"); break; }
            /* mimic wrapper: skip a byte */
            readPtr++; bytesLeft--;
        }
        if (eof && bytesLeft <= 0) break;
    }
    printf("nFrames=%d errs=%d\n", nFrames, errs);
    return 0;
}
