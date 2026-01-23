#ifndef UART_H
#define UART_H

#include "freertos/queue.h"

extern QueueHandle_t uart_queue;

void uart_init(void);
void uart_write_data(const uint8_t *data, size_t len);
void uart_change_baudrate(int baudrate);

#endif