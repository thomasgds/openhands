#include "sshd.h"
#include "kernel_log.h"
void sshd_init(void) { LOG_INFO("sshd initialized"); }
void sshd_start(void) { LOG_INFO("sshd started"); }
void sshd_stop(void) { LOG_INFO("sshd stopped"); }
