#pragma once

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool network_ready;

void network_init(void);
void udp_server_thread(void *arg1, void *arg2, void *arg3);
void udp_server_start(void); // start the UDP server thread (creates it internally)

extern int udp_sock;

#ifdef __cplusplus
}
#endif
