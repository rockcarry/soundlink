#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "wavfile.h"
#include "wavdev.h"
#include "soundlink.h"

/*
8KHz 采样率，1000ms 分为 40 个片段，每个分段 25ms，200 个采样点

261.63 293.66 329.63 349.23 392.00 440.00 493.88
                     0000   0001   0010   0011

523.25 587.33 659.26 698.46 783.99 880.00 987.77
0100   0101   0110   0111   1000   1001   1010

1046.5 1174.7 1318.5 1396.9 1568.0 1760.0 1975.5
1011   1100   1101   1110   1111

start byte + length byte + data bytes + 2 bytes checksum
 */

#define SOUNDLINK_MTU  255

static const int SL_FREQ_LIST[] = {
    262 , 294 , 330 , 349 , 392 , 440 , 494 ,
    523 , 587 , 659 , 698 , 784 , 880 , 988 ,
    1047, 1175, 1319, 1397, 1568, 1760, 1976,
};

typedef struct {
    void    *wavfile;
    void    *wavdev ;
    uint8_t  send_data_buf [(1 + 1 + SOUNDLINK_MTU + 2) * 1];
    int16_t  send_freq_data[(1 + 1 + SOUNDLINK_MTU + 2) * 2];
} SOUNDLINK;

static void gen_sin_wav(int16_t *pcm, int n, int samprate, int freq)
{
    int  i;
    for (i=0; i<n; i++) {
        pcm[i] = 32760 * sin(i * 2 * M_PI * freq / samprate);
    }
}

void* soundlink_init(void)
{
    SOUNDLINK *sl = calloc(1, sizeof(SOUNDLINK));
    if (sl) {
        sl->wavfile = wavfile_create(8000, 1, (1 + 1 + SOUNDLINK_MTU + 2) * 2 * 250);
        sl->wavdev  = wavdev_init(8000, 1, 8000, 1);
        if (!sl->wavfile || !sl->wavdev) {
            wavfile_free(sl->wavfile);
            wavdev_exit (sl->wavdev );
            free(sl); sl = NULL;
        }
    }
    return sl;
}

void soundlink_exit(void *ctxt)
{
    SOUNDLINK *sl = (SOUNDLINK*)ctxt;
    if (sl) {
        wavfile_free(sl->wavfile);
        wavdev_exit (sl->wavdev );
        free(sl);
    }
}

int soundlink_send(void *ctxt, char *buf, int len, char *dst)
{
    int n = len < SOUNDLINK_MTU ? len : SOUNDLINK_MTU;
    uint16_t checksum = 0, i;
    int16_t   *pcmbuf = NULL;
    SOUNDLINK *sl     = (SOUNDLINK*)ctxt;
    if (!ctxt || !buf || len < 0) {
        printf("ctxt: %p, buf: %p, len: %d !\n", ctxt, buf, len);
        return -1;
    }

    sl->send_data_buf[0] = 'S';
    sl->send_data_buf[1] =  n ;
    memcpy(sl->send_data_buf + 2, buf, n);
    for (i=0; i<n+2; i++) checksum += sl->send_data_buf[i];
    sl->send_data_buf[n + 2 + 0] = (uint8_t)(checksum >> 0);
    sl->send_data_buf[n + 2 + 1] = (uint8_t)(checksum >> 8);

    sl->send_freq_data[0] = SL_FREQ_LIST[20];
    sl->send_freq_data[1] = SL_FREQ_LIST[1 ];
    for (i=1; i<n+2; i++) {
        sl->send_freq_data[i * 2 + 0] = SL_FREQ_LIST[((sl->send_data_buf[i] >> 4) & 0xF) + 3];
        sl->send_freq_data[i * 2 + 1] = SL_FREQ_LIST[((sl->send_data_buf[i] >> 0) & 0xF) + 3];
    }

    wavfile_getval(sl->wavfile, "buffer_pointer", &pcmbuf);
    for (i = 0; i<(1 + 1 + n + 2) * 2; i++) {
        gen_sin_wav(pcmbuf + i * 200 * sizeof(int16_t), 200, 8000, sl->send_freq_data[i]);
    }

    if (strcmp(dst, "wavdev") == 0) {
        wavdev_play (sl->wavdev , pcmbuf, (1 + 1 + n + 2) * 2 * 200 * sizeof(int16_t));
    } else {
        wavfile_save(sl->wavfile, dst   , (1 + 1 + n + 2) * 2 * 200 * sizeof(int16_t));
    }
    return n;
}

int soundlink_recv(void *ctxt, char *buf, int len)
{
    return 0;
}

#ifdef _TEST_SOUNDLINK_
int main(int argc, char *argv[])
{
    char *msg = "hello world !\n";
    char *file= "test.wav";
    void *sl  = soundlink_init();
    if (argc >= 2) msg = argv[1];
    if (argc >= 3) file= argv[2];
    soundlink_send(sl, msg, strlen(msg) + 1, file);
    soundlink_exit(sl);
    return 0;
}
#endif
