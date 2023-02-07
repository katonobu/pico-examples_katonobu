/*
this appears as either a RNDIS or CDC-ECM USB virtual network adapter; the OS picks its preference

RNDIS should be valid on Linux and Windows hosts, and CDC-ECM should be valid on Linux and macOS hosts

The MCU appears to the host as IP address 192.168.7.1, and provides a DHCP server, DNS server, and web server.
*/
/*
Some smartphones *may* work with this implementation as well, but likely have limited (broken) drivers,
and likely their manufacturer has not tested such functionality.  Some code workarounds could be tried:

The smartphone may only have an ECM driver, but refuse to automatically pick ECM (unlike the OSes above);
try modifying ./examples/devices/net_lwip_webserver/usb_descriptors.c so that CONFIG_ID_ECM is default.

The smartphone may be artificially picky about which Ethernet MAC address to recognize; if this happens, 
try changing the first byte of tud_network_mac_address[] below from 0x02 to 0x00 (clearing bit 1).
*/

/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "lwip/apps/http_client.h"

#include "tinyusb_net_lwip.h"

//const static char * http_url = "neverssl.com";

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
    bool * http_done_p = (bool *)arg;
    *http_done_p = true;
}

static err_t recv_fn(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    printf(">>> recv_fn >>>\n");
    if (p == NULL) {
        printf("p is NULL\n");
    } else {
        printf("p: %p\n", p);
        printf("next: %p\n", p->next);
        printf("payload: %p\n", p->payload);
        printf("len: %d\n", p->len);
//        printf("contents:%s\n",p->payload);
    }
    printf("<<< recv_fn <<<\n");
    return ERR_OK;
}

int main(void)
{
  stdio_init_all();

  printf("tinyusb_net-httpc start.\n");
  printf("this is build at %s %s\n",__DATE__, __TIME__);

  tinyusb_arch_init();

  ////////////////// http get
  httpc_connection_t settings = {
          .use_proxy = 0,
          .headers_done_fn = headers_done_fn,
          .result_fn = result_fn
  };
  httpc_state_t *connection = NULL;
  bool http_done = false;

//  err_t err = httpc_get_file_dns(http_url, HTTP_DEFAULT_PORT, "/", &settings, recv_fn, &http_done, &connection);
//  err_t err = httpc_get_file_dns("34.223.124.45", HTTP_DEFAULT_PORT, "/", &settings, recv_fn, &http_done, &connection);
  err_t err = httpc_get_file_dns("192.168.7.2", 8000, "/", &settings, recv_fn, &http_done, &connection);

  printf("retun code of httpc_get_file_dns() : %d:%s\n", err, lwip_strerr(err));
  if (err == 0) {
    while (http_done == false)
    {
      tinyusb_net_lwip_transfer();
    }
  }
  tinyusb_arch_deinit();
  printf("------------------------------------\n");
  printf("reboot...\n");
  reset_usb_boot(0,0);
  while(1);
  return 0;
}
