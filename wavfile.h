#ifndef __WAVFILE_H__
#define __WAVFILE_H__

void* wavfile_load  (char *file);
void* wavfile_create(int samprate, int chnum, int duration);
void  wavfile_free  (void *ctxt);
int   wavfile_save  (void *ctxt, char *file, int size);
int   wavfile_getval(void *ctxt, char *name, void *val); // name: "sample_rate", "channel_num", "duration", "buffer_pointer", "buffer_size"

#endif
