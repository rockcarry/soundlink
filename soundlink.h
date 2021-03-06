#ifndef __SOUNDLINK_H__
#define __SOUNDLINK_H__

typedef void (*PFN_SOUNDLINK_CALLBACK)(void *cbctxt, char *buf, int len);

void* soundlink_init(void);
void  soundlink_exit(void *ctxt);
int   soundlink_send(void *ctxt, char *buf, int len, char *dst);
int   soundlink_recv(void *ctxt, char *src, PFN_SOUNDLINK_CALLBACK callback, void *cbctxt);

#endif

