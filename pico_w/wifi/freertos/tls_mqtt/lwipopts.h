#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#include "lwipopts_examples_common.h"

#if !NO_SYS
#define TCPIP_THREAD_STACKSIZE 1024
#define DEFAULT_THREAD_STACKSIZE 1024
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define TCPIP_MBOX_SIZE 8
#define LWIP_TIMEVAL_PRIVATE 0

// not necessary, can be done either way
#define LWIP_TCPIP_CORE_LOCKING_INPUT 1
#endif

/* TCP WND must be at least 16 kb to match TLS record size
   or you will get a warning "altcp_tls: TCP_WND is smaller than the RX decrypion buffer, connection RX might stall!" */
#undef TCP_WND
#define TCP_WND  16384

#define LWIP_ALTCP               1
#define LWIP_ALTCP_TLS           1
#define LWIP_ALTCP_TLS_MBEDTLS   1

#define LWIP_DEBUG 1
#define ALTCP_MBEDTLS_DEBUG  LWIP_DBG_ON

// counter measure for lwip-mqtt: sys_timeout: timeout != NULL, pool MEMP_SYS_TIMEOUT is empty
#define MEMP_NUM_SYS_TIMEOUT            (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 1)

#endif
