// Stand-in for the sender's `record` console command (host test of tools/record.py).
//   fake_sender in.wav [drop_seq] [corrupt_seq]  < commands  > console output
// Waits for "record <s>", then prints exactly what the firmware prints (dbn_rec_line), using the
// WAV samples as the "mic". Optional: leave out one hop / corrupt one line to test the host side.
#include "dbn_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: fake_sender in.wav [drop_seq] [corrupt_seq]\n"); return 2; }
    long drop = argc > 2 ? atol(argv[2]) : -1, corrupt = argc > 3 ? atol(argv[3]) : -1;
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    fseek(f, 44, SEEK_SET);                       /* plain 44-byte header (as written by Python's wave) */
    char cmd[128];
    int secs = 0;
    while (fgets(cmd, sizeof(cmd), stdin)) {
        if (sscanf(cmd, " record %d", &secs) == 1) break;
        printf("\n");
        fflush(stdout);
    }
    if (secs <= 0) return 1;
    int frames = secs * 16000 / 512;
    printf("record %d\nREC-BEGIN fs=16000 bits=16 frames=%d hop=512\n", secs, frames);
    printf("I (1234) sender: a log line in between\n");
    static int16_t pcm[512];
    static char line[DBN_REC_LINE_MAX(sizeof(pcm))];
    for (int s = 0; s < frames; s++) {
        memset(pcm, 0, sizeof(pcm));
        if (fread(pcm, 2, 512, f) == 0) { /* past the end: zeros */ }
        if (s == drop) continue;
        size_t n = dbn_rec_line((uint32_t)s, pcm, sizeof(pcm), line);
        if (s == corrupt) line[n / 2] = line[n / 2] == 'A' ? 'B' : 'A';
        fwrite(line, 1, n, stdout);
    }
    printf("REC-END frames=%d dropped=%d\n", frames - (drop >= 0), drop >= 0);
    fflush(stdout);
    fclose(f);
    return 0;
}
