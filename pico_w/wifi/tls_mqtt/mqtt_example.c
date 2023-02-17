/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "lwip/ip4_addr.h"

#include "lwip/dns.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/http_client.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"

#ifndef RUN_FREERTOS_ON_CORE
#define RUN_FREERTOS_ON_CORE 0
#endif

#define TEST_TASK_PRIORITY				( tskIDLE_PRIORITY + 1UL )

#define MSG_BUFF_LEN (256)

const static char * mqtt_url = "test.mosquitto.org";
static ip_addr_t mqtt_ip;
static mqtt_client_t* mqtt_client;
static unsigned char mqtt_msg_buff[MSG_BUFF_LEN];

const static u8_t host_cert[] = "\
-----BEGIN CERTIFICATE-----\n\
MIIEAzCCAuugAwIBAgIUBY1hlCGvdj4NhBXkZ/uLUZNILAwwDQYJKoZIhvcNAQEL\n\
BQAwgZAxCzAJBgNVBAYTAkdCMRcwFQYDVQQIDA5Vbml0ZWQgS2luZ2RvbTEOMAwG\n\
A1UEBwwFRGVyYnkxEjAQBgNVBAoMCU1vc3F1aXR0bzELMAkGA1UECwwCQ0ExFjAU\n\
BgNVBAMMDW1vc3F1aXR0by5vcmcxHzAdBgkqhkiG9w0BCQEWEHJvZ2VyQGF0Y2hv\n\
by5vcmcwHhcNMjAwNjA5MTEwNjM5WhcNMzAwNjA3MTEwNjM5WjCBkDELMAkGA1UE\n\
BhMCR0IxFzAVBgNVBAgMDlVuaXRlZCBLaW5nZG9tMQ4wDAYDVQQHDAVEZXJieTES\n\
MBAGA1UECgwJTW9zcXVpdHRvMQswCQYDVQQLDAJDQTEWMBQGA1UEAwwNbW9zcXVp\n\
dHRvLm9yZzEfMB0GCSqGSIb3DQEJARYQcm9nZXJAYXRjaG9vLm9yZzCCASIwDQYJ\n\
KoZIhvcNAQEBBQADggEPADCCAQoCggEBAME0HKmIzfTOwkKLT3THHe+ObdizamPg\n\
UZmD64Tf3zJdNeYGYn4CEXbyP6fy3tWc8S2boW6dzrH8SdFf9uo320GJA9B7U1FW\n\
Te3xda/Lm3JFfaHjkWw7jBwcauQZjpGINHapHRlpiCZsquAthOgxW9SgDgYlGzEA\n\
s06pkEFiMw+qDfLo/sxFKB6vQlFekMeCymjLCbNwPJyqyhFmPWwio/PDMruBTzPH\n\
3cioBnrJWKXc3OjXdLGFJOfj7pP0j/dr2LH72eSvv3PQQFl90CZPFhrCUcRHSSxo\n\
E6yjGOdnz7f6PveLIB574kQORwt8ePn0yidrTC1ictikED3nHYhMUOUCAwEAAaNT\n\
MFEwHQYDVR0OBBYEFPVV6xBUFPiGKDyo5V3+Hbh4N9YSMB8GA1UdIwQYMBaAFPVV\n\
6xBUFPiGKDyo5V3+Hbh4N9YSMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEL\n\
BQADggEBAGa9kS21N70ThM6/Hj9D7mbVxKLBjVWe2TPsGfbl3rEDfZ+OKRZ2j6AC\n\
6r7jb4TZO3dzF2p6dgbrlU71Y/4K0TdzIjRj3cQ3KSm41JvUQ0hZ/c04iGDg/xWf\n\
+pp58nfPAYwuerruPNWmlStWAXf0UTqRtg4hQDWBuUFDJTuWuuBvEXudz74eh/wK\n\
sMwfu1HFvjy5Z0iMDU8PUDepjVolOCue9ashlS4EB5IECdSR2TItnAIiIwimx839\n\
LdUdRudafMu5T5Xma182OC0/u/xRlEm+tvKGGmfFcN0piqVl8OrSPBgIlb+1IKJE\n\
m/XriWr/Cq4h/JfB7NTsezVslgkBaoU=\n\
-----END CERTIFICATE-----\n";

enum mqtt_stt_t {
    STT_AFTER_RESET,
    STT_NET_CONNECTING,
    STT_MQTT_CONNECTING,
    STT_MQTT_CONNECTED,
    STT_MQTT_DISCONNECTED,
};

static struct mqtt_connect_client_info_t mqtt_client_info =
{
    "RZPPW",
    NULL, /* user */
    NULL, /* pass */
    100,  /* keep alive */
    NULL, /* will_topic */
    NULL, /* will_msg */
    0,    /* will_qos */
    0     /* will_retain */
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    , NULL
#endif
};

struct mqtt_user_info_t {
    enum mqtt_stt_t mqtt_stt;
    uint32_t led_interval;
    bool led_stt;
};

static struct mqtt_user_info_t user_info;

static void update_mqtt_stt(enum mqtt_stt_t stt) {
    LWIP_PLATFORM_DIAG(("MQTT stt update from %d to %d\n", user_info.mqtt_stt, stt));
    user_info.mqtt_stt = stt;
    if (stt < STT_MQTT_CONNECTED) {
        user_info.led_interval = 50;
    } else if (stt == STT_MQTT_CONNECTED) {
        user_info.led_interval = 500;
    } else {
        user_info.led_interval = 2000;
    }
}

static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    bool *dns_resolved_p = (bool *)arg;
    if (ipaddr) {
        printf("address %s\n", ip4addr_ntoa(ipaddr));
    } else {
        printf("dns request failed\n");
    }
    mqtt_ip = *ipaddr;
    *dns_resolved_p = true;
}

static void
mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
  const struct mqtt_connect_client_info_t* client_info = (const struct mqtt_connect_client_info_t*)arg;
  LWIP_PLATFORM_DIAG(("MQTT client \"%s\" data cb: len %d, flags %d\n", client_info->client_id,
          (int)len, (int)flags));
  if (len < (MSG_BUFF_LEN - 1)) {
    memcpy(mqtt_msg_buff, data, len);
    mqtt_msg_buff[len] = '\0';
  } else {
    printf("--- message length is too long. expected %d < %d\n",len, MSG_BUFF_LEN - 1);
    memcpy(mqtt_msg_buff, data, (MSG_BUFF_LEN - 1));
    mqtt_msg_buff[MSG_BUFF_LEN - 1] = '\0';
  }
  LWIP_PLATFORM_DIAG(("Rx:%s\n",mqtt_msg_buff));
}

static void
mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
  const struct mqtt_connect_client_info_t* client_info = (const struct mqtt_connect_client_info_t*)arg;

  LWIP_PLATFORM_DIAG(("MQTT client \"%s\" publish cb: topic %s, len %d\n", client_info->client_id,
          topic, (int)tot_len));
}

static void
mqtt_request_cb(void *arg, err_t err)
{
  const struct mqtt_connect_client_info_t* client_info = (const struct mqtt_connect_client_info_t*)arg;

  LWIP_PLATFORM_DIAG(("MQTT client \"%s\" request cb: err %d\n", client_info->client_id, (int)err));
}

static void
mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
  const struct mqtt_connect_client_info_t* client_info = (const struct mqtt_connect_client_info_t*)arg;
  LWIP_UNUSED_ARG(client);

  LWIP_PLATFORM_DIAG(("MQTT client \"%s\" connection cb: status %d\n", client_info->client_id, (int)status));

  if (status == MQTT_CONNECT_ACCEPTED) {
    update_mqtt_stt(STT_MQTT_CONNECTED);
    mqtt_subscribe(client, "hello", 1, mqtt_request_cb, LWIP_CONST_CAST(void*, client_info));
    mqtt_publish(client, "hello", "01234567", 8, 1, 0, NULL, NULL);
  } else {
    update_mqtt_stt(STT_MQTT_DISCONNECTED);
    mqtt_unsubscribe(client, "hello", mqtt_request_cb, LWIP_CONST_CAST(void*, client_info));
  }
}

int main( void ) {
    stdio_init_all();

//    mbedtls_x509_crt_info(host_cert, sizeof(host_cert)

    update_mqtt_stt(STT_NET_CONNECTING);
    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect.\n");
        exit(1);
    } else {
        printf("Connected.\n");
    }

    printf("Start resolve %s\n",mqtt_url);
    cyw43_arch_lwip_begin();
    ip_addr_t dns_ip_address;
    bool dns_resolved = false;
    int dns_err = dns_gethostbyname(mqtt_url, &dns_ip_address, dns_found, &dns_resolved);
    cyw43_arch_lwip_end();
    printf("return of dns_gethostbyname = %d\n", dns_err);
    if (dns_err == ERR_OK) {
        mqtt_ip = dns_ip_address;
    } else {
        printf("Waiting for DNS resolve\n");
        while (!dns_resolved) {
          sleep_ms(10);
        }
    }
    printf("Resolved %s as %s\n", mqtt_url, ip4addr_ntoa(&mqtt_ip));


    update_mqtt_stt(STT_MQTT_CONNECTING);

    mqtt_client = mqtt_client_new();

    mqtt_set_inpub_callback(mqtt_client,
        mqtt_incoming_publish_cb,
        mqtt_incoming_data_cb,
        LWIP_CONST_CAST(void*, &mqtt_client_info));
    
    mqtt_client_info.tls_config = altcp_tls_create_config_client(host_cert, sizeof(host_cert));

    cyw43_arch_lwip_begin();
    mqtt_client_connect(mqtt_client,
        &mqtt_ip, 8883, // MQTT_PORT,
        mqtt_connection_cb, LWIP_CONST_CAST(void*, &mqtt_client_info),
        &mqtt_client_info);
    cyw43_arch_lwip_end();

    uint32_t running_count = 0;
    uint32_t wait_sec = 10;
    printf("start waiting for %d sec\n", wait_sec);
    while(running_count < 10 * wait_sec) {
        sleep_ms(100);
        running_count++;
    }
    printf("%d sec has passed\n", wait_sec);

    mqtt_disconnect(mqtt_client);
    printf("MQTT disconnect\n");
    update_mqtt_stt(STT_MQTT_DISCONNECTED);
    cyw43_arch_deinit();
    printf("cyw43_arch_deinit\n");
    printf("------------------------------------\n");
    printf("reboot...\n");
    reset_usb_boot(0,0);
    while(1){
        sleep_ms(100);
    }
}

