/*
Author:     BuiKhanhHoang
E-mail:     buikhanhhoang2003@gmail.com
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem_api.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "lwip/lwip_napt.h"
#include "esp_mac.h"
#include "dhcpserver/dhcpserver.h"
#include "esp_random.h"

#include "gw_modem.h"

#define WIFI_CHANNEL       1
#define MAX_STA_CONN       4
#define RANDOM_STR_LEN     8

static const char *TAG = "hotspot";

// Generate a random alphanumeric string
static void gen_random_string(char *output, size_t len) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (size_t i = 0; i < len - 1; i++) {
        output[i] = charset[esp_random() % (sizeof(charset) - 1)];
    }
    output[len - 1] = '\0';
}

static void gen_random_mac(uint8_t *mac) {
    esp_fill_random(mac, 6);
    mac[0] &= 0xFE; // clear multicast bit
    mac[0] |= 0x02; // set local admin bit
}

/**********************************************
 *   SECTION: ESP-IDF API based implementations
 **********************************************/

// Set DNS for DHCP server offered to connected clients
static esp_err_t set_dhcps_dns(esp_netif_t *netif, uint32_t addr) {
    esp_netif_dns_info_t dns;
    dns.ip.u_addr.ip4.addr = addr;
    dns.ip.type = IPADDR_TYPE_V4;

    dhcps_offer_t dhcps_dns_value = OFFER_DNS;

    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET,  
                                            ESP_NETIF_DOMAIN_NAME_SERVER,
                                            &dhcps_dns_value,
                                            sizeof(dhcps_dns_value)));

    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));  
    return ESP_OK;
}

// WiFi event callback for station connect/disconnect
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG, "station "MACSTR" joined, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG, "station "MACSTR" left, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

// Initialize SoftAP with random SSID/PASS and MAC
void wifi_init_softap(char *ssid, char *pass, uint8_t *mac) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));                            
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,  
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));               
    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_AP, mac));             

    wifi_config_t wifi_config = {
        .ap = {
            .channel = WIFI_CHANNEL,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(ssid);

    if (strlen(pass) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        strncpy((char *)wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));  
    ESP_ERROR_CHECK(esp_wifi_start());                              

    ESP_LOGI(TAG, "SoftAP started - SSID:%s PASS:%s MAC:"MACSTR, ssid, pass, MAC2STR(mac));
}

/**********************************************
 *                  APP MAIN
 **********************************************/
void app_main(void) {
    esp_log_level_set("*", ESP_LOG_INFO);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();                              
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());                      
        ret = nvs_flash_init();                                    
    }
    ESP_ERROR_CHECK(ret);

    char ssid[RANDOM_STR_LEN];
    char pass[RANDOM_STR_LEN];
    uint8_t mac[6];

    gen_random_string(ssid, RANDOM_STR_LEN);
    gen_random_string(pass, RANDOM_STR_LEN);
    gen_random_mac(mac);
    pcbartists_modem_power_up_por();
    ESP_ERROR_CHECK(esp_netif_init());                         
    ESP_ERROR_CHECK(esp_event_loop_create_default());            
    ESP_ERROR_CHECK(esp_netif_set_default_wifi_ap_handlers());     

    // Setup modem and wait for connection
    pcbartists_modem_setup();
    ESP_LOGI(TAG, "Waiting for modem IP...");
    xEventGroupWaitBits(pcbartists_modem_eventgroup(), MODEM_CONNECT_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    // Access Point
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();    // << ESP-IDF API call >>
    esp_netif_t *modem_netif = pcbartists_get_modem_netif();
    assert(modem_netif);

    // dns
    esp_netif_dns_info_t dns;
    ESP_ERROR_CHECK(esp_netif_get_dns_info(modem_netif, ESP_NETIF_DNS_MAIN, &dns)); // << ESP-IDF API call >>
    set_dhcps_dns(ap_netif, dns.ip.u_addr.ip4.addr);

    // Launch WiFi AP and NAT
    wifi_init_softap(ssid, pass, mac);
    ip_napt_enable(_g_esp_netif_soft_ap_ip.ip.addr, 1);           

    ESP_LOGW(TAG, "4G Hotspot is now active");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep 1s
    }

    pcbartists_modem_deinit(); // never reached
}
