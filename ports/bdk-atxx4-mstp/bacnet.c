/**************************************************************************
 *
 * Copyright (C) 2010 Steve Karg <skarg@users.sourceforge.net>
 *
 * SPDX-License-Identifier: MIT
 *
 *********************************************************************/
#include <stdint.h>
#include <stdbool.h>
/* hardware layer includes */
#include "hardware.h"
#include "bacnet/basic/sys/mstimer.h"
#include "seeprom.h"
#include "nvdata.h"
#include "rs485.h"
#include "input.h"
#include "adc.h"
#include "led.h"
/* BACnet Stack includes */
#include "bacnet/datalink/datalink.h"
#include "bacnet/npdu.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/dcc.h"
#include "bacnet/iam.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/object/ai.h"
#include "bacnet/basic/object/av.h"
#include "bacnet/basic/object/bi.h"
#include "bacnet/basic/object/bo.h"
/* me */
#include "bacnet.h"

/* MAC Address of MS/TP */
static uint8_t MSTP_MAC_Address;
/* timer for device communications control */
static struct mstimer DCC_Timer;
#define DCC_CYCLE_SECONDS 1

static bool seeprom_version_test(void)
{
    uint16_t version = 0;
    uint16_t id = 0;
    bool status = false;
    int rv;

    rv = seeprom_bytes_read(NV_SEEPROM_TYPE_0, (uint8_t *)&id, 2);
    if (rv > 0) {
        rv = seeprom_bytes_read(NV_SEEPROM_VERSION_0, (uint8_t *)&version, 2);
    }

    if ((rv > 0) && (id == SEEPROM_ID) && (version == SEEPROM_VERSION)) {
        status = true;
    } else if (rv > 0) {
        version = SEEPROM_VERSION;
        id = SEEPROM_ID;
        seeprom_bytes_write(NV_SEEPROM_TYPE_0, (uint8_t *)&id, 2);
        seeprom_bytes_write(NV_SEEPROM_VERSION_0, (uint8_t *)&version, 2);
    } else {
        while (1) {
            /* SEEPROM is faulty! */
        }
    }

    return status;
}

static void device_id_init(uint8_t mac)
{
    uint32_t device_id = 0;

    /* Get the device ID from the eeprom */
    eeprom_bytes_read(
        NV_EEPROM_DEVICE_0, (uint8_t *)&device_id, sizeof(device_id));
    if (device_id < BACNET_MAX_INSTANCE) {
        Device_Set_Object_Instance_Number(device_id);
    } else {
        /* use the DIP switch address as the Device ID if unconfigured */
        Device_Set_Object_Instance_Number(MSTP_MAC_Address);
    }
}

void bacnet_init(void)
{
    uint8_t max_master = 0;

    MSTP_MAC_Address = input_address();
    dlmstp_set_mac_address(MSTP_MAC_Address);
    eeprom_bytes_read(NV_EEPROM_MAX_MASTER, &max_master, 1);
    if (max_master > 127) {
        max_master = 127;
    }
    dlmstp_set_max_master(max_master);
    dlmstp_init(NULL);
    /* test for valid data structure in SEEPROM */
    if (!seeprom_version_test()) {
        /* do something when SEEPROM is invalid - i.e. init to defaults */
    }
    /* initialize objects */
    Device_Init(NULL);
    device_id_init(MSTP_MAC_Address);
    /* set up our confirmed service unrecognized service handler - required! */
    apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);
    /* we need to handle who-is to support dynamic device binding */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_HAS, handler_who_has);
    /* Set the handlers for any confirmed services that we support. */
    /* We must implement read property - it's required! */
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_PROP_MULTIPLE, handler_read_property_multiple);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_REINITIALIZE_DEVICE, handler_reinitialize_device);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_WRITE_PROPERTY, handler_write_property);
    /* handle communication so we can shutup when asked */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL,
        handler_device_communication_control);
    /* start the cyclic 1 second timer for DCC */
    mstimer_set(&DCC_Timer, DCC_CYCLE_SECONDS * 1000);
    /* Hello World! */
    Send_I_Am(&Handler_Transmit_Buffer[0]);
}

/** Static receive buffer, initialized with zeros by the C Library Startup Code. */

static uint8_t PDUBuffer[MAX_MPDU + 16 /* Add a little safety margin to the buffer,
                                        * so that in the rare case, the message
                                        * would be filled up to MAX_MPDU and some
                                        * decoding functions would overrun, these
                                        * decoding functions will just end up in
                                        * a safe field of static zeros. */];

/** BACnet task doing receive and transmit. */

void bacnet_task(void)
{
    uint8_t mstp_mac_address;
    uint16_t pdu_len;
    BACNET_ADDRESS src; /* source address */
    uint16_t value;
    bool button_value;
    uint8_t i;
    BACNET_BINARY_PV binary_value = BINARY_INACTIVE;
    BACNET_POLARITY polarity;
    bool out_of_service;

    mstp_mac_address = input_address();
    if (MSTP_MAC_Address != mstp_mac_address) {
        /* address changed! */
        MSTP_MAC_Address = mstp_mac_address;
        dlmstp_set_mac_address(MSTP_MAC_Address);
        device_id_init(MSTP_MAC_Address);
        Send_I_Am(&Handler_Transmit_Buffer[0]);
    }
    /* handle the inputs */
    value = adc_result_10bit(7);
    Analog_Input_Present_Value_Set(0, value);
    for (i = 0; i < 5; i++) {
        button_value = input_button_value(i);
        if (button_value) {
            binary_value = BINARY_ACTIVE;
        } else {
            binary_value = BINARY_INACTIVE;
        }
        Binary_Input_Present_Value_Set(i, binary_value);
    }
    /* Binary Output */
    for (i = 0; i < 2; i++) {
        out_of_service = Binary_Output_Out_Of_Service(i);
        if (!out_of_service) {
            binary_value = Binary_Output_Present_Value(i);
            polarity = Binary_Output_Polarity(i);
            if (polarity != POLARITY_NORMAL) {
                if (binary_value == BINARY_ACTIVE) {
                    binary_value = BINARY_INACTIVE;
                } else {
                    binary_value = BINARY_ACTIVE;
                }
            }
            if (binary_value == BINARY_ACTIVE) {
                if (i == 0) {
                    led_on(LED_2);
                } else {
                    led_on(LED_3);
                }
            } else {
                if (i == 0) {
                    led_off(LED_2);
                } else {
                    led_off(LED_3);
                }
            }
        }
    }
    /* handle the communication timer */
    if (mstimer_expired(&DCC_Timer)) {
        mstimer_reset(&DCC_Timer);
        dcc_timer_seconds(DCC_CYCLE_SECONDS);
    }
    /* handle the messaging */
    pdu_len = datalink_receive(&src, &PDUBuffer[0], MAX_MPDU, 0);
    if (pdu_len) {
        npdu_handler(&src, &PDUBuffer[0], pdu_len);
    }
}
