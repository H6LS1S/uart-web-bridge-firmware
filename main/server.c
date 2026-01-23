#include <string.h>

#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "uart.h"

static const char *TAG = "[SERVER]";
static volatile int sse_fd = -1;

httpd_handle_t http_server = NULL;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
static const char sse_resp[] = "HTTP/1.1 200 OK\r\n"
		"Cache-Control: no-store\r\n"
		"Content-Type: text/event-stream\r\n"
		"\r\n"
		"retry: 20000\r\n"
		"\r\n";

static esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t err) {
	httpd_resp_set_status(req, "303 See Other");
	httpd_resp_set_hdr(req, "Location", "/");
	httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

esp_err_t index_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *) index_html_start,  index_html_end - index_html_start);
	return ESP_OK;
}

static esp_err_t stream_handler(httpd_req_t *req) {
	http_server = req->handle;
	sse_fd = httpd_req_to_sockfd(req);

	httpd_socket_send(http_server, sse_fd, sse_resp, sizeof(sse_resp) - 1, 0);
	return ESP_OK;
}

void server_send_sse(const uint8_t *data, size_t len) {
	if (sse_fd < 0 || http_server == NULL) return;

	httpd_socket_send(http_server, sse_fd, "data: ", 8, 0);
	httpd_socket_send(http_server, sse_fd, (const char *) data, len, 0);
	httpd_socket_send(http_server, sse_fd, "\r\n", 2, 0);
}

static esp_err_t send_handler(httpd_req_t *req) {
	char buf[256];
	const int ret = httpd_req_recv(req, buf, sizeof(buf));

	if (ret > 0) uart_write_data((uint8_t *) buf, ret);

	httpd_resp_send(req, NULL, 0);
	return ESP_OK;
}

static esp_err_t ota_handler(httpd_req_t *req) {
	esp_ota_handle_t update_handle = 0;
	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

	if (update_partition == NULL) {
		ESP_LOGE(TAG, "OTA partition not found!");
		return ESP_FAIL;
	}

	if (req->content_len <= 0) {
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File is empty");
		return ESP_FAIL;
	}

	esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "Starting OTA. Size: %d bytes", req->content_len);

	char *buf = malloc(1024);
	int remaining = req->content_len;
	while (remaining > 0) {
		int recv_len = httpd_req_recv(req, buf, MIN(remaining, 1024));
		if (recv_len <= 0) {
			if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) continue;
			esp_ota_abort(update_handle);
			free(buf);
			return ESP_FAIL;
		}

		esp_ota_write(update_handle, buf, recv_len);
		remaining -= recv_len;
	}
	free(buf);

	if (esp_ota_end(update_handle) != ESP_OK || esp_ota_set_boot_partition(update_partition) != ESP_OK) {
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Finalize Failed");
		return ESP_FAIL;
	}

	httpd_resp_sendstr(req, "Update applied! Rebooting...");
	vTaskDelay(pdMS_TO_TICKS(1000));
	esp_restart();
	return ESP_OK;
}

void server_start(void) {
	esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
	esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
	esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.max_uri_handlers = 16;
	config.lru_purge_enable = true;

	uart_init();

	if (httpd_start(&http_server, &config) != ESP_OK) return;

	ESP_LOGI(TAG, "Registering URI handlers");

	httpd_register_err_handler(http_server, HTTPD_404_NOT_FOUND, not_found_handler);

	httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler};
	httpd_register_uri_handler(http_server, &index_uri);

	httpd_uri_t stream_uri = {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler};
	httpd_register_uri_handler(http_server, &stream_uri);

	httpd_uri_t send_uri = {.uri = "/send", .method = HTTP_POST, .handler = send_handler};
	httpd_register_uri_handler(http_server, &send_uri);

	httpd_uri_t ota_uri = {.uri = "/ota", .method = HTTP_POST, .handler = ota_handler};
	httpd_register_uri_handler(http_server, &ota_uri);

	ESP_LOGI(TAG, "HTTP server started");
}
