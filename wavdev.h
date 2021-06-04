#ifndef __WAVDEV_H__
#define __WAVDEV_H__

typedef void (*PFN_WAVEIN_CALLBACK)(void *ctxt, void *buf, int len);
void* wavdev_init  (int in_samprate, int in_chnum, int out_samprate, int out_chnum, PFN_WAVEIN_CALLBACK callback, void *cbctxt);
void  wavdev_exit  (void *ctxt);
void  wavdev_play  (void *ctxt, void *buf, int len);
void  wavdev_record(void *ctxt, int start);

#endif
