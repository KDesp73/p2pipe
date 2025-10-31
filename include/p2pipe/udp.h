#ifndef UDP_H
#define UDP_H

#include <stdio.h>
int server(int port);

/*
 * Send a UDP "HANDSHAKE <payload>" to server_ip:server_port and wait
 * for a reply. On success, the received reply is written into out_buf
 * (null-terminated) and the function returns the number of bytes received
 * (not including the trailing NUL). On error or timeout, returns -1.
 *
 * timeout_sec: how many seconds to wait for each reply before retransmit.
 * max_retries: number of retransmits; if 0 => try forever.
 */
ssize_t udp_handshake_blocking(const char *server_ip,
                               unsigned short server_port,
                               const char *payload,
                               char *out_buf,
                               size_t out_buf_size,
                               unsigned timeout_sec,
                               unsigned max_retries,
                               unsigned short client_port,
                               int *out_sock);

unsigned short get_available_udp_port(void);

#endif // UDP_H
