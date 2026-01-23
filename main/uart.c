#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "uart.h"
#include "server.h"

#define UART_NUM       UART_NUM_1
#define UART_TX_PIN    9
#define UART_RX_PIN    8
#define UART_BUF_SIZE  1024

static const char *TAG = "[UART]";
static int current_baudrate = 115200;

static void uart_event_task(void *pvParameters) {
	uart_event_t event;
	uint8_t *data = malloc(UART_BUF_SIZE);

	while (1) {
		if (xQueueReceive((QueueHandle_t) pvParameters, &event, portMAX_DELAY)) {
			switch (event.type) {
				case UART_DATA:
					const int len = uart_read_bytes(UART_NUM, data, event.size, portMAX_DELAY);
					if (len > 0) server_send_sse(data, len);

					break;

				case UART_FIFO_OVF:
					ESP_LOGW(TAG, "UART FIFO overflow");
					uart_flush_input(UART_NUM);
					xQueueReset((QueueHandle_t)pvParameters);
					break;

				case UART_BUFFER_FULL:
					ESP_LOGW(TAG, "UART ring buffer full");
					uart_flush_input(UART_NUM);
					xQueueReset((QueueHandle_t)pvParameters);
					break;

				case UART_FRAME_ERR:
					ESP_LOGW(TAG, "UART frame error");
					break;

				default:
					break;
			}
		}
	}
	free(data);
}

void uart_init(void) {
	const uart_config_t uart_config = {
		.baud_rate = current_baudrate,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};

	ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE * 2, 0, 20, &uart_queue, 0));
	ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

	xTaskCreate(uart_event_task, "uart_event_task", 4096, (void *) uart_queue, 12, NULL);

	ESP_LOGI(TAG, "UART initialized at %d baud", current_baudrate);
}

void uart_write_data(const uint8_t *data, size_t len) {
	uart_write_bytes(UART_NUM, data, len);
}

void uart_change_baudrate(int baudrate) {
	current_baudrate = baudrate;
	ESP_ERROR_CHECK(uart_set_baudrate(UART_NUM, baudrate));
	ESP_LOGI(TAG, "Baudrate changed to %d", baudrate);
}

QueueHandle_t uart_queue;
