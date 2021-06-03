#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

typedef struct {
    HWAVEOUT hWaveOut;
    WAVEHDR  sWaveHdr;
    #define FLAG_PLAYING (1 << 0)
    DWORD    dwFlags;
} WAVDEV;

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    WAVDEV *dev = (WAVDEV*)dwInstance;
    switch (uMsg) {
    case WOM_DONE:
        waveOutUnprepareHeader(dev->hWaveOut, &dev->sWaveHdr, sizeof(WAVEHDR));
        dev->dwFlags &= ~FLAG_PLAYING;
        break;
    }
}

void* wavdev_init(int in_samprate, int in_chnum, int out_samprate, int out_chnum)
{
    WAVDEV *dev = calloc(1, sizeof(WAVDEV));
    if (dev) {
        WAVEFORMATEX wfx = {0};
        DWORD        result;
        wfx.cbSize          = sizeof(wfx);
        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.wBitsPerSample  = 16;
        wfx.nSamplesPerSec  = out_samprate;
        wfx.nChannels       = out_chnum;
        wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
        result = waveOutOpen(&dev->hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, (DWORD_PTR)dev, CALLBACK_FUNCTION);
        if (result != MMSYSERR_NOERROR) {
            free(dev); dev = NULL;
        }
    }
    return dev;
}

void wavdev_exit(void *ctxt)
{
    WAVDEV *dev = (WAVDEV*)ctxt;
    if (dev) {
        waveOutReset(dev->hWaveOut);
        waveOutClose(dev->hWaveOut);
        free(dev);
    }
}

void wavdev_play(void *ctxt, void *buf, int len)
{
    WAVDEV *dev = (WAVDEV*)ctxt;
    if (dev) {
        dev->dwFlags |= FLAG_PLAYING;
        dev->sWaveHdr.lpData         = buf;
        dev->sWaveHdr.dwBufferLength = len;
        waveOutPrepareHeader(dev->hWaveOut, &dev->sWaveHdr, sizeof(WAVEHDR));
        waveOutWrite(dev->hWaveOut, &dev->sWaveHdr, sizeof(WAVEHDR));
        while (dev->dwFlags & FLAG_PLAYING) Sleep(100);
    }
}

