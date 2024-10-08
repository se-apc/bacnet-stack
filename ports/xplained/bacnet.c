/**
 * @file
 * @brief BACnet task and initialization for the Xplained board
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2013
 * @copyright SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
/* hardware layer includes */
#include "rs485.h"
#include "nvmdata.h"
#include "wdt.h"
#include "led.h"
#include "adc-hdw.h"
/* BACnet Stack includes */
#include "bacnet/npdu.h"
#include "bacnet/dcc.h"
#include "bacnet/iam.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/basic/sys/mstimer.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/basic/sys/ringbuf.h"
/* BACnet objects */
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/object/ai.h"
/* me */
#include "bacnet.h"

/* buffer for incoming BACnet messages */
struct mstp_rx_packet {
    BACNET_ADDRESS src;
    uint16_t length;
    uint8_t buffer[MAX_MPDU];
};
/* count must be a power of 2 for ringbuf library */
#ifndef MSTP_RECEIVE_PACKET_COUNT
#define MSTP_RECEIVE_PACKET_COUNT 2
#endif
static volatile struct mstp_rx_packet Receive_Buffer[MSTP_RECEIVE_PACKET_COUNT];
static RING_BUFFER Receive_Queue;
/* Device ID to track changes */
static uint32_t Device_ID = 0xFFFFFFFF;
/* timer for device communications control */
static struct mstimer DCC_Timer;
#define DCC_CYCLE_SECONDS 1
/* timer for COV */
static struct mstimer COV_Timer;
#define COV_CYCLE_SECONDS 1
/* timer for TSM */
static struct mstimer TSM_Timer;
#define TSM_CYCLE_SECONDS 1
/* timer for Reinit */
static struct mstimer Reinit_Timer;
/* buffer for incoming packets */
static uint8_t PDUBuffer[MAX_MPDU];

/**************************************************************************
 * Description: handles reinitializing the device after a few seconds
 * Returns: none
 * Notes: gives the device enough time to acknowledge the RD request
 **************************************************************************/
static void reinit_task(void)
{
    BACNET_REINITIALIZED_STATE state = BACNET_REINIT_IDLE;

    state = Device_Reinitialized_State();
    if (state == BACNET_REINIT_IDLE) {
        /* set timer to never expire */
        mstimer_set(&Reinit_Timer, 0);
    } else if (mstimer_interval(&Reinit_Timer) > 0) {
        if (mstimer_expired(&Reinit_Timer)) {
            /* reset MCU via watchdog timeout */
            wdt_reset_mcu();
        }
    } else {
        mstimer_set(&Reinit_Timer, 3000);
    }
}

/**************************************************************************
 * Description: handles recurring strictly timed task
 * Returns: none
 * Notes: called by ISR every 5 milliseconds
 **************************************************************************/
void bacnet_task_timed(void)
{
    struct mstp_rx_packet *pkt = NULL;
    uint16_t pdu_len = 0;
    BACNET_ADDRESS src;

    pdu_len = dlmstp_receive(&src, &PDUBuffer[0], sizeof(PDUBuffer), 5);
    if (pdu_len) {
        pkt = (struct mstp_rx_packet *)Ringbuf_Data_Peek(&Receive_Queue);
        if (pkt) {
            memcpy(pkt->buffer, PDUBuffer, MAX_MPDU);
            bacnet_address_copy(&pkt->src, &src);
            pkt->length = pdu_len;
            Ringbuf_Data_Put(&Receive_Queue, (volatile uint8_t *)pkt);
        }
    }
}

/**************************************************************************
 * Description: handles recurring task
 * Returns: none
 * Notes: none
 **************************************************************************/
static void bacnet_test_task(void)
{
    static unsigned index = 0;
    uint32_t instance;
    float float_value;
    uint16_t adc_value;

    instance = Analog_Input_Index_To_Instance(index);
    if (!Analog_Input_Out_Of_Service(instance)) {
        adc_value = adc_result_12bit(index);
        float_value = adc_value;
        float_value /= 4095;
        Analog_Input_Present_Value_Set(instance, float_value);
    }
    index++;
    if (index >= MAX_ANALOG_INPUTS) {
        index = 0;
    }
}

/**************************************************************************
 * Description: handles recurring task
 * Returns: none
 * Notes: none
 **************************************************************************/
void bacnet_task(void)
{
    struct mstp_rx_packet pkt = { 0 };
    bool pdu_available = false;

    /* hello, World! */
    if (Device_ID != Device_Object_Instance_Number()) {
        Device_ID = Device_Object_Instance_Number();
        Send_I_Am(&Handler_Transmit_Buffer[0]);
    }
    /* handle the timers */
    if (mstimer_expired(&DCC_Timer)) {
        mstimer_reset(&DCC_Timer);
        dcc_timer_seconds(DCC_CYCLE_SECONDS);
        led_on_interval(LED_DEBUG, 500);
    }
    if (mstimer_expired(&TSM_Timer)) {
        mstimer_reset(&TSM_Timer);
        tsm_timer_milliseconds(mstimer_interval(&TSM_Timer));
    }
    reinit_task();
    bacnet_test_task();
    /* handle the messaging */
    if ((!dlmstp_send_pdu_queue_full()) && (!Ringbuf_Empty(&Receive_Queue))) {
        Ringbuf_Pop(&Receive_Queue, (uint8_t *)&pkt);
        pdu_available = true;
    }
    if (pdu_available) {
        led_on_interval(LED_APDU, 125);
        npdu_handler(&pkt.src, &pkt.buffer[0], pkt.length);
    }
}

/**************************************************************************
 * Description: initializes the BACnet library
 * Returns: none
 * Notes: none
 **************************************************************************/
void bacnet_init(void)
{
    unsigned i;

    Ringbuf_Init(&Receive_Queue, (uint8_t *)&Receive_Buffer,
        sizeof(struct mstp_rx_packet), MSTP_RECEIVE_PACKET_COUNT);
    dlmstp_init(NULL);
    /* initialize objects */
    Device_Init(NULL);
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
    /* handle communication so we can shut up when asked */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL,
        handler_device_communication_control);
    /* start the cyclic 1 second timer for DCC */
    mstimer_set(&DCC_Timer, DCC_CYCLE_SECONDS * 1000);
    /* start the cyclic 1 second timer for COV */
    mstimer_set(&COV_Timer, COV_CYCLE_SECONDS * 1000);
    /* start the cyclic 1 second timer for TSM */
    mstimer_set(&TSM_Timer, TSM_CYCLE_SECONDS * 1000);
    for (i = 0; i < MAX_ANALOG_INPUTS; i++) {
        Analog_Input_Units_Set(
            Analog_Input_Index_To_Instance(i), UNITS_PERCENT);
    }
}
