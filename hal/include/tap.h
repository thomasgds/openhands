#ifndef TAP_H
#define TAP_H
int tap_open(const char *dev);
int tap_read(int fd, void *buf, int len);
int tap_write(int fd, const void *buf, int len);
int tap_set_addr(int fd, const char *addr);
#endif
