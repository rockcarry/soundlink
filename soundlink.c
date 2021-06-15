#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "wavfile.h"
#include "wavdev.h"
#include "fft.h"
#include "soundlink.h"

/*
8KHz 采样率，1000ms 分为 40 个片段，每个分段 25ms，200 个采样点
start byte + length byte + data bytes + 2 bytes checksum
 */

#define SOUNDLINK_MTU                255
#define SOUNDLINK_FFT_LEN            128
#define SOUNDLINK_FREQ_TO_IDX(freq) ((freq) * SOUNDLINK_FFT_LEN / 8000)
#define SOUNDLINK_IDX_TO_FREQ(idx ) ( 8000 * (idx) / SOUNDLINK_FFT_LEN)
#define SOUNDLINK_MIN_FREQ_IDX       SOUNDLINK_FREQ_TO_IDX(500)
#define SOUNDLINK_MAX_FREQ_IDX      (SOUNDLINK_MIN_FREQ_IDX + 15)
#define SOUNDLINK_START_CODE_IDX    (SOUNDLINK_MIN_FREQ_IDX + 17)
#define LOW_NIBBLE_TO_FREQ(byte)     SOUNDLINK_IDX_TO_FREQ(SOUNDLINK_MIN_FREQ_IDX + (((byte) >> 0) & 0xF))
#define HIGH_NIBBLE_TO_FREQ(byte)    SOUNDLINK_IDX_TO_FREQ(SOUNDLINK_MIN_FREQ_IDX + (((byte) >> 4) & 0xF))

enum {
    STATE_IDLE = 0,
    STATE_RECV,
};

typedef struct {
    void    *wavfile;
    void    *wavdev ;
    void    *fft    ;
    int      state  ;
    int      check  ;
    int16_t  pcmbuf[50];
    int      pcmnum;
    uint8_t  recvbuf[1 + SOUNDLINK_MTU + 2];
    int      recvnum;
    int      nibble_counter[16];
    int      nibble_recvnum;
    PFN_SOUNDLINK_CALLBACK callback;
    void                  *cbctxt;
} SOUNDLINK;

static void gen_sin_wav(int16_t *pcm, int n, int samprate, int freq)
{
    int i; for (i=0; i<n; i++) pcm[i] = 32760 * sin(i * 2 * M_PI * freq / samprate);
}

static void wavein_callback_proc(void *ctxt, void *buf, int len)
{
    SOUNDLINK *sl = (SOUNDLINK*)ctxt;
    if (sl) {
        int16_t *srcbuf = (int16_t*)buf;
        int      srcnum =  len / sizeof(int16_t);
        while (srcnum > 0) {
            int ncopy = srcnum < (50 - sl->pcmnum) ? srcnum : (50 - sl->pcmnum);
            memcpy(sl->pcmbuf + sl->pcmnum, srcbuf, ncopy * sizeof(int16_t));
            srcbuf     += ncopy;
            srcnum     -= ncopy;
            sl->pcmnum += ncopy;

            if (sl->pcmnum == 50) {
                float    fft_buf[SOUNDLINK_FFT_LEN * 2] = {0}, maxamp = 0;
                int      freqidx = 0, i;
                for (i = 0; i < 50; i++) fft_buf[i * 2] = sl->pcmbuf[i];
                fft_execute(sl->fft, fft_buf, fft_buf);
                for (i = 0; i < SOUNDLINK_FFT_LEN / 2; i++) {
                    float curamp = fft_buf[i * 2 + 0] * fft_buf[i * 2 + 0] + fft_buf[i * 2 + 1] * fft_buf[i * 2 + 1];
                    if (maxamp < curamp) {
                        maxamp = curamp;
                        freqidx= i;
                    }
                }
                sl->pcmnum = 0;

                // check start code
                if (sl->check <= 2) {
                    if (freqidx == SOUNDLINK_START_CODE_IDX) sl->check += 1;
                    else sl->check = 0;
                } else if (sl->check == 3) {
                    if (freqidx != SOUNDLINK_START_CODE_IDX) {
                        sl->state   = STATE_RECV;
                        sl->check   = 0;
                        sl->recvnum = 0;
                        sl->nibble_recvnum = 0;
                    }
                }

                // recv data
                if (sl->state == STATE_RECV) {
                    if (freqidx) { printf("max amp freq idx: %d\n", freqidx); fflush(stdout); }
                    if (++sl->nibble_recvnum < 4) {
                        int nibble = freqidx - SOUNDLINK_MIN_FREQ_IDX;
                        if (nibble >= 0 && nibble <= 15) sl->nibble_counter[nibble]++;
                    } else {
                        int max = 0, nibble = 0;
                        for (i=0; i<16; i++) {
                            if (max < sl->nibble_counter[i]) {
                                max = sl->nibble_counter[i];
                                nibble = i;
                            }
                            sl->nibble_counter[i] = 0;
                        }
                        sl->nibble_recvnum = 0;
                        if ((sl->recvnum & 1) == 0) {
                            sl->recvbuf[sl->recvnum++ / 2] = nibble << 0;
                        } else {
                            sl->recvbuf[sl->recvnum++ / 2]|= nibble << 4;
                        }
                        if (sl->recvnum >= 3 * 2 && sl->recvnum == (1 + sl->recvbuf[0] + 2) * 2) {
                            uint16_t checksum = 0;
                            for (i = 0; i < 1 + sl->recvbuf[0]; i++) checksum += sl->recvbuf[i];
                            if (checksum == ((sl->recvbuf[1 + sl->recvbuf[0] + 0] << 0) | (sl->recvbuf[1 + sl->recvbuf[0] + 1] << 8))) {
                                if (sl->callback) sl->callback(sl->cbctxt, (char*)sl->recvbuf + 1, sl->recvbuf[0]);
                            }
                            sl->state = STATE_IDLE;
                        }
                    }
                    if (sl->recvnum == (1 + SOUNDLINK_MTU + 2) * 2) {
                        sl->state = STATE_IDLE;
                    }
                }
            }
        }
    }
}

void* soundlink_init(void)
{
    SOUNDLINK *sl = calloc(1, sizeof(SOUNDLINK));
    if (sl) {
        sl->wavfile = wavfile_create(8000, 1, (1 + 1 + SOUNDLINK_MTU + 2) * 2 * 250);
        sl->wavdev  = wavdev_init(8000, 1, 8000, 1, wavein_callback_proc, sl);
        sl->fft     = fft_init(128);
        if (!sl->wavfile || !sl->wavdev || !sl->fft) { soundlink_exit(sl); sl = NULL; }
    }
    return sl;
}

void soundlink_exit(void *ctxt)
{
    SOUNDLINK *sl = (SOUNDLINK*)ctxt;
    if (sl) {
        wavfile_free(sl->wavfile);
        wavdev_exit (sl->wavdev );
        fft_free    (sl->fft    );
        free(sl);
    }
}

int soundlink_send(void *ctxt, char *buf, int len, char *dst)
{
    int        n = len < SOUNDLINK_MTU ? len : SOUNDLINK_MTU;
    uint16_t   checksum, i;
    int16_t   *pcmbuf = NULL;
    SOUNDLINK *sl     = (SOUNDLINK*)ctxt;
    if (!ctxt || !buf || len < 0) {
        printf("soundlink_send invalid params, ctxt: %p, buf: %p, len: %d !\n", ctxt, buf, len);
        return -1;
    }

    // generate start code
    wavfile_getval(sl->wavfile, "buffer_pointer", &pcmbuf);
    gen_sin_wav(pcmbuf, 200, 8000, SOUNDLINK_IDX_TO_FREQ(SOUNDLINK_START_CODE_IDX)); pcmbuf += 200;
    gen_sin_wav(pcmbuf, 200, 8000, SOUNDLINK_IDX_TO_FREQ(SOUNDLINK_START_CODE_IDX)); pcmbuf += 200;

    // generate length
    gen_sin_wav(pcmbuf, 200, 8000, LOW_NIBBLE_TO_FREQ( n)); pcmbuf += 200;
    gen_sin_wav(pcmbuf, 200, 8000, HIGH_NIBBLE_TO_FREQ(n)); pcmbuf += 200;

    // generate data
    for (checksum = n, i = 0; i < n; i++) {
        gen_sin_wav(pcmbuf, 200, 8000, LOW_NIBBLE_TO_FREQ( buf[i])); pcmbuf += 200;
        gen_sin_wav(pcmbuf, 200, 8000, HIGH_NIBBLE_TO_FREQ(buf[i])); pcmbuf += 200;
        checksum += buf[i];
    }

    // generate checksum
    gen_sin_wav(pcmbuf, 200, 8000, LOW_NIBBLE_TO_FREQ( (checksum >> 0) & 0xFF)); pcmbuf += 200;
    gen_sin_wav(pcmbuf, 200, 8000, HIGH_NIBBLE_TO_FREQ((checksum >> 0) & 0xFF)); pcmbuf += 200;
    gen_sin_wav(pcmbuf, 200, 8000, LOW_NIBBLE_TO_FREQ( (checksum >> 8) & 0xFF)); pcmbuf += 200;
    gen_sin_wav(pcmbuf, 200, 8000, HIGH_NIBBLE_TO_FREQ((checksum >> 8) & 0xFF)); pcmbuf += 200;

    if (strcmp(dst, "wavdev") == 0) {
        wavfile_getval(sl->wavfile, "buffer_pointer", &pcmbuf);
        wavdev_play (sl->wavdev , pcmbuf, (1 + 1 + n + 2) * 2 * 200 * sizeof(int16_t));
    } else {
        wavfile_save(sl->wavfile, dst   , (1 + 1 + n + 2) * 2 * 200 * sizeof(int16_t));
    }
    return n;
}

int soundlink_recv(void *ctxt, char *src, PFN_SOUNDLINK_CALLBACK callback, void *cbctxt)
{
    SOUNDLINK *sl = (SOUNDLINK*)ctxt;
    if (sl) {
        sl->callback = callback;
        sl->cbctxt   = cbctxt;
        if (src == NULL) {
            wavdev_record(sl->wavdev, 0);
        } else if (strcmp(src, "wavdev") == 0) {
            wavdev_record(sl->wavdev, 1);
        } else {
            void *wf = wavfile_load(src);
            if (wf) {
                int16_t *pcm_buf = NULL;
                int32_t  pcm_len = 0, i;
                wavfile_getval(wf, "buffer_pointer", &pcm_buf);
                wavfile_getval(wf, "buffer_size"   , &pcm_len);
                for (i = 0; i < pcm_len / sizeof(int16_t); i += 200) {
                    wavein_callback_proc(sl, pcm_buf + i, 200 * sizeof(int16_t));
                }
                wavfile_free(wf);
            }
        }
    }
    return 0;
}

#ifdef _TEST_SOUNDLINK_
static void soundlink_callback_proc(void *cbctxt, char *buf, int len)
{
    printf("recv data: %s, len: %d\n", buf, len);
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    char *msg = "hello world !\n";
    char *file= "test.wav";
    void *sl  = soundlink_init();

    if (argc >= 2) msg = argv[1];
    if (argc >= 3) file= argv[2];
    if (argc >= 2) {
        soundlink_send(sl, msg, strlen(msg) + 1, file);
        goto done;
    }

    while (1) {
        char cmd [256];
        char msg [256];
        char name[256];
        scanf("%256s", cmd);
        if (strcmp(cmd, "send") == 0) {
            scanf("%256s %256s", msg, name);
            soundlink_send(sl, msg, strlen(msg), name);
        } else if (strcmp(cmd, "recv_start") == 0) {
            scanf("%256s", name);
            soundlink_recv(sl, name, soundlink_callback_proc, NULL);
        } else if (strcmp(cmd, "recv_stop") == 0) {
            soundlink_recv(sl, NULL, NULL, NULL);
        } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            break;
        }
    }

done:
    soundlink_exit(sl);
    return 0;
}
#endif
