/**
 * @file
 * @brief Send an Unconfirmed Event Notification.
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2009
 * @copyright SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "bacnet/event.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/sys/debug.h"

/**
 * @brief Sends an Unconfirmed Alarm/Event Notification.
 * @ingroup BIBB-AE-N-A
 * @param buffer [in,out] The buffer to build the message in for sending.
 * @param data [in] The information about the Event to be sent.
 * @param dest [in] The destination address information (may be a broadcast).
 * @return Size of the message sent (bytes), or a negative value on error.
 */
int Send_UEvent_Notify(
    uint8_t *buffer,
    const BACNET_EVENT_NOTIFICATION_DATA *data,
    BACNET_ADDRESS *dest)
{
    int len = 0;
    int pdu_len = 0;
    int bytes_sent = 0;
    BACNET_NPDU_DATA npdu_data;
    BACNET_ADDRESS my_address;

    fprintf(stderr,
        "UEvent: request class=%u object=%u:%u process=%u toState=%u\n",
        (unsigned)data->notificationClass,
        (unsigned)data->eventObjectIdentifier.type,
        (unsigned)data->eventObjectIdentifier.instance,
        (unsigned)data->processIdentifier, (unsigned)data->toState);
    datalink_get_my_address(&my_address);
    /* encode the NPDU portion of the packet */
    npdu_encode_npdu_data(&npdu_data, false, MESSAGE_PRIORITY_NORMAL);
    pdu_len = npdu_encode_pdu(buffer, dest, &my_address, &npdu_data);
    /* encode the APDU portion of the packet */
    len = uevent_notify_encode_apdu(&buffer[pdu_len], data);
    if (len <= 0) {
        fprintf(stderr, "UEvent: APDU encoding returned %d\n", len);
    }
    pdu_len += len;
    /* send the data */
    bytes_sent = datalink_send_pdu(dest, &npdu_data, &buffer[0], pdu_len);
    if (bytes_sent <= 0) {
        int saved_errno = errno;

        fprintf(stderr,
            "UEvent: datalink send failed: bytes=%d pdu_len=%d "
            "errno=%d (%s)\n",
            bytes_sent, pdu_len, saved_errno, strerror(saved_errno));
    } else {
        fprintf(stderr,
            "UEvent: datalink send returned %d for pdu_len=%d; "
            "not proof of delivery\n",
            bytes_sent, pdu_len);
    }

    return bytes_sent;
}
