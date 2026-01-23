#include <string.h>

#include "esp_wifi.h"
#include <esp_timer.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

#include "wifi.h"

#define BOOT_BUTTON_GPIO GPIO_NUM_9

static esp_timer_handle_t reset_timer;
static void IRAM_ATTR button_isr_handler(void *arg) {
	if (gpio_get_level(BOOT_BUTTON_GPIO)) esp_timer_stop(reset_timer);
	else esp_timer_start_once(reset_timer, 10000000);
}

void reset_timer_callback(void *arg) {
	nvs_flash_erase();
	esp_restart();
}

void app_main(void) {
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ESP_ERROR_CHECK(nvs_flash_init());
	}

	const esp_timer_create_args_t timer_args = {.callback = &reset_timer_callback, .name = "reset_timer"};
	esp_timer_create(&timer_args, &reset_timer);

	gpio_config_t button_config = {
		.pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.intr_type = GPIO_INTR_ANYEDGE
	};

	gpio_config(&button_config);
	gpio_install_isr_service(0);
	gpio_isr_handler_add(BOOT_BUTTON_GPIO, button_isr_handler, NULL);

	wifi_start();
}
