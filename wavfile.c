#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "wavfile.h"

#pragma pack(1)
typedef struct {
    char     chunk_id[4];
    uint32_t chunk_size;
    char     riff_fmt[4];
} RIFF_HEADER;

typedef struct {
    uint16_t format_tag;
    uint16_t channels;
    uint32_t sample_per_sec;
    uint32_t avgbyte_per_sec;
    uint16_t block_align;
    uint16_t bits_per_sample;
} WAVE_FORMAT;

typedef struct {
    char     fmt_id[4];
    uint32_t fmt_size;
    WAVE_FORMAT wav_format;
} FMT_BLOCK;

typedef struct {
    char     data_id[4];
    uint32_t data_size;
} DATA_BLOCK;
#pragma pack()

typedef struct {
    RIFF_HEADER header;
    FMT_BLOCK   format;
    DATA_BLOCK  datblk;
    void       *datbuf;
} WAVEFILE;

void* wavfile_load(char *file)
{
    WAVEFILE *wav = calloc(1, sizeof(WAVEFILE));
    if (wav) {
        FILE *fp = fopen(file, "rb");
        if (fp) {
            fread(wav, 1, sizeof(RIFF_HEADER) + sizeof(FMT_BLOCK) + sizeof(DATA_BLOCK), fp);
            wav->datbuf = malloc(wav->datblk.data_size);
            if (wav->datbuf) {
                fread(wav->datbuf, 1, wav->datblk.data_size, fp);
            } else {
                free(wav); wav = NULL;
            }
            fclose(fp);
        }
    }
    return wav;
}

void* wavfile_create(int samprate, int chnum, int duration)
{
    WAVEFILE *wav = calloc(1, sizeof(WAVEFILE));
    if (wav) {
        memcpy(wav->header.chunk_id, "RIFF", 4);
        memcpy(wav->header.riff_fmt, "WAVE", 4);
        memcpy(wav->format.fmt_id  , "fmt ", 4);
        memcpy(wav->datblk.data_id , "data", 4);
        wav->format.fmt_size = sizeof(WAVE_FORMAT);
        wav->format.wav_format.format_tag      = 1;
        wav->format.wav_format.channels        = chnum;
        wav->format.wav_format.sample_per_sec  = samprate;
        wav->format.wav_format.avgbyte_per_sec = samprate * chnum * sizeof(int16_t);
        wav->format.wav_format.block_align     = chnum * sizeof(int16_t);
        wav->format.wav_format.bits_per_sample = 16;
        wav->datblk.data_size                  = wav->format.wav_format.avgbyte_per_sec * duration / 1000;
        wav->header.chunk_size                 = sizeof(RIFF_HEADER) - 8 + sizeof(FMT_BLOCK) + sizeof(DATA_BLOCK) + wav->datblk.data_size;
        wav->datbuf                            = calloc(1, wav->datblk.data_size);
        if (!wav->datbuf) { free(wav); wav = NULL; }
    }
    return wav;
}

void wavfile_free(void *ctxt)
{
    WAVEFILE *wav = (WAVEFILE*)ctxt;
    if (!ctxt) return;
    free(wav->datbuf);
    free(wav);
}

int wavfile_save(void *ctxt, char *file, int size)
{
    FILE     *fp  = NULL;
    WAVEFILE *wav = (WAVEFILE*)ctxt;
    if (!ctxt) return -1;
    fp = fopen(file, "wb");
    if (fp) {
        RIFF_HEADER header = wav->header;
        DATA_BLOCK  datblk = wav->datblk;
        datblk.data_size   = size == 0 ? header.chunk_size : size;
        header.chunk_size  = size == 0 ? header.chunk_size : sizeof(RIFF_HEADER) - 8 + sizeof(FMT_BLOCK) + sizeof(DATA_BLOCK) + size;
        fwrite(&header     , 1, sizeof(header     ), fp);
        fwrite(&wav->format, 1, sizeof(wav->format), fp);
        fwrite(&datblk     , 1, sizeof(datblk     ), fp);
        fwrite( wav->datbuf, 1, datblk.data_size   , fp);
        fclose(fp);
    }
    return fp ? 0 : -1;
}

// name: "sample_rate", "channel_num", "duration", "buffer_pointer", "buffer_size"
int wavfile_getval(void *ctxt, char *name, void *val)
{
    WAVEFILE *wav = (WAVEFILE*)ctxt;
    int       ret = 0;
    if (!ctxt) return ret;
    if (strcmp(name, "sample_rate"   ) == 0) {
        *(int*)val = wav->format.wav_format.sample_per_sec;
    } else if (strcmp(name, "channel_num"   ) == 0) {
        *(int*)val = wav->format.wav_format.channels;
    } else if (strcmp(name, "duration"      ) == 0) {
        *(int*)val = wav->datblk.data_size * 1000 / wav->format.wav_format.avgbyte_per_sec;
    } else if (strcmp(name, "buffer_pointer") == 0) {
        *(void**)val = wav->datbuf;
    } else if (strcmp(name, "buffer_size"   ) == 0) {
        *(int*)val = wav->datblk.data_size;
    } else ret = -1;
    return ret;
}

#ifdef _TEST_WAVFILE_
#include <math.h>

static void gen_sin_wav(int16_t *pcm, int n, int samprate, int freq)
{
    int  i;
    for (i=0; i<n; i++) {
        pcm[i] = 32760 * sin(i * 2 * M_PI * freq / samprate);
    }
}

int main(int argc, char *argv[])
{
    char    *file = "test.wav";
    int      freq = 1000;
    void    *wav  = NULL;
    int16_t *pcm  = NULL;
    int      size = 0;

    if (argc >= 2) file = argv[1];
    if (argc >= 3) freq = atoi(argv[2]);
    freq = freq ? freq : 1000;
    wav = wavfile_create(8000, 1, 10000);
    wavfile_getval(wav, "buffer_pointer", &pcm );
    wavfile_getval(wav, "buffer_size"   , &size);
    gen_sin_wav(pcm, size / sizeof(int16_t), 8000, freq);
    wavfile_save(wav, file);
    wavfile_free(wav);
    return 0;
}
#endif
