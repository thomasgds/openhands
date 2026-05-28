#include "dropbear_sock.h"
#include "kernel_log.h"
int dropbear_sock_init(void) { LOG_INFO("dropbear_sock initialized"); return 0; }
void dropbear_sock_start(void) { LOG_INFO("dropbear_sock started"); }
void dropbear_sock_stop(void) { LOG_INFO("dropbear_sock stopped"); }
