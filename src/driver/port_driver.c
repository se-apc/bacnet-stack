/* the bacnet port_driver.c 

    This file is a demo of a Linked-In Driver for use with Erlang/Elixir applications.
    The synchronous function call bacnet_drv_call is just used as a way to decode data from
    Elixir format into typed data in C.
*/

#include <stdio.h>
#include <stdint.h>
#include <unistd.h> // for sleep()
#include <math.h>
#include "erl_driver.h"
#include "bacdef.h"
#include "bacenum.h"
#include "device.h"
#include "h_apdu.h"
#include "h_whois.h"
#include "h_noserv.h"
#include "h_rp.h"
#include "h_iam.h"
#include "h_npdu.h"
#include "s_iam.h"
#include "s_whois.h"
#include "config.h"
#include "dlenv.h"
#include "bactext.h"
#include "iam.h"
#include "bacaddr.h"
#include "mstimer.h"
#include "address.h"
#include "datalink.h"
#include "ei.h"

/* FUNCTION DECLARATIONS */
unsigned int another_func(char arg);
static void MyAbortHandler(
    BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t abort_reason, bool server);
static void MyRejectHandler(
    BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t reject_reason);
static void my_i_am_handler(
    uint8_t *service_request, uint16_t service_len, BACNET_ADDRESS *src);
static void address_table_add(
    uint32_t device_id, unsigned max_apdu, BACNET_ADDRESS *src);
static struct address_entry *alloc_address_entry(void);
static void print_address_cache(void);
static void print_macaddr(uint8_t *addr, int len);

/* DATA TYPES */

/* Enum of possible functions, which can be called from the Elixir driver Module.
   Each enum value is similarly declared in bacnet_ex.ex as attributes. */
typedef enum function_ref_e {
    SET_OBJECT_INSTANCE = 1,
    GET_OBJECT_INSTANCE,
    DEVICE_INIT,
    SET_WHO_IS_HANDLER,
    SET_UNREC_SERVICE,
    SET_READ_HANDLER,
    SET_IAM_HANDLER,
    SET_FINAL_HANDLERS,
    DL_INIT,
    SEND_IAM,
    SETUP_BACNET_DEVICE,
    SEND_WHOIS,
    CHECK_RX_DATA
} function_ref_t;

#define BAC_ADDRESS_MULT 1
#define SIGNED_FUNCTION 1
#define UNSIGNED_FUNCTION 2

typedef struct {
    ErlDrvPort port;
    //other data here
} bacnet_driver_data;

struct address_entry {
    struct address_entry *next;
    uint8_t Flags;
    uint32_t device_id;
    unsigned max_apdu;
    BACNET_ADDRESS address;
};

/* STATIC VARIABLE DECLARATIONS */
// Linked list of Addresses found with Who-Is service:
static struct address_table {
    struct address_entry *first;
    struct address_entry *last;
} Address_Table = { 0 };

static BACNET_ADDRESS dest = { 0 };

/* parsed command line parameters */
static uint32_t Target_Device_ID = BACNET_MAX_INSTANCE;
static uint16_t Target_Vendor_ID = BACNET_VENDOR_ID;
static unsigned int Target_Max_APDU = MAX_APDU;
static int Target_Segmentation = SEGMENTATION_NONE;

/* global variables used in this file */
static int32_t Target_Object_Instance_Min = -1;
static int32_t Target_Object_Instance_Max = -1;

/* flag for signalling errors */
static bool Error_Detected = false;

/* debug info printing */
static bool BACnet_Debug_Enabled;

/* buffer used for receive */
static uint8_t Rx_Buf[MAX_MPDU] = { 0 };



static ErlDrvData bacnet_drv_start(ErlDrvPort port, char *buff)
{
    bacnet_driver_data *drv_data = (bacnet_driver_data *)driver_alloc(sizeof(bacnet_driver_data));
    drv_data->port = port;
    printf("Started the driver\n");

    if (getenv("BACNET_DEBUG")) {
        BACnet_Debug_Enabled = true;
    }

    return (ErlDrvData)drv_data;
}

static void bacnet_drv_stop(ErlDrvData handle)
{
    printf("Stopping the driver\n");
    driver_free((char *)handle);
}

// long unsigned int == ErlDrvSizeT
static void bacnet_drv_output(ErlDrvData handle, char *buff, ErlDrvSizeT bufflen)
{
    bacnet_driver_data *drv_data = (bacnet_driver_data *)handle;
    char c_arg = 0;
    char c_result = 0;
    bool func_ret_value = false;
    uint32_t dev_object_ret_value = 0;
    void *return_value = NULL;
    ErlDrvSizeT return_len = 0;
    unsigned timeout_milliseconds = 0;
    struct mstimer apdu_timer = { 0 };
    struct mstimer datalink_timer = { 0 };
    BACNET_ADDRESS src = { 0 }; /* address where message came from */
    unsigned int delay_milliseconds = 100;
    uint16_t pdu_len = 0;
    long int long_arg = 0;
    int index = 0;
    int version;
    int arity;
    int type, size;
    double d_arg;
    unsigned long long int ll_arg;

    // first byte is the function number as per function_ref_t
    char fn = buff[0];
    /* second byte is the argument, as long as it's a byte in size
    (otherwise encoding such as in bacnet_drv_call() should be used) */
    char arg = buff[1];

    printf("Driver output function %d, with arg=%d and len = %d\n", (int)fn, (int)arg, (int)bufflen);

    // SETUP_BACNET_DEVICE == function 11
    if (fn == SETUP_BACNET_DEVICE) {
        // maybe verify the arg??
        printf("Setup the Bacnet Device in the driver (%d)\n", (int)arg);
        // ret should be true
        func_ret_value = Routed_Device_Set_Object_Instance_Number((uint32_t)arg);

        // a step for debugging, whether Object instance was saved:
        dev_object_ret_value = Routed_Device_Object_Instance_Number();
        printf("Got object %d\n", (int)dev_object_ret_value);

        Device_Init(NULL);
        /* we need to handle who-is
        to support dynamic device binding to us */
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
        /* set the handler for all the services we don't implement
        It is required to send the proper reject message... */
        apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);
        /* we must implement read property - it's required! */
        apdu_set_confirmed_handler(
            SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
        /* handle the reply (request) coming back */
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_add);
        /* handle any errors coming back */
        apdu_set_abort_handler(MyAbortHandler);
        apdu_set_reject_handler(MyRejectHandler);
        dlenv_init();

        // Hardcode a local network broadcast address:
        dest.mac[0] = 192;
        dest.mac[1] = 168;
        dest.mac[2] = 0;
        dest.mac[3] = 255;
        dest.mac[4] = 0xBA;
        dest.mac[5] = 0xC0;
        dest.mac_len = 6;
        dest.net = BACNET_BROADCAST_NETWORK;
        Target_Device_ID = arg;
        printf("Sending IAM %d\n", (int)arg);
        printf("to Dest = %d:%d:%d:%d  %02X:%02X\n", dest.adr[0], dest.adr[1], dest.adr[2], dest.adr[3], dest.adr[4], dest.adr[5]);
        printf("to MAC = %d:%d:%d:%d  %02X:%02X\n", dest.mac[0], dest.mac[1], dest.mac[2], dest.mac[3], dest.mac[4], dest.mac[5]);
        
        Send_I_Am_To_Network(&dest, Target_Device_ID, Target_Max_APDU,
            Target_Segmentation, Target_Vendor_ID);

    } else if (fn == SEND_WHOIS) {
    
        /* setup my info */
       // maybe verify the arg??
        printf("Setup the Bacnet Device in the driver (%d)\n", (int)arg);
        // ret should be true
        func_ret_value = Routed_Device_Set_Object_Instance_Number((uint32_t)arg);

        // a step for debugging, whether Object instance was saved:
        dev_object_ret_value = Routed_Device_Object_Instance_Number();
        printf("Got object %d\n", (int)dev_object_ret_value);

        Device_Init(NULL);
        /* Note: this applications doesn't need to handle who-is
        it is confusing for the user! */
        /* set the handler for all the services we don't implement
        It is required to send the proper reject message... */
        apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);
        /* we must implement read property - it's required! */
        apdu_set_confirmed_handler(
            SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
        /* handle the reply (request) coming back */
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, my_i_am_handler);
        /* handle any errors coming back */
        apdu_set_abort_handler(MyAbortHandler);
        apdu_set_reject_handler(MyRejectHandler);


        address_init();
        dlenv_init();
        timeout_milliseconds = apdu_timeout();

        mstimer_set(&apdu_timer, timeout_milliseconds);
        mstimer_set(&datalink_timer, 1000);
        /* send the request */
        Send_WhoIs_To_Network(
            &dest, Target_Object_Instance_Min, Target_Object_Instance_Max);

    } else if (fn == CHECK_RX_DATA) {
        unsigned char inter;
        int i=0;

        // for debug, print out all buffer contents:
        printf("Received buffer: ");
        for (i=0; i < bufflen-1; i++) {
            inter = buff[i];
            printf("%u, ", (unsigned int)inter);
        }
        inter = buff[i];
        printf("%u\n\n", (unsigned int)inter);

        // buff[0] is the command argument, buff[1] etc.. is the data.
        // decode the data when term_to_binary() used in Elixir.
        if (ei_decode_version(&buff[1], &index, &version))
        {
            printf("Error occurred decoding version\n");
            return;
        }
        else {
            printf("Version  == %d\n", version);
        }

        // ei_get_type() does not affect the index offset, unlike the decode functions. 
        if (ei_get_type(&buff[1], &index, &type, &size) != 0){
            printf("Error occurred decoding arg type\n");
            return;
        }
        else{
            printf("Type = %d, size = %d\n", type, size);
        }

        switch (type) {
            case ERL_FLOAT_EXT:
                // 1000.1, e.g. 8Bytes
                // ei_decode_double()
                printf("%d is float\n", ERL_FLOAT_EXT);

                if (ei_decode_double(&buff[1], &index, &d_arg)) {
                    printf("Error occurred decoding arg\n");
                    return;
                }
                else {
                    printf("Double/float arg = %f\n", d_arg);
                }
                long_arg = (long int)round(d_arg);
                break;

            case ERL_SMALL_INTEGER_EXT:
                // 0 - 255, maps to a char
                // ei_decode_char()
                printf("%d small int\n", ERL_SMALL_INTEGER_EXT);

                if (ei_decode_char(&buff[1], &index, &c_arg) == -1) {
                    printf("Error occurred decoding char arg\n");
                    return;
                }
                else {
                    printf("Char Arg = %u\n", (unsigned int)c_arg);
                }
                long_arg = (long int)c_arg;
                break;

            case ERL_INTEGER_EXT:
                // over 255, =< 0x7FFF_FFFF, or >= negative -2147483648 (0x8000_0000 equivalent)
                // ei_decode_long()
                // ei_decode_ulong()
                printf("%d is integer\n", ERL_INTEGER_EXT);
                if (ei_decode_long(&buff[1], &index, &long_arg) == -1) {
                    printf("Error occurred decoding long arg\n");
                    return;
                }
                else {
                    printf("Long/Int Arg = %ld\n", long_arg);
                }
                break;

            case ERL_SMALL_BIG_EXT:
                // very large values, 0x8000_0000 or greater, or less than -2147483648 (0x8000_0000 equivalent)
                // ei_decode_longlong()
                // ei_decode_ulonglong()
                printf("%d is small big\n", ERL_SMALL_BIG_EXT);
                if (ei_decode_longlong(&buff[1], &index, &ll_arg) == -1) {
                    printf("Error occurred decoding small big arg\n");
                    return;
                }
                else {
                    printf("Small Big LL Arg = %lld\n", ll_arg);
                }
                long_arg = (long int)ll_arg;
                break;

            case ERL_LARGE_BIG_EXT:
                // GMP types
                // ei_decode_bignum()
                printf("%d is large big\n", ERL_LARGE_BIG_EXT);
                break;
        }

        // just something to check pass in delay argument is positive, otherwise use the default 100
        if (long_arg > 0) {
            delay_milliseconds = (unsigned int)long_arg;
        }

        /* returns 0 bytes on timeout */
        // datalink_receive == bip_receive() in IPv4 implementation
        pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU,
            delay_milliseconds);
        /* process */
        if (pdu_len) {
            npdu_handler(&src, &Rx_Buf[0], pdu_len);
        }
    
        print_address_cache();

        if (Address_Table.first != 0)
        {
            // returned value sent to handle_info() callback of the 
            printf("Device %d discovered\n", (int)Address_Table.first->device_id);
            return_value = (void *)&(Address_Table.first->device_id);
            return_len = sizeof(uint32_t);
        }

    } else if (fn == SET_OBJECT_INSTANCE) {
        // maybe verify the arg??
        printf("Call the Routed_Device_Set_Object_Instance_Number(%d)\n", (int)arg);
        func_ret_value = Routed_Device_Set_Object_Instance_Number((uint32_t)arg);
        if (func_ret_value) {
            c_result = 1;
        } else {
            c_result = 0;
        }
        // this is a blocking call from the Elixir side too.
        //sleep(5);
        return_value = (void *)&c_result;
        return_len = sizeof(char);
    } else if (fn == GET_OBJECT_INSTANCE) {
        printf("Call Device_Object_Instance_Number and return value\n");
        dev_object_ret_value = Device_Object_Instance_Number();
        printf("Got object %d\n", (int)dev_object_ret_value);
        return_value = (void *)&dev_object_ret_value;
        return_len = sizeof(uint32_t);
    } else if (fn == DEVICE_INIT) {
        printf("Initialise the Device\n");
        Device_Init(NULL);
    } else if (fn == SET_WHO_IS_HANDLER) {
        printf("call apdu_set_unconfirmed_handler() with WHOIS handler\n");
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    } else if (fn == SET_UNREC_SERVICE) {
        printf("call apdu_set_unrecognized_service_handler_handler()\n");
        apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);
    } else if (fn == SET_READ_HANDLER) {
        printf("call apdu_set_confirmed_handler()\n");
        apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
    } else if (fn == SET_IAM_HANDLER) {
        printf("call apdu_set_unconfirmed_handler() with IAM handler\n");
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_add);
    } else if (fn == SET_FINAL_HANDLERS) {
        printf("set abort and reject handlers\n");
        apdu_set_abort_handler(MyAbortHandler);
        apdu_set_reject_handler(MyRejectHandler);
    } else if (fn == DL_INIT) {
        printf("DataLink INIT\n");
        dlenv_init();
    } else if (fn == SEND_IAM) {
        printf("Send IAM packet\n");
        // Hardcode a local network broadcast address:
        dest.mac[0] = 192;
        dest.mac[1] = 168;
        dest.mac[2] = 0;
        dest.mac[3] = 255;
        dest.mac[4] = 0xBA;
        dest.mac[5] = 0xC0;
        dest.mac_len = 6;
        dest.net = BACNET_BROADCAST_NETWORK;
        Target_Device_ID = arg;
        printf("Sending IAM %d\n", (int)arg);
        printf("to Dest = %d:%d:%d:%d  %02X:%02X\n", dest.adr[0], dest.adr[1], dest.adr[2], dest.adr[3], dest.adr[4], dest.adr[5]);
        printf("to MAC = %d:%d:%d:%d  %02X:%02X\n", dest.mac[0], dest.mac[1], dest.mac[2], dest.mac[3], dest.mac[4], dest.mac[5]);
        
        Send_I_Am_To_Network(&dest, Target_Device_ID, Target_Max_APDU,
            Target_Segmentation, Target_Vendor_ID);
    } else {
        printf("no supported function mapped -> %d\n", (int)fn);
        // can return NULL and length of 0.
    }

    //send callback to Elixir calling module in handle_call: port, buffer, bufferlen.
    driver_output(drv_data->port, (char *)return_value, return_len);
}

void bacnet_drv_ready_input(ErlDrvData handle, ErlDrvEvent event)
{
    bacnet_driver_data *drv_data = (bacnet_driver_data *)handle;
    printf("Input ready\n");
}

void bacnet_drv_ready_output(ErlDrvData handle, ErlDrvEvent event)
{
    printf("Output ready\n");
}

// Synchronous function call from Elixir Driver Module
ErlDrvSSizeT bacnet_drv_call(ErlDrvData handle,
            unsigned int command, char *buf, ErlDrvSizeT len,
            char **rbuf, ErlDrvSizeT rlen,
            unsigned int *flags)
{
    bacnet_driver_data *drv_data = (bacnet_driver_data *)handle;

    double d_arg;
    long int res;
    int version, index = 0;
    char c_arg;
    int type, size;
    unsigned long int l_arg;
    signed long int sl_arg;
    unsigned long long int ll_arg;
    signed long long int sll_arg;

    printf("Driver Call function\n");

    unsigned char inter;

    printf("Received buffer: ");
    for (int i=0; i < len; i++) {
        inter = buf[i];
        printf("%u, ", (unsigned int)inter);
    }
    printf("\n\n");

    if (ei_decode_version(buf, &index, &version))
    {
        printf("Error occurred decoding version\n");
        return((ErlDrvSSizeT) ERL_DRV_ERROR_GENERAL);
    }
    else {
        printf("Version  == %d\n", version);
    }
    if (ei_get_type(buf, &index, &type, &size) != 0){
        printf("Error occurred decoding arg type\n");
        return((ErlDrvSSizeT) ERL_DRV_ERROR_BADARG);
    }
    else{
        printf("Type = %d, size = %d\n", type, size);
    }

    /* A developer would need to know up-front whether the type they expect in C
       will be neg or not, to use signed or unsigned version of the call. */
    if (command == SIGNED_FUNCTION) {
        switch (type) {
            case ERL_FLOAT_EXT:
                // e.g. 1000.1, -200.123, represented by 8 Bytes
                // ei_decode_double()
                printf("%d is float\n", ERL_FLOAT_EXT);

                if (ei_decode_double(buf, &index, &d_arg)) {
                    printf("Error occurred decoding arg\n");
                    return((ErlDrvSSizeT) ERL_DRV_ERROR_BADARG);
                }
                else {
                    printf("Double/float arg = %f\n", d_arg);
                }
                break;

            case ERL_SMALL_INTEGER_EXT:
                // 0 - 255, maps to a char (positive values only)
                // ei_decode_char()
                printf("%d small int\n", ERL_SMALL_INTEGER_EXT);

                if (ei_decode_char(buf, &index, &c_arg) == -1) {
                    printf("Error occurred decoding char arg\n");
                    return((ErlDrvSSizeT) ERL_DRV_ERROR_BADARG);
                }
                else {
                    printf("Char Arg = %u\n", (unsigned int)c_arg);
                }
                break;

            case ERL_INTEGER_EXT:
                // over 255, =< 0x7FFF_FFFF, or >= negative -2147483648 (0x8000_0000 equivalent)
                // ei_decode_long()
                // ei_decode_ulong()
                printf("%d is integer\n", ERL_INTEGER_EXT);
                if (ei_decode_long(buf, &index, &sl_arg) == -1) {
                    printf("Error occurred decoding long arg\n");
                    return((ErlDrvSSizeT) ERL_DRV_ERROR_BADARG);
                }
                else {
                    printf("Long/Int Arg = %ld\n", sl_arg);
                }
                break;

            case ERL_SMALL_BIG_EXT:
                // very large values, 0x8000_0000 or greater, or less than -2147483648 (0x8000_0000 equivalent)
                // ei_decode_longlong()
                // ei_decode_ulonglong()
                printf("%d is small big\n", ERL_SMALL_BIG_EXT);
                if (ei_decode_longlong(buf, &index, &sll_arg) == -1) {
                    printf("Error occurred decoding small big arg\n");
                    return((ErlDrvSSizeT) ERL_DRV_ERROR_BADARG);
                }
                else {
                    printf("Small Big LL Arg = %lld\n", sll_arg);
                }  
                break;

            case ERL_LARGE_BIG_EXT:
                // GMP types
                // ei_decode_bignum()
                printf("%d is large big\n", ERL_LARGE_BIG_EXT);
                break;
        }

        // Return a Long integer, a random value:
        res = (long)33;

    } else if (command == UNSIGNED_FUNCTION) {
        switch (type) {
            case ERL_FLOAT_EXT:
                // e.g. 1000.1, -200.123, represented by 8 Bytes
                // ei_decode_double()
                printf("%d is float\n", ERL_FLOAT_EXT);

                if (ei_decode_double(buf, &index, &d_arg)) {
                    printf("Error occurred decoding arg\n");
                    return((ErlDrvSSizeT) ERL_DRV_ERROR_BADARG);
                }
                else {
                    printf("Double/floag arg = %f\n", d_arg);
                }
                break;
            case ERL_SMALL_INTEGER_EXT:
                // 0 - 255, maps to a char
                // ei_decode_char()
                printf("%d small int\n", ERL_SMALL_INTEGER_EXT);

                if (ei_decode_char(buf, &index, &c_arg) == -1) {
                    printf("Error occurred decoding char arg\n");
                    return((ErlDrvSSizeT) ERL_DRV_ERROR_BADARG);
                }
                else {
                    printf("Char Arg = %u\n", (unsigned int)c_arg);
                }
                break;
            case ERL_INTEGER_EXT:
                // over 255, =< 0x7FFF_FFFF
                // ei_decode_long()
                // ei_decode_ulong()
                printf("%d is integer\n", ERL_INTEGER_EXT);
                if (ei_decode_ulong(buf, &index, &l_arg) == -1) {
                    printf("Error occurred decoding unsigned long arg\n");
                    return((ErlDrvSSizeT) ERL_DRV_ERROR_BADARG);
                }
                else {
                    printf("Unsigned Long/Int Arg = %lu\n", l_arg);
                }
                break;

            case ERL_SMALL_BIG_EXT:
                // very large values, 0x8000_0000 or greater.
                // ei_decode_longlong()
                // ei_decode_ulonglong()
                printf("%d is small big\n", ERL_SMALL_BIG_EXT);
                if (ei_decode_ulonglong(buf, &index, &ll_arg) == -1) {
                    printf("Error occurred decoding unsigned small big arg\n");
                    return((ErlDrvSSizeT) ERL_DRV_ERROR_BADARG);
                }
                else {
                    printf("Unsigned Small Big LL Arg = %llu\n", ll_arg);
                }     
                break;

            case ERL_LARGE_BIG_EXT:
                // GMP types
                // ei_decode_bignum()
                printf("%d is large big\n", ERL_LARGE_BIG_EXT);
                break;
        }

        // Return a Long integer, a random value:
        res = (long)44;

    } else {
        // A driver should handle potential bad data
        return((ErlDrvSSizeT) ERL_DRV_ERROR_BADARG);
    }

    // index should be reset prior to use.
    index = 0;
    // Version is always the first term to be added to a buffer.
    if (ei_encode_version(*rbuf, &index) != 0) {
        return((ErlDrvSSizeT) ERL_DRV_ERROR_ERRNO);
    }

    // encode the long integer value set in the if(command) block, i.e. 33 and 44.
    if (ei_encode_long(*rbuf, &index, res) != 0) {
        return((ErlDrvSSizeT) ERL_DRV_ERROR_ERRNO);
    }
    else {
        // return length of buffer that was set.
        return((ErlDrvSSizeT) index);
    }
}

static void MyAbortHandler(
    BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t abort_reason, bool server)
{
    (void)src;
    (void)invoke_id;
    (void)server;
    printf("BACnet Abort: %s\n", bactext_abort_reason_name(abort_reason));
    Error_Detected = true;
}

static void MyRejectHandler(
    BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t reject_reason)
{
    (void)src;
    (void)invoke_id;
    printf("BACnet Reject: %s\n", bactext_reject_reason_name(reject_reason));
    Error_Detected = true;
}

static void my_i_am_handler(
    uint8_t *service_request, uint16_t service_len, BACNET_ADDRESS *src)
{
    int len = 0;
    uint32_t device_id = 0;
    unsigned max_apdu = 0;
    int segmentation = 0;
    uint16_t vendor_id = 0;
    unsigned i = 0;

    (void)service_len;

    printf("Received IAM Handler back\n");
    len = iam_decode_service_request(
        service_request, &device_id, &max_apdu, &segmentation, &vendor_id);
    if (BACnet_Debug_Enabled) {
        fprintf(stderr, "Received I-Am Request");
    }
    if (len != -1) {
        if (BACnet_Debug_Enabled) {
            fprintf(stderr, " from %lu, MAC = ", (unsigned long)device_id);
            if ((src->mac_len == 6) && (src->len == 0)) {
                fprintf(stderr, "%u.%u.%u.%u %02X%02X\n", (unsigned)src->mac[0],
                    (unsigned)src->mac[1], (unsigned)src->mac[2],
                    (unsigned)src->mac[3], (unsigned)src->mac[4],
                    (unsigned)src->mac[5]);
            } else {
                for (i = 0; i < src->mac_len; i++) {
                    fprintf(stderr, "%02X", (unsigned)src->mac[i]);
                    if (i < (src->mac_len - 1)) {
                        fprintf(stderr, ":");
                    }
                }
                fprintf(stderr, "\n");
            }
        }
        address_table_add(device_id, max_apdu, src);
    } else {
        if (BACnet_Debug_Enabled) {
            fprintf(stderr, ", but unable to decode it.\n");
        }
    }

    return;
}


static void address_table_add(
    uint32_t device_id, unsigned max_apdu, BACNET_ADDRESS *src)
{
    struct address_entry *pMatch;
    uint8_t flags = 0;

    pMatch = Address_Table.first;
    while (pMatch) {
        if (pMatch->device_id == device_id) {
            if (bacnet_address_same(&pMatch->address, src)) {
                return;
            }
            flags |= BAC_ADDRESS_MULT;
            pMatch->Flags |= BAC_ADDRESS_MULT;
        }
        pMatch = pMatch->next;
    }

    pMatch = alloc_address_entry();

    pMatch->Flags = flags;
    pMatch->device_id = device_id;
    pMatch->max_apdu = max_apdu;
    pMatch->address = *src;

    return;
}
static struct address_entry *alloc_address_entry(void)
{
    struct address_entry *rval;
    rval = (struct address_entry *)calloc(1, sizeof(struct address_entry));
    if (Address_Table.first == 0) {
        Address_Table.first = Address_Table.last = rval;
    } else {
        Address_Table.last->next = rval;
        Address_Table.last = rval;
    }
    return rval;
}

static void print_macaddr(uint8_t *addr, int len)
{
    int j = 0;

    while (j < len) {
        if (j != 0) {
            printf(":");
        }
        printf("%02X", addr[j]);
        j++;
    }
    while (j < MAX_MAC_LEN) {
        printf("   ");
        j++;
    }
}

static void print_address_cache(void)
{
    BACNET_ADDRESS address;
    unsigned total_addresses = 0;
    unsigned dup_addresses = 0;
    struct address_entry *addr;
    uint8_t local_sadr = 0;

    /*  NOTE: this string format is parsed by src/address.c,
       so these must be compatible. */

    printf(";%-7s  %-20s %-5s %-20s %-4s\n", "Device", "MAC (hex)", "SNET",
        "SADR (hex)", "APDU");
    printf(";-------- -------------------- ----- -------------------- ----\n");

    addr = Address_Table.first;
    while (addr) {
        bacnet_address_copy(&address, &addr->address);
        total_addresses++;
        if (addr->Flags & BAC_ADDRESS_MULT) {
            dup_addresses++;
            printf(";");
        } else {
            printf(" ");
        }
        printf(" %-7u ", addr->device_id);
        print_macaddr(address.mac, address.mac_len);
        printf(" %-5hu ", address.net);
        if (address.net) {
            print_macaddr(address.adr, address.len);
        } else {
            print_macaddr(&local_sadr, 1);
        }
        printf(" %-4u ", (unsigned)addr->max_apdu);
        printf("\n");

        addr = addr->next;
    }
    printf(";\n; Total Devices: %u\n", total_addresses);
    if (dup_addresses) {
        printf("; * Duplicate Devices: %u\n", dup_addresses);
    }
}

ErlDrvEntry bacnet_driver_entry = {
    NULL,			/* F_PTR init, called when driver is loaded */
    bacnet_drv_start,		/* L_PTR start, called when port is opened */
    bacnet_drv_stop,		/* F_PTR stop, called when port is closed */
    bacnet_drv_output,		/* F_PTR output, called when erlang has sent */
    bacnet_drv_ready_input,			/* F_PTR ready_input, called when input descriptor ready */
    bacnet_drv_ready_output,			/* F_PTR ready_output, called when output descriptor ready */
    "libbacnet-stack",		/* char *driver_name, the argument to open_port */
    NULL,			/* F_PTR finish, called when unloaded */
    NULL,                       /* void *handle, Reserved by VM */
    NULL,			/* F_PTR control, port_command callback */
    NULL,			/* F_PTR timeout, reserved */
    NULL,			/* F_PTR outputv, reserved */
    NULL,                       /* F_PTR ready_async, only for async drivers */
    NULL,                       /* F_PTR flush, called when port is about 
				   to be closed, but there is data in driver 
				   queue */
    bacnet_drv_call,                       /* F_PTR call, much like control, sync call
				   to driver */
    NULL,                       /* unused */
    ERL_DRV_EXTENDED_MARKER,    /* int extended marker, Should always be 
				   set to indicate driver versioning */
    ERL_DRV_EXTENDED_MAJOR_VERSION, /* int major_version, should always be 
				       set to this value */
    ERL_DRV_EXTENDED_MINOR_VERSION, /* int minor_version, should always be 
				       set to this value */
    0,                          /* int driver_flags, see documentation */
    NULL,                       /* void *handle2, reserved for VM use */
    NULL,                       /* F_PTR process_exit, called when a 
				   monitored process dies */
    NULL                        /* F_PTR stop_select, called to close an 
				   event object */
};

DRIVER_INIT(libbacnet-stack) /* must match name in driver_entry */
{
    return &bacnet_driver_entry;
}

