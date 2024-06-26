/**
* @file
* @author Steve Karg
* @date October 2019
* @brief Header file for a basic SubscribeCOV request handler, FSM, & Task
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
#ifndef HANDLER_COV_SUBSCRIBE_H
#define HANDLER_COV_SUBSCRIBE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>
#/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/apdu.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    BACNET_STACK_EXPORT
    void handler_cov_subscribe(
        uint8_t * service_request,
        uint16_t service_len,
        BACNET_ADDRESS * src,
        BACNET_CONFIRMED_SERVICE_DATA * service_data);
    BACNET_STACK_EXPORT
    bool handler_cov_fsm(
        const bool reset);
    BACNET_STACK_EXPORT
    void handler_cov_task(
        void);
    BACNET_STACK_EXPORT
    void handler_cov_timer_seconds(
        uint32_t elapsed_seconds);
    BACNET_STACK_EXPORT
    void handler_cov_init(
        void);
    BACNET_STACK_EXPORT
    int handler_cov_encode_subscriptions(
        uint8_t * apdu,
        int max_apdu);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
