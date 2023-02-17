/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "lwip/ip4_addr.h"

#include "FreeRTOS.h"
#include "task.h"
#include "lwip/dns.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/http_client.h"

#ifndef RUN_FREERTOS_ON_CORE
#define RUN_FREERTOS_ON_CORE 0
#endif

#define TEST_TASK_PRIORITY				( tskIDLE_PRIORITY + 1UL )

const static char * http_url = "neverssl.com";

enum httpc_stt_t {
    STT_AFTER_RESET,
    STT_NET_CONNECTING,
    STT_HTTPC_CONNECTING,
    STT_HTTPC_REQUESTED,
    STT_HTTPC_HEADERS_DONE,
    STT_HTTPC_RECEIVING,
    STT_HTTPC_RESULT_DONE,
    STT_HTTPC_DISCONNECTED,
};

struct httpc_user_info_t {
    enum httpc_stt_t httpc_stt;
    uint32_t led_interval;
    bool led_stt;
};

static struct httpc_user_info_t user_info;

static void update_httpc_stt(enum httpc_stt_t stt) {
    LWIP_PLATFORM_DIAG(("httpc stt update from %d to %d\n", user_info.httpc_stt, stt));
    user_info.httpc_stt = stt;
    if (stt < STT_HTTPC_CONNECTING) {
        user_info.led_interval = 50;
    } else if (stt < STT_HTTPC_DISCONNECTED) {
        user_info.led_interval = 100;
    } else {
        user_info.led_interval = 2000;
    }
}

static err_t headers_done_fn(httpc_state_t *connection, void *arg,
                             struct pbuf *hdr, u16_t hdr_len, u32_t content_len)
{
    update_httpc_stt(STT_HTTPC_HEADERS_DONE);
    printf("in headers_done_fn\n");
    return ERR_OK;
}

static void result_fn(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err)
{
    update_httpc_stt(STT_HTTPC_RESULT_DONE);
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
    update_httpc_stt(STT_HTTPC_RECEIVING);
    printf(">>> recv_fn >>>\n");
    if (p == NULL) {
        printf("p is NULL\n");
    } else {
        printf("p: %p\n", p);
        printf("next: %p\n", p->next);
        printf("payload: %p\n", p->payload);
        printf("len: %d\n", p->len);
        printf("contents:%s\n",p->payload);
    }
    printf("<<< recv_fn <<<\n");
    *http_done_p = true;
    return ERR_OK;
}

void main_task(__unused void *params) {
    update_httpc_stt(STT_NET_CONNECTING);
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
    update_httpc_stt(STT_HTTPC_CONNECTING);

    ////////////////// http get
    httpc_connection_t settings = {
            .use_proxy = 0,
            .headers_done_fn = headers_done_fn,
            .result_fn = result_fn
    };
    httpc_state_t *connection = NULL;
    bool http_done = false;

    cyw43_arch_lwip_begin();
    err_t err = httpc_get_file_dns(http_url, HTTP_DEFAULT_PORT, "/", &settings, recv_fn, &http_done, &connection);
    cyw43_arch_lwip_end();
    printf("err = %d\n", err);

    update_httpc_stt(STT_HTTPC_REQUESTED);

    printf("Waiting for http request done\n");
    while (!http_done) {
        vTaskDelay(10);
    }

    update_httpc_stt(STT_HTTPC_DISCONNECTED);
    cyw43_arch_deinit();
    printf("cyw43_arch_deinit\n");
    printf("------------------------------------\n");
    printf("reboot...\n");
    reset_usb_boot(0,0);
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
