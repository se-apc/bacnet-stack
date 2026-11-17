/**
 * @file
 * @brief  Send a ConfirmedEventNotification Request.
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2005
 * @copyright SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "bacnet/event.h"
#include "bacnet/dcc.h"
/* some demo stuff needed */
#include "bacnet/basic/object/device.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/basic/binding/address.h"
#include "bacnet/basic/sys/debug.h"
#include "bacnet/basic/services.h"

/** Sends an Confirmed Alarm/Event Notification.
 * @ingroup EVNOTFCN
 *
 * @param pdu [in] the PDU buffer used for sending the message
 * @param pdu_size [in] Size of the PDU buffer
 * @param data [in] The information about the Event to be sent.
 * @param dest [in] BACNET_ADDRESS of the destination device
 * @return invoke id of outgoing message, or 0 if communication is disabled,
 *         or no tsm slot is available.
 */
uint8_t Send_CEvent_Notify_Address(
    uint8_t *pdu,
    uint16_t pdu_size,
    const BACNET_EVENT_NOTIFICATION_DATA *data,
    BACNET_ADDRESS *dest)
{
    int len = 0;
    int pdu_len = 0;
    int bytes_sent = 0;
    BACNET_NPDU_DATA npdu_data;
    BACNET_ADDRESS my_address;
    uint8_t invoke_id = 0;

    fprintf(stderr,
        "CEvent: request class=%u object=%u:%u process=%u toState=%u "
        "pdu_limit=%u\n",
        (unsigned)data->notificationClass,
        (unsigned)data->eventObjectIdentifier.type,
        (unsigned)data->eventObjectIdentifier.instance,
        (unsigned)data->processIdentifier, (unsigned)data->toState,
        (unsigned)pdu_size);
    if (!dcc_communication_enabled()) {
        fprintf(stderr, "CEvent: not sent: DCC communication disabled\n");
        return 0;
    }
    if (!dest) {
        fprintf(stderr, "CEvent: not sent: NULL destination\n");
        return 0;
    }
    /* is there a tsm available? */
    invoke_id = tsm_next_free_invokeID();
    if (invoke_id) {
        /* encode the NPDU portion of the packet */
        datalink_get_my_address(&my_address);
        npdu_encode_npdu_data(&npdu_data, true, MESSAGE_PRIORITY_NORMAL);
        pdu_len = npdu_encode_pdu(pdu, dest, &my_address, &npdu_data);
        /* encode the APDU portion of the packet */
        len = cevent_notify_encode_apdu(&pdu[pdu_len], invoke_id, data);
        if (len <= 0) {
            fprintf(stderr,
                "CEvent: APDU encoding returned %d for invoke_id=%u\n",
                len, (unsigned)invoke_id);
        }
        pdu_len += len;
        /* will it fit in the sender?
           note: if there is a bottleneck router in between
           us and the destination, we won't know unless
           we have a way to check for that and update the
           max_apdu in the address binding table. */
        if ((uint16_t)pdu_len < pdu_size) {
            tsm_set_confirmed_unsegmented_transaction(
                invoke_id, dest, &npdu_data, pdu, (uint16_t)pdu_len);
            bytes_sent = datalink_send_pdu(dest, &npdu_data, pdu, pdu_len);
            if (bytes_sent <= 0) {
                int saved_errno = errno;

                fprintf(stderr,
                    "CEvent: datalink send failed: invoke_id=%u bytes=%d "
                    "pdu_len=%d errno=%d (%s); transaction retained\n",
                    (unsigned)invoke_id, bytes_sent, pdu_len, saved_errno,
                    strerror(saved_errno));
            } else {
                fprintf(stderr,
                    "CEvent: datalink send returned %d for pdu_len=%d "
                    "invoke_id=%u; awaiting transaction completion, "
                    "not proof of delivery\n",
                    bytes_sent, pdu_len, (unsigned)invoke_id);
            }
        } else {
            tsm_free_invoke_id(invoke_id);
            invoke_id = 0;
            fprintf(stderr,
                "CEvent: not sent: encoded PDU length=%d >= limit=%u; "
                "invoke ID released\n",
                pdu_len, (unsigned)pdu_size);
        }
    } else {
        fprintf(stderr,
            "CEvent: not sent: no free TSM invoke ID (capacity=%u)\n",
            (unsigned)MAX_TSM_TRANSACTIONS);
    }

    return invoke_id;
}

/** Sends an Confirmed Alarm/Event Notification.
 * @ingroup EVNOTFCN
 *
 * @param device_id [in] ID of the destination device
 * @param data [in] The information about the Event to be sent.
 * @return invoke id of outgoing message, or 0 if communication is disabled,
 *         or no tsm slot is available.
 */
uint8_t Send_CEvent_Notify(
    uint32_t device_id, const BACNET_EVENT_NOTIFICATION_DATA *data)
{
    BACNET_ADDRESS dest = { 0 };
    unsigned max_apdu = 0;
    uint8_t invoke_id = 0;
    bool status = false;

    /* is the device bound? */
    status = address_get_by_device(device_id, &max_apdu, &dest);
    if (status) {
        if (sizeof(Handler_Transmit_Buffer) < max_apdu) {
            max_apdu = sizeof(Handler_Transmit_Buffer);
        }
        invoke_id = Send_CEvent_Notify_Address(
            Handler_Transmit_Buffer, max_apdu, data, &dest);
        fprintf(stderr, "CEvent: device=%u class=%u result invoke_id=%u\n",
            (unsigned)device_id, (unsigned)data->notificationClass,
            (unsigned)invoke_id);
    } else {
        fprintf(stderr,
            "CEvent: not sent: no address binding for device=%u class=%u\n",
            (unsigned)device_id, (unsigned)data->notificationClass);
    }

    return invoke_id;
}
