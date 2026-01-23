#ifndef SERVER_H
#define SERVER_H

#include "esp_http_server.h"

extern httpd_handle_t http_server;

void server_start(void);
void server_send_sse(const uint8_t *data, size_t len);

#endif