/**
* @file
* @author Steve Karg
* @date October 2019
* @brief Header file for a basic BBMD for BVLC IPv6 handler
*
* @section LICENSE
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef BVLC6_HANDLER_H
#define BVLC6_HANDLER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/datalink/bvlc6.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /* user application function prototypes */
    BACNET_STACK_EXPORT
    int bvlc6_handler(
        BACNET_IP6_ADDRESS *addr,
        BACNET_ADDRESS * src,
        uint8_t * npdu,
        uint16_t npdu_len);

    BACNET_STACK_EXPORT
    int bvlc6_bbmd_enabled_handler(BACNET_IP6_ADDRESS *addr,
        BACNET_ADDRESS *src,
        uint8_t *mtu,
        uint16_t mtu_len);

    BACNET_STACK_EXPORT
    int bvlc6_bbmd_disabled_handler(BACNET_IP6_ADDRESS *addr,
        BACNET_ADDRESS *src,
        uint8_t *mtu,
        uint16_t mtu_len);

    BACNET_STACK_EXPORT
    int bvlc6_send_pdu(BACNET_ADDRESS *dest,
        BACNET_NPDU_DATA *npdu_data,
        uint8_t *pdu,
        unsigned pdu_len);

    BACNET_STACK_EXPORT
    int bvlc6_register_with_bbmd(
        BACNET_IP6_ADDRESS *bbmd_addr,
        uint16_t time_to_live_seconds);

    BACNET_STACK_EXPORT
    void bvlc6_remote_bbmd_address(
        BACNET_IP6_ADDRESS *bbmd_addr);

    BACNET_STACK_EXPORT
    uint16_t bvlc6_remote_bbmd_lifetime(
        void);

    BACNET_STACK_EXPORT
    uint16_t bvlc6_get_last_result(
        void);

    BACNET_STACK_EXPORT
    void bvlc6_set_last_result(
        const uint16_t result_code);

    BACNET_STACK_EXPORT
    uint8_t bvlc6_get_function_code(
        void);

    BACNET_STACK_EXPORT
    void bvlc6_maintenance_timer(
        uint16_t seconds);

    BACNET_STACK_EXPORT
    void bvlc6_debug_enable(
        void);

    BACNET_STACK_EXPORT
    void bvlc6_cleanup(void);

    BACNET_STACK_EXPORT
    void bvlc6_init(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
