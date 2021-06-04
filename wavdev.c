#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include "wavdev.h"

#define WAVEIN_BUF_NUM  5
typedef struct {
    HWAVEIN  hWaveIn ;
    WAVEHDR  sWaveInHdr[WAVEIN_BUF_NUM];
    HWAVEOUT hWaveOut;
    WAVEHDR  sWaveOutHdr;
    #define FLAG_PLAYING   (1 << 0)
    #define FLAG_RECORDING (1 << 1)
    DWORD    dwFlags;
    PFN_WAVEIN_CALLBACK callback;
    void               *cbctxt  ;
} WAVDEV;

static BOOL CALLBACK waveInProc(HWAVEIN hWav, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    WAVDEV  *dev = (WAVDEV *)dwInstance;
    WAVEHDR *phdr= (WAVEHDR*)dwParam1;

    switch (uMsg) {
    case WIM_DATA:
        if (dev->callback) dev->callback(dev->cbctxt, phdr->lpData, phdr->dwBytesRecorded);
        waveInAddBuffer(hWav, phdr, sizeof(WAVEHDR));
        break;
    }
    return TRUE;
}

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    WAVDEV *dev = (WAVDEV*)dwInstance;
    switch (uMsg) {
    case WOM_DONE:
        waveOutUnprepareHeader(dev->hWaveOut, &dev->sWaveOutHdr, sizeof(WAVEHDR));
        dev->dwFlags &= ~FLAG_PLAYING;
        break;
    }
}

void* wavdev_init(int in_samprate, int in_chnum, int out_samprate, int out_chnum, PFN_WAVEIN_CALLBACK callback, void *cbctxt)
{
    int     inbufsize = in_samprate * sizeof(int16_t) * in_chnum / 40;
    WAVDEV *dev = calloc(1, sizeof(WAVDEV) + WAVEIN_BUF_NUM * inbufsize);
    if (dev) {
        WAVEFORMATEX wfx = {0};
        DWORD        result;
        int          i;
        wfx.cbSize         = sizeof(wfx);
        wfx.wFormatTag     = WAVE_FORMAT_PCM;
        wfx.wBitsPerSample = 16;
        wfx.nSamplesPerSec = in_samprate;
        wfx.nChannels      = in_chnum;
        wfx.nBlockAlign    = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec= wfx.nBlockAlign * wfx.nSamplesPerSec;
        result = waveInOpen(&dev->hWaveIn, WAVE_MAPPER, &wfx, (DWORD_PTR)waveInProc, (DWORD_PTR)dev, CALLBACK_FUNCTION);
        if (result != MMSYSERR_NOERROR) {
            wavdev_exit(dev); dev = NULL;
        }
        for (i=0; i<WAVEIN_BUF_NUM; i++) {
            dev->sWaveInHdr[i].dwBufferLength = inbufsize;
            dev->sWaveInHdr[i].lpData         = (LPSTR)dev + sizeof(WAVDEV) + i * inbufsize;
            waveInPrepareHeader(dev->hWaveIn, &dev->sWaveInHdr[i], sizeof(WAVEHDR));
            waveInAddBuffer    (dev->hWaveIn, &dev->sWaveInHdr[i], sizeof(WAVEHDR));
        }

        wfx.cbSize          = sizeof(wfx);
        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.wBitsPerSample  = 16;
        wfx.nSamplesPerSec  = out_samprate;
        wfx.nChannels       = out_chnum;
        wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
        result = waveOutOpen(&dev->hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, (DWORD_PTR)dev, CALLBACK_FUNCTION);
        if (result != MMSYSERR_NOERROR) {
            wavdev_exit(dev); dev = NULL;
        }

        dev->callback = callback;
        dev->cbctxt   = cbctxt;
    }
    return dev;
}

void wavdev_exit(void *ctxt)
{
    WAVDEV *dev = (WAVDEV*)ctxt;
    if (dev) {
        int i;
        waveOutReset(dev->hWaveOut);
        waveOutClose(dev->hWaveOut);
        waveInStop  (dev->hWaveIn );
        for (i=0; i<WAVEIN_BUF_NUM; i++) waveInUnprepareHeader(dev->hWaveIn, &dev->sWaveInHdr[i], sizeof(WAVEHDR));
        waveInClose (dev->hWaveIn );
        free(dev);
    }
}

void wavdev_play(void *ctxt, void *buf, int len)
{
    WAVDEV *dev = (WAVDEV*)ctxt;
    if (dev) {
        dev->dwFlags |= FLAG_PLAYING;
        dev->sWaveOutHdr.lpData         = buf;
        dev->sWaveOutHdr.dwBufferLength = len;
        waveOutPrepareHeader(dev->hWaveOut, &dev->sWaveOutHdr, sizeof(WAVEHDR));
        waveOutWrite(dev->hWaveOut, &dev->sWaveOutHdr, sizeof(WAVEHDR));
        while (dev->dwFlags & FLAG_PLAYING) Sleep(100);
    }
}

void wavdev_record(void *ctxt, int start)
{
    WAVDEV *dev = (WAVDEV*)ctxt;
    if (dev) {
        if (start) {
            if ((dev->dwFlags & FLAG_RECORDING) == 0) {
                dev->dwFlags |= FLAG_RECORDING;
                waveInStart(dev->hWaveIn);
            }
        } else {
            if ((dev->dwFlags & FLAG_RECORDING) != 0) {
                dev->dwFlags &=~FLAG_RECORDING;
                waveInStop (dev->hWaveIn);
            }
        }
    }
}
