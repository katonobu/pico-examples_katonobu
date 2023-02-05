// MIT License
//
// Copyright (c) 2023 Nobuo Kato (katonobu4649@gmail.com)
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "bsp/board.h"
#include "tusb.h"

#include "dhserver.h"
#include "dnserver.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/netif.h"
#include "tinyusb_net_lwip.h"

/* network parameters of this MCU */
static const ip_addr_t ipaddr  = IPADDR4_INIT_BYTES(192, 168, 7, 1);
static const ip_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
static const ip_addr_t gateway = IPADDR4_INIT_BYTES(0, 0, 0, 0);

/* database IP addresses that can be offered to the host; this must be in RAM to store assigned MAC addresses */
static dhcp_entry_t entries[] =
{
    /* mac ip address                          lease time */
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 2), 24 * 60 * 60 },
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 3), 24 * 60 * 60 },
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 4), 24 * 60 * 60 },
};

static const dhcp_config_t dhcp_config_local = {
    .router = IPADDR4_INIT_BYTES(0, 0, 0, 0),  /* router address (if any) */
    .port = 67,                                /* listen port */
    .dns = IPADDR4_INIT_BYTES(192, 168, 7, 1), /* dns server (if any) */
    "usb",                                     /* dns suffix */
    TU_ARRAY_SIZE(entries),                    /* num entry */
    entries                                    /* entries */
};

typedef struct tinyusb_net_lwip{
  struct netif netif;
  const dhcp_config_t *dhcp_config_p;
} tinyusb_net_lwip_t;

tinyusb_net_lwip_t tinyusb_state = {
  .netif = {},
  .dhcp_config_p = &dhcp_config_local
};

/* shared between tud_network_recv_cb() and service_traffic() */
static struct pbuf *received_frame;

// toDo: get from unique id
// pico/pico-sdk_katonobu/lib/tinyusb/src/class/net/net_device.h
// client must provide this: 48-bit MAC address
const uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};

static void tinyusb_init(tinyusb_net_lwip_t *self, bool up);
static void tinyusb_deinit(tinyusb_net_lwip_t *self);
static void tinyusb_tcpip_init(tinyusb_net_lwip_t * self);
static void tinyusb_tcpip_deinit(tinyusb_net_lwip_t * self);
static err_t tinyusb_netif_init(struct netif *netif);
static err_t linkoutput_fn(struct netif *netif, struct pbuf *p);
static void lwip_status_callback(struct netif *netif);
static void lwip_link_callback(struct netif *netif);
static void lwip_remove_callback(struct netif *netif);
static bool dns_query_proc(const char *name, ip_addr_t *addr);

void tinyusb_net_lwip_transfer(void)
{
  tud_task();
  /* handle any packet received by tud_network_recv_cb() */
  if (received_frame)
  {
    ethernet_input(received_frame, &tinyusb_state.netif);
    pbuf_free(received_frame);
    received_frame = NULL;
    tud_network_recv_renew();
  }
  sys_check_timeouts();
}

/////////////////////////////////////////////////////
// -------------- arch 
/////////////////////////////////////////////////////
// pico/pico-sdk_katonobu/src/rp2_common/pico_cyw43_arch/cyw43_arch_poll.c
int tinyusb_arch_init(void) {
  static bool done_lwip_init;
  if (!done_lwip_init) {
      lwip_init();
      done_lwip_init = true;
  }
  tinyusb_init(&tinyusb_state, true);
  return 0;
}

void tinyusb_arch_deinit(void) {
  tinyusb_deinit(&tinyusb_state);
}

/* lwip has provision for using a mutex, when applicable */
sys_prot_t sys_arch_protect(void)
{
  return 0;
}
void sys_arch_unprotect(sys_prot_t pval)
{
  (void)pval;
}

/* lwip needs a millisecond time source, and the TinyUSB board support code has one available */
uint32_t sys_now(void)
{
  return board_millis();
}

/////////////////////////////////////////////////////
// -------------- driver
/////////////////////////////////////////////////////
// pico/pico-sdk_katonobu/lib/cyw43-driver/src/cyw43_ctrl.c
static void tinyusb_init(tinyusb_net_lwip_t *self, bool up) {
  // initialize TinyUSB
  tusb_init();
  if (up) {
    tinyusb_tcpip_deinit(self);
    tinyusb_tcpip_init(self);
  }
}

static void tinyusb_deinit(tinyusb_net_lwip_t *self) {
    tinyusb_tcpip_deinit(self);
}

// pico/pico-sdk_katonobu/lib/cyw43-driver/src/cyw43_lwip.c
static void tinyusb_tcpip_init(tinyusb_net_lwip_t * self) {
    struct netif *netif = &(self->netif);
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif_add(netif, &ipaddr, &netmask, &gateway, &tinyusb_state, tinyusb_netif_init, ip_input);
    netif_set_hostname(netif, "RZPP");

    netif_set_default(netif);
    netif_set_up(netif);

    netif_set_status_callback(netif, lwip_status_callback);
    netif_set_remove_callback(netif, lwip_remove_callback);
    netif_set_link_callback(netif, lwip_link_callback);

#ifdef DHCPC
    self->dhcp_config = &dhcp_config;
    dns_setserver(0, &dnsaddr);
    dhcp_set_struct(netif, &self->dhcp_client);
    dhcp_start(netif);
#else
    err_t ret_val = ERR_OK;
    ret_val = dhserv_init(self->dhcp_config_p);
    if (ret_val == ERR_OK){
      ret_val = dnserv_init(&ipaddr, 53, dns_query_proc);
      if (ret_val != ERR_OK){
        printf("dnsserver init fail\n");
      }
    } else {
      printf("dhcpserver init fail\n");
    }
#endif
}

static void tinyusb_tcpip_deinit(tinyusb_net_lwip_t * self){
#ifdef DHCPC
    struct netif *netif = &self->netif;
    dhcp_stop(netif);
#endif
}

/////////////////////////////////////////////////////
// callbacks called from lwip
/////////////////////////////////////////////////////

// err_t cyw43_netif_init(struct netif *netif)
static err_t tinyusb_netif_init(struct netif *netif)
{
  err_t ret_val = ERR_OK;

  printf("IP:%d.%d.%d.%d\n", ip4_addr1_val(ipaddr), ip4_addr2_val(ipaddr), ip4_addr3_val(ipaddr), ip4_addr4_val(ipaddr));

  LWIP_ASSERT("netif != NULL", (netif != NULL));
  netif->linkoutput = linkoutput_fn;
  netif->output = etharp_output;
  netif->mtu = CFG_TUD_NET_MTU;
  netif_set_flags(netif, NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP);

  /* the lwip virtual MAC address must be different from the host's; to ensure this, we toggle the LSbit */
  netif->hwaddr_len = sizeof(tud_network_mac_address);
  memcpy(netif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
  netif->hwaddr[5] ^= 0x01;

  return ret_val;
}

static err_t linkoutput_fn(struct netif *netif, struct pbuf *p)
{
  (void)netif;

  for (;;)
  {
    /* if TinyUSB isn't ready, we must signal back to lwip that there is nothing we can do */
    if (!tud_ready())
      return ERR_USE;

    /* if the network driver can accept another packet, we make it happen */
    if (tud_network_can_xmit(p->tot_len))
    {
      tud_network_xmit(p, 0 /* unused for this example */);
      return ERR_OK;
    }

    /* transfer execution to TinyUSB in the hopes that it will finish transmitting the prior packet */
    tud_task();
  }
}

static void lwip_status_callback(struct netif *netif) {
  printf("status\n");
}

static void lwip_link_callback(struct netif *netif) {
  printf("link : ", netif->flags);
  if (netif->flags == 0x0F) {
    ip4_addr_debug_print(LWIP_DBG_ON,&netif->ip_addr);
  }
  printf("\n");
}

static void lwip_remove_callback(struct netif *netif) {
  printf("removed\n");
}

/////////////////////////////////////////////////////
/* handle any DNS requests from dns-server */
/////////////////////////////////////////////////////
static bool dns_query_proc(const char *name, ip_addr_t *addr)
{
  if (0 == strcmp(name, "tiny.usb"))
  {
    *addr = ipaddr;
    return true;
  }
  return false;
}

/////////////////////////////////////////////////////
// callbacks called from tiny usb
/////////////////////////////////////////////////////
// pico/pico-sdk_katonobu/lib/tinyusb/src/class/net/net_device.h

// client must provide this: return false if the packet buffer was not accepted
bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
  /* this shouldn't happen, but if we get another packet before 
  parsing the previous, we must signal our inability to accept it */
  if (received_frame) return false;

  if (size)
  {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);

    if (p)
    {
      /* pbuf_alloc() has already initialized struct; all we need to do is copy the data */
      memcpy(p->payload, src, size);

      /* store away the pointer for service_traffic() to later handle */
      received_frame = p;
    }
  }

  return true;
}

// client must provide this: copy from network stack packet pointer to dst
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
  struct pbuf *p = (struct pbuf *)ref;

  (void)arg; /* unused for this example */

  return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

// client must provide this: initialize any network state back to the beginning
void tud_network_init_cb(void)
{
  // This may called only link up, can't detect link down

  /* if the network is re-initializing and we have a leftover packet, we must do a cleanup */
  if (received_frame)
  {
    pbuf_free(received_frame);
    received_frame = NULL;
  }
  netif_set_link_up(&(tinyusb_state.netif));
}
