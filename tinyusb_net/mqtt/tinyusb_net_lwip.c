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

/* IP configuration */
static const dhcp_config_t *dhcp_config_p;
static const ip_addr_t* ipaddr_p;
static const ip_addr_t* netmask_p;
static const ip_addr_t* gateway_p;

/* lwip context */
static struct netif netif_data;

/* shared between tud_network_recv_cb() and service_traffic() */
static struct pbuf *received_frame;

// toDo: MACアドレス、ユニークIDから取ってくる。
const uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};

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

static err_t output_fn(struct netif *netif, struct pbuf *p, const ip_addr_t *addr)
{
  return etharp_output(netif, p, addr);
}

static err_t netif_init_cb(struct netif *netif)
{
  err_t ret_val = ERR_OK;

  LWIP_ASSERT("netif != NULL", (netif != NULL));
  netif->mtu = CFG_TUD_NET_MTU;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
  netif->state = NULL;
  netif->name[0] = 'E';
  netif->name[1] = 'X';
  netif->linkoutput = linkoutput_fn;
  netif->output = output_fn;

  return ret_val;
}

static void init_lwip(void)
{
  struct netif *netif = &netif_data;

  lwip_init();

  // toDo: MACアドレス、ユニークIDから取ってくる。
  /* the lwip virtual MAC address must be different from the host's; to ensure this, we toggle the LSbit */
  netif->hwaddr_len = sizeof(tud_network_mac_address);
  memcpy(netif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
  netif->hwaddr[5] ^= 0x01;

  netif = netif_add(netif, ipaddr_p, netmask_p, gateway_p, NULL, netif_init_cb, ip_input);
  netif_set_default(netif);
}

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

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
  struct pbuf *p = (struct pbuf *)ref;

  (void)arg; /* unused for this example */

  return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

void tinyusb_net_lwip_transfer(void)
{
  tud_task();
  /* handle any packet received by tud_network_recv_cb() */
  if (received_frame)
  {
    ethernet_input(received_frame, &netif_data);
    pbuf_free(received_frame);
    received_frame = NULL;
    tud_network_recv_renew();
  }

  sys_check_timeouts();
}

void tud_network_init_cb(void)
{
  /* if the network is re-initializing and we have a leftover packet, we must do a cleanup */
  if (received_frame)
  {
    pbuf_free(received_frame);
    received_frame = NULL;
  }
}

void tinyusb_net_init(const ip_addr_t* ipaddr, const ip_addr_t* netmask, const ip_addr_t* gateway) {
  /* initialize TinyUSB */
  tusb_init();

  /* initialize lwip, dhcp-server, dns-server, and http */
  init_lwip();
}

// WでLEDを使いたいとき、これらの関数がバッティングするかも。
// #ifdefで無効化しないと。

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
