#include <wifi_provisioning/scheme_softap.h>

#include "wifi_provisioning/manager.h"
#include "esp_wifi.h"
#include "esp_log.h"

#include "mdns.h"

#include "server.h"

#define WIFI_CONNECTED_EVENT BIT0

static const char *TAG = "[WIFI]";
static EventGroupHandle_t wifi_event_group;

static void provisioning_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	if (event_base == WIFI_PROV_EVENT) {
		switch (event_id) {
			case WIFI_PROV_START:
				ESP_LOGI(TAG, "Provisioning started");
				break;
			case WIFI_PROV_CRED_RECV: {
				wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *) event_data;
				ESP_LOGI(TAG, "Received Wi-Fi credentials"
						"\n\tSSID     : %s\n\tPassword : %s",
						(const char *) wifi_sta_cfg->ssid,
						(const char *) wifi_sta_cfg->password);
				break;
			}
			case WIFI_PROV_CRED_FAIL:
				wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *) event_data;
				ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
						"\n\tPlease reset to factory and retry provisioning",
						(*reason == WIFI_PROV_STA_AUTH_ERROR) ?
						"Wi-Fi station authentication failed" : "Wi-Fi access-point not found");

				wifi_prov_mgr_reset_sm_state_on_failure();
				break;
			case WIFI_PROV_CRED_SUCCESS:
				ESP_LOGI(TAG, "Provisioning successful");
				break;
			case WIFI_PROV_END:
				wifi_prov_mgr_deinit();
				break;
			default:
				break;
		}
	}
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	if (event_base == WIFI_EVENT)
		switch (event_id) {
		case WIFI_EVENT_STA_START:
				esp_wifi_connect();
				break;
		case WIFI_EVENT_STA_DISCONNECTED:
				ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
				esp_wifi_connect();
				break;
		case WIFI_EVENT_AP_STACONNECTED:
				ESP_LOGI(TAG, "SoftAP transport: Connected!");
				break;
		case WIFI_EVENT_AP_STADISCONNECTED:
				ESP_LOGI(TAG, "SoftAP transport: Disconnected!");
				break;
		default:
				break;
		}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
		ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
		xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
	}
}

void wifi_start(void) {
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &provisioning_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

	esp_netif_create_default_wifi_sta();
	esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	bool provisioned = false;
	wifi_prov_mgr_config_t config = {
		.scheme = wifi_prov_scheme_softap,
		.app_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
		.scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
	};
	ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
	ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

	mdns_init();
	mdns_hostname_set("uart");
	mdns_instance_name_set("ESP32 UART Bridge");

	ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));

	server_start();

	if (provisioned) {
		ESP_LOGI(TAG, "Already provisioned, connecting to STA...");
		wifi_prov_mgr_deinit();

		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
		ESP_ERROR_CHECK(esp_wifi_start());
	} else {
		ESP_LOGI(TAG, "Not provisioned, starting SoftAP provisioning...");

		wifi_prov_scheme_softap_set_httpd_handle(&http_server);
		wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_0, NULL, "UART_Bridge", NULL); // secure=true для шифрування
	}
}
