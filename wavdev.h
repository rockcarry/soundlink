#ifndef __WAVDEV_H__
#define __WAVDEV_H__

void* wavdev_init(int in_samprate, int in_chnum, int out_samprate, int out_chnum);
void  wavdev_exit(void *ctxt);
void  wavdev_play(void *ctxt, void *buf, int len);

#endif
