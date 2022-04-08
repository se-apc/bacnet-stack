/* the bacnet NIF sample: bacnet_nif.c

    This file is a demo of a NIF for use with Erlang/Elixir applications.
    The synchronous functions all map to functions in Elixir NIF Module
*/


#include <erl_nif.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "bacdef.h"
#include "bacenum.h"
#include "device.h"
#include "h_apdu.h"
#include "h_whois.h"
#include "h_noserv.h"
#include "h_rp.h"
#include "h_iam.h"
#include "s_iam.h"
// #include "config.h"
#include "dlenv.h"
#include "bactext.h"

/* EXTERN FUNCTION DECLARATIONS */
extern int foo(int x);
extern int bar(int y);
extern int get_var();
extern bool Routed_Device_Set_Object_Instance_Number(uint32_t object_id);
extern void Device_Init(object_functions_t *object_table);
extern void apdu_set_unconfirmed_handler(
    BACNET_UNCONFIRMED_SERVICE service_choice, unconfirmed_function pFunction);
extern void apdu_set_unrecognized_service_handler_handler(confirmed_function pFunction);
extern void apdu_set_confirmed_handler(
    BACNET_CONFIRMED_SERVICE service_choice, confirmed_function pFunction);
extern void apdu_set_abort_handler(abort_function pFunction);
extern void apdu_set_reject_handler(reject_function pFunction);
extern void dlenv_init(void);
extern void Send_I_Am_To_Network(BACNET_ADDRESS* target_address, uint32_t device_id,
                          unsigned int max_apdu, int segmentation,
                          uint16_t vendor_id);

/* STATIC FUNCTION DECLARATIONS */
static void MyAbortHandler(
    BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t abort_reason, bool server);
static void MyRejectHandler(
    BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t reject_reason);


/* STATIC VARIABLE DECLARATIONS */
static BACNET_ADDRESS dest = { 0 };

/* parsed command line parameters */
static uint32_t Target_Device_ID = BACNET_MAX_INSTANCE;
static uint16_t Target_Vendor_ID = BACNET_VENDOR_ID;
static unsigned int Target_Max_APDU = MAX_APDU;
static int Target_Segmentation = SEGMENTATION_NONE;

/* flag for signalling errors */
static bool Error_Detected = false;


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

// sample functions
static ERL_NIF_TERM foo_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    int x, ret;
    if (!enif_get_int(env, argv[0], &x)) {
	return enif_make_badarg(env);
    }
    ret = foo(x);
    return enif_make_int(env, ret);
}

// sample functions
static ERL_NIF_TERM bar_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    int y, ret;
    if (!enif_get_int(env, argv[0], &y)) {
	return enif_make_badarg(env);
    }
    ret = bar(y);
    return enif_make_int(env, ret);
}

// sample functions
static ERL_NIF_TERM get_var_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    int ret;

    ret = get_var();
    return enif_make_int(env, ret);
}

// ERL_NIF_TERM == unsigned long, 64b.
static ERL_NIF_TERM set_device_object_instance_number_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    bool func_ret_value = false;
    int arg = 0;
    int result = 0;

    if (!enif_get_int(env, argv[0], &arg)) {
	return enif_make_badarg(env);
    }

    printf("Call the Routed_Device_Set_Object_Instance_Number(%d)\n", arg);

    func_ret_value = Routed_Device_Set_Object_Instance_Number((uint32_t)arg);
    if (func_ret_value) {
        result = 1;
    } else {
        result = 0;
    }

    return enif_make_int(env, result);
}

static ERL_NIF_TERM get_device_object_instance_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    uint32_t dev_object_ret_value = 0;

    printf("Call Routed_Device_Object_Instance_Number and return value\n");
    dev_object_ret_value = Routed_Device_Object_Instance_Number();
    printf("Got object %d\n", (int)dev_object_ret_value);

    return enif_make_int(env, (int)dev_object_ret_value);
}

static ERL_NIF_TERM device_init_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    printf("Initialise the Device\n");
    Device_Init(NULL);
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM set_who_is_handler_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    printf("call apdu_set_unconfirmed_handler() with WHOIS handler\n");
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM set_unrec_service_handler_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    printf("call apdu_set_unrecognized_service_handler_handler()\n");
    apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM set_read_prop_handler_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    printf("call apdu_set_confirmed_handler()\n");
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM set_iam_handler_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    printf("call apdu_set_unconfirmed_handler() with IAM handler\n");
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_add);

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM set_final_handlers_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    printf("set abort and reject handlers\n");
    apdu_set_abort_handler(MyAbortHandler);
    apdu_set_reject_handler(MyRejectHandler);

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM dl_init_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    printf("DataLink INIT\n");
    dlenv_init();

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM send_iam_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    int arg = 0;

    if (!enif_get_int(env, argv[0], &arg)) {
	return enif_make_badarg(env);
    }

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

    return enif_make_atom(env, "ok");
}

/* list of functions declared in corresponding BacnetNif Elixir NIF Module */
static ErlNifFunc nif_funcs[] = {
    // name, arity, local function
    {"foo", 1, foo_nif},
    {"bar", 1, bar_nif},
    {"get_var", 0, get_var_nif},
    {"set_device_object_instance_number", 1, set_device_object_instance_number_nif},
    {"get_device_object_instance", 0, get_device_object_instance_nif},
    {"device_init", 0, device_init_nif},
    {"set_who_is_handler", 0, set_who_is_handler_nif},
    {"set_unrec_service_handler", 0, set_unrec_service_handler_nif},
    {"set_read_prop_handler", 0, set_read_prop_handler_nif},
    {"set_iam_handler", 0, set_iam_handler_nif},
    {"set_final_handlers", 0, set_final_handlers_nif},
    {"dl_init", 0, dl_init_nif},
    {"send_iam", 1, send_iam_nif},

};

/* The name must match the name of the Module using the NIF */
ERL_NIF_INIT(Elixir.BacnetNif, nif_funcs, NULL, NULL, NULL, NULL)

