#include "erl_driver.h"
//#include "erl_nif_api_funcs.h"
#include "erl_nif.h"

void driver_free(void *ptr)
{
    printf("driver_free\n");
}

ERL_NIF_TERM enif_make_int(ErlNifEnv* env, int i)
{
    printf("enif_make_int()\n");
    return 0;
}

ERL_NIF_TERM enif_make_atom(ErlNifEnv* env, const char* name)
{
    printf("enif_make_atom()\n");
    return 0;
}

int enif_get_int(ErlNifEnv* env, ERL_NIF_TERM term, int* ip)
{
    printf("enif_get_int()\n");
    return 0;
}

void *driver_alloc(ErlDrvSizeT size)
{
    printf("driver_alloc()\n");
    return NULL;
}

int driver_output(ErlDrvPort port, char *buf, ErlDrvSizeT len)
{
    printf("driver_output()\n");
    return 0;
}

ERL_NIF_TERM enif_make_badarg(ErlNifEnv* env)
{
    printf("enif_make_badarg()\n");
    return 0;
}
