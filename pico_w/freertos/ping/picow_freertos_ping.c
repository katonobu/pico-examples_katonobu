/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/ip4_addr.h"

#include "FreeRTOS.h"
#include "task.h"
#include "lwip/dns.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/http_client.h"
#include "ping.h"

#ifndef RUN_FREERTOS_ON_CORE
#define RUN_FREERTOS_ON_CORE 0
#endif

#define TEST_TASK_PRIORITY				( tskIDLE_PRIORITY + 1UL )

static ip_addr_t ping_ip = IPADDR4_INIT_BYTES(142,251,35,196); // pin test ip
//static ip_addr_t mqtt_ip = IPADDR4_INIT_BYTES(34,77,13,55);
static ip_addr_t mqtt_ip = IPADDR4_INIT_BYTES(172,16,82,232);
static mqtt_client_t* mqtt_client;
static unsigned char msg_buff[256];

enum mqtt_stt_t {
    STT_AFTER_RESET,
    STT_NET_CONNECTING,
    STT_MQTT_CONNECTING,
    STT_MQTT_CONNECTED,
    STT_MQTT_DISCONNECTED,
};

static const struct mqtt_connect_client_info_t mqtt_client_info =
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

static void
mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
  const struct mqtt_connect_client_info_t* client_info = (const struct mqtt_connect_client_info_t*)arg;
  LWIP_PLATFORM_DIAG(("MQTT client \"%s\" data cb: len %d, flags %d\n", client_info->client_id,
          (int)len, (int)flags));
  memcpy(msg_buff, data, len);
  msg_buff[len] = '\0';
  LWIP_PLATFORM_DIAG(("Rx:%s\n",msg_buff));
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

static err_t headers_done_fn(httpc_state_t *connection, void *arg,
                             struct pbuf *hdr, u16_t hdr_len, u32_t content_len)
{
    printf("in headers_done_fn\n");
    return ERR_OK;
}

static void result_fn(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err)
{
    printf(">>> result_fn >>>\n");
    printf("httpc_result: %s\n",
             httpc_result == HTTPC_RESULT_OK              ? "HTTPC_RESULT_OK"
           : httpc_result == HTTPC_RESULT_ERR_UNKNOWN     ? "HTTPC_RESULT_ERR_UNKNOWN"
           : httpc_result == HTTPC_RESULT_ERR_CONNECT     ? "HTTPC_RESULT_ERR_CONNECT"
           : httpc_result == HTTPC_RESULT_ERR_HOSTNAME    ? "HTTPC_RESULT_ERR_HOSTNAME"
           : httpc_result == HTTPC_RESULT_ERR_CLOSED      ? "HTTPC_RESULT_ERR_CLOSED"
           : httpc_result == HTTPC_RESULT_ERR_TIMEOUT     ? "HTTPC_RESULT_ERR_TIMEOUT"
           : httpc_result == HTTPC_RESULT_ERR_SVR_RESP    ? "HTTPC_RESULT_ERR_SVR_RESP"
           : httpc_result == HTTPC_RESULT_ERR_MEM         ? "HTTPC_RESULT_ERR_MEM"
           : httpc_result == HTTPC_RESULT_LOCAL_ABORT     ? "HTTPC_RESULT_LOCAL_ABORT"
           : httpc_result == HTTPC_RESULT_ERR_CONTENT_LEN ? "HTTPC_RESULT_ERR_CONTENT_LEN"
           : "*UNKNOWN*");
    printf("received %ld bytes\n", rx_content_len);
    printf("server response: %ld\n", srv_res);
    printf("err: %d\n", err);
    printf("<<< result_fn <<<\n");
}

static err_t recv_fn(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    bool * http_done_p = (bool *)arg;
    printf(">>> recv_fn >>>\n");
    if (p == NULL) {
        printf("p is NULL\n");
    } else {
        printf("p: %p\n", p);
        printf("next: %p\n", p->next);
        printf("payload: %p\n", p->payload);
        printf("len: %d\n", p->len);
    }
    printf("<<< recv_fn <<<\n");
    *http_done_p = true;
    return ERR_OK;
}

static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    bool *dns_resolved_p = (bool *)arg;
    if (ipaddr) {
        printf("address %s\n", ip4addr_ntoa(ipaddr));
    } else {
        printf("dns request failed\n");
    }
    *dns_resolved_p = true;
}

void main_task(__unused void *params) {
    update_mqtt_stt(STT_NET_CONNECTING);
    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return;
    }
    cyw43_arch_enable_sta_mode();
    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect.\n");
        exit(1);
    } else {
        printf("Connected.\n");
    }

    ////////////////// DNS address resolve 
    cyw43_arch_lwip_begin();
    ip_addr_t dns_ip_address;
    bool dns_resolved = false;
    int dns_err = dns_gethostbyname("public.cloud.shiftr.io", &dns_ip_address, dns_found, &dns_resolved);
    cyw43_arch_lwip_end();
    printf("return of dns_gethostbyname = %d\n", dns_err);
    if (dns_err == ERR_OK) {
        printf("ntp address %s\n", ip4addr_ntoa(&dns_ip_address));
    }
    printf("Waiting for DNS resolve\n");
    while (!dns_resolved) {
        vTaskDelay(10);
    }

    ////////////////// http get
    httpc_connection_t settings = {
            .use_proxy = 0,
            .headers_done_fn = headers_done_fn,
            .result_fn = result_fn
    };
    httpc_state_t *connection = NULL;
    bool http_done = false;

    cyw43_arch_lwip_begin();
    err_t err = httpc_get_file_dns("neverssl.com", HTTP_DEFAULT_PORT, "/", &settings, recv_fn, &http_done, &connection);
    cyw43_arch_lwip_end();
    printf("err = %d\n", err);

    printf("Waiting for http request done\n");
    while (!http_done) {
        vTaskDelay(10);
    }


//    ping_init_with_blocking(&mqtt_ip, 10);

    update_mqtt_stt(STT_MQTT_CONNECTING);

    mqtt_client = mqtt_client_new();

    mqtt_set_inpub_callback(mqtt_client,
        mqtt_incoming_publish_cb,
        mqtt_incoming_data_cb,
        LWIP_CONST_CAST(void*, &mqtt_client_info));

    cyw43_arch_lwip_begin();
    mqtt_client_connect(mqtt_client,
        &mqtt_ip, MQTT_PORT,
        mqtt_connection_cb, LWIP_CONST_CAST(void*, &mqtt_client_info),
        &mqtt_client_info);
    cyw43_arch_lwip_end();

    uint32_t running_count = 0;
    uint32_t wait_sec = 60;
    while(running_count < 10 * wait_sec) {
        vTaskDelay(100);
        running_count++;
    }
    printf("%d sec has passed\n", wait_sec);

    mqtt_disconnect(mqtt_client);
    printf("MQTT disconnect\n");
    update_mqtt_stt(STT_MQTT_DISCONNECTED);
    cyw43_arch_deinit();
    printf("cyw43_arch_deinit\n");
    printf("------------------------------------\n");
    while(1){
        vTaskDelay(100);
    }
}

void led_task(__unused void *params) {
    printf("LED Task started\n");
    user_info.led_interval = 50;
    while(true) {
        vTaskDelay(user_info.led_interval);
        if (1 < user_info.led_interval) {
            user_info.led_stt = !user_info.led_stt;
        } else {
            user_info.led_stt = false;
        }
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, user_info.led_stt);
    }
}

void vLaunch( void) {
    TaskHandle_t main_task_handle;
    TaskHandle_t led_task_handle;
    xTaskCreate(main_task, "TestMainThread", configMINIMAL_STACK_SIZE, NULL, TEST_TASK_PRIORITY, &main_task_handle);
    xTaskCreate(led_task, "LedThread", configMINIMAL_STACK_SIZE, NULL, TEST_TASK_PRIORITY+1, &led_task_handle);

#if NO_SYS && configUSE_CORE_AFFINITY && configNUM_CORES > 1
    // we must bind the main task to one core (well at least while the init is called)
    // (note we only do this in NO_SYS mode, because cyw43_arch_freertos
    // takes care of it otherwise)
    vTaskCoreAffinitySet(task, 1);
#endif

    /* Start the tasks and timer running. */
    vTaskStartScheduler();
}

int main( void )
{
    stdio_init_all();

    /* Configure the hardware ready to run the demo. */
    const char *rtos_name;
#if ( portSUPPORT_SMP == 1 )
    rtos_name = "FreeRTOS SMP";
#else
    rtos_name = "FreeRTOS";
#endif

#if ( portSUPPORT_SMP == 1 ) && ( configNUM_CORES == 2 )
    printf("Starting %s on both cores:\n", rtos_name);
    vLaunch();
#elif ( RUN_FREE_RTOS_ON_CORE == 1 )
    printf("Starting %s on core 1:\n", rtos_name);
    multicore_launch_core1(vLaunch);
    while (true);
#else
    printf("Starting %s on core 0:\n", rtos_name);
    vLaunch();
#endif
    return 0;
}
