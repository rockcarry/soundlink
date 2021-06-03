#ifndef __SOUNDLINK_H__
#define __SOUNDLINK_H__

void* soundlink_init(void);
void  soundlink_exit(void *ctxt);
int   soundlink_send(void *ctxt, char *buf, int len, char *dst);
int   soundlink_recv(void *ctxt, char *buf, int len);

#endif

