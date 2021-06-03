#ifndef __FFT_H__
#define __FFT_H__

#ifdef __cplusplus
extern "C" {
#endif

void *fft_init   (int len);
void  fft_free   (void *c);
void  fft_execute(void *c, float *in, float *out);

#ifdef __cplusplus
}
#endif

#endif
