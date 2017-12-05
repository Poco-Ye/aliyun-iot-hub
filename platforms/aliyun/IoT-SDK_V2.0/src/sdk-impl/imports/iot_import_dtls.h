/*
 * Copyright (c) 2014-2016 Alibaba Group. All rights reserved.
 * License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */



#include "iot_import_coap.h"
#include "mbedtls/ssl.h"
#include "mbedtls/platform.h"
#include "mbedtls/sha256.h"
#include "mbedtls/debug.h"
#include "mbedtls/timing.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ssl_cookie.h"


#ifndef __COAP_DTLS_H__
#define __COAP_DTLS_H__


#define dtls_log_print(level, ...) \
    {\
    fprintf(stderr, "%s [%s #%d]   ",level, __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);\
    }

#define DTLS_TRC(fmt,  args...)  dtls_log_print("<TRACE>", fmt, ##args)
#define DTLS_DUMP(fmt, args...)  dtls_log_print("<DUMP> ", fmt, ##args)
#define DTLS_DEBUG(fmt,args...)  dtls_log_print("<DEBUG>", fmt, ##args)
#define DTLS_INFO(fmt, args...)  dtls_log_print("<INFO> ", fmt, ##args)
#define DTLS_ERR(fmt,  args...)  dtls_log_print("<ERROR>", fmt, ##args)

#define DTLS_ERROR_BASE       (1<<24)


#define DTLS_SUCCESS                        0
#define DTLS_INVALID_PARAM             (DTLS_ERROR_BASE | 1)
#define DTLS_INVALID_CA_CERTIFICATE    (DTLS_ERROR_BASE | 2)
#define DTLS_HANDSHAKE_IN_PROGRESS     (DTLS_ERROR_BASE | 3)
#define DTLS_HANDSHAKE_FAILED          (DTLS_ERROR_BASE | 4)
#define DTLS_FATAL_ALERT_MESSAGE       (DTLS_ERROR_BASE | 5)
#define DTLS_PEER_CLOSE_NOTIFY         (DTLS_ERROR_BASE | 6)
#define DTLS_SESSION_CREATE_FAILED     (DTLS_ERROR_BASE | 7)
#define DTLS_READ_DATA_FAILED          (DTLS_ERROR_BASE | 8)


typedef  int (*coap_dtls_send_t)(void *socket_id,
                           const unsigned char  *p_data,
                                 size_t          datalen);


typedef  int (*coap_dtls_recv_t)(void *socket_id,
                                 unsigned char   *p_data,
                                 size_t           datalen);

typedef  int (*coap_dtls_recv_timeout_t)(void *socket_id,
                              unsigned char   *p_data,
                              size_t           datalen,
                              unsigned int     timeout);


typedef struct
{
    int               socket_id;
    unsigned char     remote_addr[NETWORK_ADDR_LEN];
    unsigned short    remote_port;
} dtls_network_t;


typedef struct
{
    dtls_network_t            network;
    coap_dtls_send_t          send_fn;
    coap_dtls_recv_t          recv_fn;
    coap_dtls_recv_timeout_t  recv_timeout_fn;
    unsigned char             *p_ca_cert_pem;
    char                      *p_host;
} coap_dtls_options_t;


typedef void DTLSContext;

DTLSContext *HAL_DTLSSession_init();

unsigned int HAL_DTLSSession_create(DTLSContext *conetxt, coap_dtls_options_t  *p_options);

unsigned int HAL_DTLSSession_write(DTLSContext *conetxt,
                          const unsigned char   *p_data,
                                unsigned int    *p_datalen);


unsigned int HAL_DTLSSession_read(DTLSContext *conetxt,
                               unsigned char   *p_data,
                               unsigned int    *p_datalen,
                               unsigned int     timeout);

unsigned int HAL_DTLSSession_free(DTLSContext *conetxt);


#endif
