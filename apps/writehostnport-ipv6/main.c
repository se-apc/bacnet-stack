/**************************************************************************
 *
 * Copyright (C) 2006-2007 Steve Karg <skarg@users.sourceforge.net>
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
 *
 *********************************************************************/

/* command line tool that sends a BACnet service, and displays the response */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h> /* for time */
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h> /* toupper */
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/bactext.h"
#include "bacnet/iam.h"
#include "bacnet/arf.h"
#include "bacnet/npdu.h"
#include "bacnet/apdu.h"
#include "bacnet/datalink/bvlc6.h"
#include "bacnet/whois.h"
#include "bacnet/version.h"
/* some demo stuff needed */
#include "bacnet/basic/binding/address.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/basic/sys/filename.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/datalink/dlenv.h"
#include "bacport.h"

#ifndef MAX_PROPERTY_VALUES
#define MAX_PROPERTY_VALUES 64
#endif

#if BACNET_SVC_SERVER
#error "App requires server-only features disabled! Set BACNET_SVC_SERVER=0"
#endif

/* buffer used for receive */
static uint8_t Rx_Buf[MAX_MPDU] = { 0 };

/* global variables used in this file */
static uint32_t Target_Device_Object_Instance = BACNET_MAX_INSTANCE;
static uint32_t Target_Object_Instance = BACNET_MAX_INSTANCE;
static BACNET_OBJECT_TYPE Target_Object_Type = OBJECT_ANALOG_INPUT;
static BACNET_PROPERTY_ID Target_Object_Property = PROP_ACKED_TRANSITIONS;
/* array index value or BACNET_ARRAY_ALL */
static int32_t Target_Object_Property_Index = BACNET_ARRAY_ALL;
static BACNET_APPLICATION_DATA_VALUE
    Target_Object_Property_Value[MAX_PROPERTY_VALUES];

/* 0 if not set, 1..16 if set */
static uint8_t Target_Object_Property_Priority = 0;

/* needed to filter incoming messages */
static uint8_t Request_Invoke_ID = 0;
static BACNET_ADDRESS Target_Address;
/* needed for return value of main application */
static bool Error_Detected = false;

static void MyErrorHandler(BACNET_ADDRESS *src,
    uint8_t invoke_id,
    BACNET_ERROR_CLASS error_class,
    BACNET_ERROR_CODE error_code)
{
    if (address_match(&Target_Address, src) &&
        (invoke_id == Request_Invoke_ID)) {
        printf("BACnet Error: %s: %s\n",
            bactext_error_class_name((int)error_class),
            bactext_error_code_name((int)error_code));
        Error_Detected = true;
    }
}

static void MyAbortHandler(
    BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t abort_reason, bool server)
{
    (void)server;
    if (address_match(&Target_Address, src) &&
        (invoke_id == Request_Invoke_ID)) {
        printf(
            "BACnet Abort: %s\n", bactext_abort_reason_name((int)abort_reason));
        Error_Detected = true;
    }
}

static void MyRejectHandler(
    BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t reject_reason)
{
    if (address_match(&Target_Address, src) &&
        (invoke_id == Request_Invoke_ID)) {
        printf("BACnet Reject: %s\n",
            bactext_reject_reason_name((int)reject_reason));
        Error_Detected = true;
    }
}

static void MyWritePropertySimpleAckHandler(
    BACNET_ADDRESS *src, uint8_t invoke_id)
{
    if (address_match(&Target_Address, src) &&
        (invoke_id == Request_Invoke_ID)) {
        printf("\nWriteProperty Acknowledged!\n");
    }
}

static void Init_Service_Handlers(void)
{
    Device_Init(NULL);
    /* we need to handle who-is
       to support dynamic device binding to us */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    /* handle i-am to support binding to other devices */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_add);
    /* set the handler for all the services we don't implement
       It is required to send the proper reject message... */
    apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);
    /* we must implement read property - it's required! */
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
    /* handle the ack coming back */
    apdu_set_confirmed_simple_ack_handler(
        SERVICE_CONFIRMED_WRITE_PROPERTY, MyWritePropertySimpleAckHandler);
    /* handle any errors coming back */
    apdu_set_error_handler(SERVICE_CONFIRMED_WRITE_PROPERTY, MyErrorHandler);
    apdu_set_abort_handler(MyAbortHandler);
    apdu_set_reject_handler(MyRejectHandler);
}

static void print_usage(char *filename)
{
    printf("Usage: %s <Device ID>  [IPv6]:port\n",
        filename);
}

static void print_help(char *filename)
{
    printf(
       "Send a Write FD_BBMD_Address (HostNPort) property to a FOREIGN device.\r\n"
       "\r\n"
       "To send a Write FD_BBMD_Address property request to a BACnet foreign device od ID 777\r\n"
       "at 2001:bb6::1 using port 47808 FDBBMD_Address value [::1]:47808\r\n"
       "%s 777  [::1]:47808\r\n",
       filename_remove_path(filename));
}

int parse_addr_n_port(const char *input, BACNET_IP6_ADDRESS *addr) {
    char address[INET6_ADDRSTRLEN];
    int port = 65536;

    // Parse the address
    char* start = strchr(input, '[');
    char* end = strchr(input, ']');
    if (start && end) {
        size_t length = end - start - 1;
        strncpy(address, start + 1, length);
        address[length] = '\0';
    } else {
        fprintf(stderr, "[%s %d %s]: Invalid IPv6 format!! '%s'\r\n", __FILE__, __LINE__, __func__, input);
        return 1;
    }

    // Parse the port
    char *portStr = strchr(end, ':');
    char *endPortStr = NULL;
    if (portStr) {
        portStr++;
        port = strtol(portStr, &endPortStr, 10);
    } else {
        fprintf(stderr, "[%s %d %s]: Invalid port!! '%s'\r\n", __FILE__, __LINE__, __func__, end);
    }

    // Check for parsing errors
    if ((errno == ERANGE && (port == LONG_MAX || port == LONG_MIN)) || (errno != 0 && port == 0)) {
        fprintf(stderr, "[%s %d %s]: Invalid port!! '%s'\r\n", __FILE__, __LINE__, __func__, portStr);
        return 1;
    } else if (endPortStr == portStr) {
        fprintf(stderr, "[%s %d %s]: Invalid port!! No digits were found! '%s'\r\n", __FILE__, __LINE__, __func__, portStr);
        return 1;
    }

    // Output the parsed address and port
    printf("Address: %s\r\n", address);
    printf("Port: %d\r\n", port);
#if 0
typedef struct BACnet_IP6_Address {
    uint8_t address[IP6_ADDRESS_MAX];
    uint16_t port;
} BACNET_IP6_ADDRESS;

#endif
    if (inet_pton(AF_INET6, address, &addr->address) == 1) {
        //Conversion successful
    } else {
        fprintf(stderr, "[%s %d %s]: Invalid IPv6 format!! '%s'\r\n", __FILE__, __LINE__, __func__, address);
        return 1;
    }

    if(port > 65535)
        return 1;

    addr->port = port;

    return 0;
}

uint8_t _Send_Write_Property_Request_Data(BACNET_ADDRESS dest,
    BACNET_OBJECT_TYPE object_type,
    uint32_t object_instance,
    BACNET_PROPERTY_ID object_property,
    uint8_t *application_data,
    int application_data_len,
    uint8_t priority,
    uint32_t array_index)
{
    BACNET_ADDRESS my_address;
    unsigned max_apdu = 0;
    uint8_t invoke_id = 0;
    bool status = false;
    int len = 0;
    int pdu_len = 0;
    int bytes_sent = 0;
    BACNET_WRITE_PROPERTY_DATA data;
    BACNET_NPDU_DATA npdu_data;

    if (!dcc_communication_enabled()) {
        return 0;
    }

    /* is the device bound? */
#if 0
    status = address_get_by_device(device_id, &max_apdu, &dest);
    /* is there a tsm available? */
    if (status) {
#endif
        invoke_id = tsm_next_free_invokeID();
#if 0
    }
#endif
    if (invoke_id) {
        /* encode the NPDU portion of the packet */
        datalink_get_my_address(&my_address);
        npdu_encode_npdu_data(&npdu_data, true, MESSAGE_PRIORITY_NORMAL);
        pdu_len = npdu_encode_pdu(
            &Handler_Transmit_Buffer[0], &dest, &my_address, &npdu_data);
        /* encode the APDU portion of the packet */
        data.object_type = object_type;
        data.object_instance = object_instance;
        data.object_property = object_property;
        data.array_index = array_index;
        data.application_data_len = application_data_len;
        memcpy(&data.application_data[0], &application_data[0],
            application_data_len);
        data.priority = priority;
        len =
            wp_encode_apdu(&Handler_Transmit_Buffer[pdu_len], invoke_id, &data);
        pdu_len += len;
        /* will it fit in the sender?
           note: if there is a bottleneck router in between
           us and the destination, we won't know unless
           we have a way to check for that and update the
           max_apdu in the address binding table. */
        if ((unsigned)pdu_len < max_apdu) {
            tsm_set_confirmed_unsegmented_transaction(invoke_id, &dest,
                &npdu_data, &Handler_Transmit_Buffer[0], (uint16_t)pdu_len);
            bytes_sent = datalink_send_pdu(
                &dest, &npdu_data, &Handler_Transmit_Buffer[0], pdu_len);
            if (bytes_sent <= 0) {
#if PRINT_ENABLED
                fprintf(stderr, "Failed to Send WriteProperty Request (%s)!\n",
                    strerror(errno));
#endif
            }
        } else {
            tsm_free_invoke_id(invoke_id);
            invoke_id = 0;
#if PRINT_ENABLED
            fprintf(stderr,
                "Failed to Send WriteProperty Request "
                "(exceeds destination maximum APDU)!\n");
#endif
        }
    }

    return invoke_id;
}


int main(int argc, char *argv[])
{
    BACNET_ADDRESS src = { 0 }; /* address where message came from */
    BACNET_IP6_ADDRESS destination;
    BACNET_IP6_ADDRESS fd_bbmd_address;
    uint16_t pdu_len = 0;
    unsigned timeout = 100; /* milliseconds */
    unsigned max_apdu = 0;
    unsigned object_type = 0;
    unsigned object_property = 0;
    time_t elapsed_seconds = 0;
    time_t last_seconds = 0;
    time_t current_seconds = 0;
    time_t timeout_seconds = 0;
    bool found = false;
    char *value_string;
    bool status = false;
    int args_remaining = 0, tag_value_arg = 0, i = 0;
    long property_tag;
    long priority = BACNET_NO_PRIORITY;
    uint8_t context_tag = 0;
    int argi = 0;

    /* print help if requested */
    for (argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "--help") == 0) {
            print_usage(filename_remove_path(argv[0]));
            print_help(filename_remove_path(argv[0]));
            return 0;
        }
        if (strcmp(argv[argi], "--version") == 0) {
            printf(
                "%s %s\n", filename_remove_path(argv[0]), BACNET_VERSION_TEXT);
            printf("Copyright (C) 2024 by Tomasz Motyl\n"
                   "This is free software; see the source for copying "
                   "conditions.\n"
                   "There is NO warranty; not even for MERCHANTABILITY or\n"
                   "FITNESS FOR A PARTICULAR PURPOSE.\n");
            return 0;
        }
    }
    if (argc < 3) {
        print_usage(filename_remove_path(argv[0]));
        return 0;
    }
    /* decode the command line parameters */
    Target_Device_Object_Instance = strtol(argv[1], NULL, 0);
    Target_Object_Type = OBJECT_NETWORK_PORT;
    Target_Object_Instance = 0;
    Target_Object_Property = PROP_FD_BBMD_ADDRESS;
    Target_Object_Property_Priority = BACNET_NO_PRIORITY;
    Target_Object_Property_Index = BACNET_ARRAY_ALL;
#if 0
    if(parse_addr_n_port(argv[1], &destination)) {
        return 0;
    }
#endif

    if(parse_addr_n_port(argv[2], &fd_bbmd_address)) {
        return 0;
    }
#if 0

#endif

    if (Target_Device_Object_Instance > BACNET_MAX_INSTANCE) {
        fprintf(stderr, "device-instance=%u - not greater than %u\n",
            Target_Device_Object_Instance, BACNET_MAX_INSTANCE);
        return 1;
    }
    if (Target_Object_Type > MAX_BACNET_OBJECT_TYPE) {
        fprintf(stderr, "object-type=%u - it must be less than %u\n",
            Target_Object_Type, MAX_BACNET_OBJECT_TYPE + 1);
        return 1;
    }
    if (Target_Object_Instance > BACNET_MAX_INSTANCE) {
        fprintf(stderr, "object-instance=%u - not greater than %u\n",
            Target_Object_Instance, BACNET_MAX_INSTANCE);
        return 1;
    }
    if (Target_Object_Property > MAX_BACNET_PROPERTY_ID) {
        fprintf(stderr, "property=%u - it must be less than %u\n",
            Target_Object_Property, MAX_BACNET_PROPERTY_ID + 1);
        return 1;
    }

        /* Print the written value (for debug) */
#if 0
        fprintf(stderr, "Writing: ");
        BACNET_OBJECT_PROPERTY_VALUE dummy_opv = {
            .value = &Target_Object_Property_Value[i],
            .array_index = Target_Object_Property_Index,
        };
        bacapp_print_value(stderr, &dummy_opv);
        fprintf(stderr, "\n");

        uint8_t apdu[1000];
        int len = bacapp_encode_application_data(apdu, &Target_Object_Property_Value[i]);
        for(int q=0;q<len;q++) {
            printf("%02x ", apdu[q]);
        }
        printf("\n");
#endif

    /* setup my info */
    Device_Set_Object_Instance_Number(BACNET_MAX_INSTANCE);
    fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
    address_init();
    fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
    Init_Service_Handlers();
    fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
    dlenv_init();
    {
        char *pEnv = NULL;
        BACNET_IP6_ADDRESS addr;
        pEnv = getenv("BACNET_BIP6_DEBUG");
        fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
        if (pEnv) {
            bip6_debug_enable();
            bvlc6_debug_enable();
        }
        fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
        pEnv = getenv("BACNET_BIP6_BROADCAST");
        fprintf(stderr, "[%s %d %s]: pEnv = %p)!!\r\n", __FILE__, __LINE__, __func__, pEnv);
        if (pEnv) {
            fprintf(stderr, "[%s %d %s]: b4 bcast = %d!\r\n", __FILE__, __LINE__, __func__, (uint16_t)strtol(pEnv, NULL, 0));
            bvlc6_address_set(&addr, (uint16_t)strtol(pEnv, NULL, 0), 0, 0, 0, 0, 0,
                    0, BIP6_MULTICAST_GROUP_ID);
            bip6_set_broadcast_addr(&addr);
        } else {
            bvlc6_address_set(&addr, BIP6_MULTICAST_SITE_LOCAL, 0, 0, 0, 0, 0, 0,
                    BIP6_MULTICAST_GROUP_ID);
            bip6_set_broadcast_addr(&addr);
        }
        fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
        pEnv = getenv("BACNET_BIP6_PORT");
        fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
        if (pEnv) {
            bip6_set_port((uint16_t)strtol(pEnv, NULL, 0));
        } else {
            bip6_set_port(0xBAC0);
        }
        fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
    }
    fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
    atexit(datalink_cleanup);
    /* configure the timeout values */
    last_seconds = time(NULL);
    timeout_seconds = (apdu_timeout() / 1000) * apdu_retries();
    /* try to bind with the device */
    fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
    found = address_bind_request(
        Target_Device_Object_Instance, &max_apdu, &Target_Address);
    fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
    if (!found) {
        Send_WhoIs(
            Target_Device_Object_Instance, Target_Device_Object_Instance);
    }
    /* loop forever */
    fprintf(stderr, "[%s %d %s]: b4 for(;;)!!\r\n", __FILE__, __LINE__, __func__);
    for (;;) {
        /* increment timer - exit if timed out */
        current_seconds = time(NULL);

        /* at least one second has passed */
        if (current_seconds != last_seconds) {
            tsm_timer_milliseconds(
                (uint16_t)((current_seconds - last_seconds) * 1000));
            datalink_maintenance_timer(current_seconds - last_seconds);
        }
        if (Error_Detected) {
            break;
        }
        /* wait until the device is bound, or timeout and quit */
        //fprintf(stderr, "[%s %d %s]: found = '%s'\r\n", __FILE__, __LINE__, __func__, found ? "true" : "false");
        if (!found) {
            found = address_bind_request(
                Target_Device_Object_Instance, &max_apdu, &Target_Address);
        pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout);
	//if(pdu_len != 0)
	  fprintf(stderr, "received %d bytes\r\n", pdu_len);
        }
        //fprintf(stderr, "[%s %d %s]: found = '%s'\r\n", __FILE__, __LINE__, __func__, found ? "true" : "false");
        if (found) {
        //fprintf(stderr, "[%s %d %s]: found = '%s'\r\n", __FILE__, __LINE__, __func__, found ? "true" : "false");
            if (Request_Invoke_ID == 0) {
                uint8_t apdu[1024] = {0, };
                BACNET_HOST_N_PORT hnp = {
                    .host_ip_address = 1,
                    .host_name = 0,
                    .port = fd_bbmd_address.port,
                    .host = {. ip_address = { .length = sizeof(fd_bbmd_address.address)} }};

                memcpy(hnp.host.ip_address.value, fd_bbmd_address.address, sizeof(fd_bbmd_address.address));
                int apdu_len = host_n_port_encode(apdu, &hnp);
#if 0
typedef struct BACnet_Octet_String {
    size_t length;
    uint8_t value[MAX_OCTET_STRING_BYTES];
} BACNET_OCTET_STRING;
#endif
#if 0
typedef struct BACnetHostNPort {
    bool host_ip_address:1;
    bool host_name:1;
    union BACnetHostAddress {
        /* none = host_ip_address AND host_name are FALSE */
        BACNET_OCTET_STRING ip_address;
        BACNET_CHARACTER_STRING name;
    } host;
    uint16_t port;
} BACNET_HOST_N_PORT;
#endif
#if 0
                Request_Invoke_ID = Send_Write_Property_Request(
                    Target_Device_Object_Instance, Target_Object_Type,
                    Target_Object_Instance, Target_Object_Property,
                    &Target_Object_Property_Value[0],
                    Target_Object_Property_Priority,
                    Target_Object_Property_Index);
#else
Request_Invoke_ID = Send_Write_Property_Request_Data(Target_Device_Object_Instance,
    Target_Object_Type,
    Target_Object_Instance,
    Target_Object_Property,
    apdu,
    apdu_len,
    Target_Object_Property_Priority,
    Target_Object_Property_Index);
#endif
            } else if (tsm_invoke_id_free(Request_Invoke_ID)) {
                break;
            } else if (tsm_invoke_id_failed(Request_Invoke_ID)) {
                fprintf(stderr, "\rError: TSM Timeout!\n");
                tsm_free_invoke_id(Request_Invoke_ID);
                Error_Detected = true;
                /* try again or abort? */
                break;
            }
        } else {
            /* increment timer - exit if timed out */
            elapsed_seconds += (current_seconds - last_seconds);
            if (elapsed_seconds > timeout_seconds) {
                Error_Detected = true;
                printf("\rError: APDU Timeout!\n");
                break;
            }
        }

        /* returns 0 bytes on timeout */
        pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout);
	//if(pdu_len != 0)
	  fprintf(stderr, "received %d bytes\r\n", pdu_len);

        /* process */
        if (pdu_len) {
            npdu_handler(&src, &Rx_Buf[0], pdu_len);
        }

        /* keep track of time for next check */
        last_seconds = current_seconds;
	Sleep(777);
    }
    if (Error_Detected) {
        return 1;
    }
    return 0;
}
