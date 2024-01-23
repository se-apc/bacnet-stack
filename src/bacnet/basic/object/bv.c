/**************************************************************************
 *
 * Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
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

/* Binary Output Objects - customize for your use */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacapp.h"
#include "bacnet/config.h" /* the custom stuff */
#include "bacnet/wp.h"
#include "bacnet/rp.h"
#include "bacnet/basic/object/bv.h"
#include "bacnet/basic/services.h"

#include "bacnet/basic/sys/debug.h"

#ifndef MAX_BINARY_VALUES
#define MAX_BINARY_VALUES 10
#endif

#define PRINTF debug_perror

/* When all the priorities are level null, the present value returns */
/* the Relinquish Default value */
#define RELINQUISH_DEFAULT BINARY_INACTIVE
/* Here is our Priority Array.*/
static BACNET_BINARY_PV Binary_Value_Level[MAX_BINARY_VALUES]
                                          [BACNET_MAX_PRIORITY];
/* Writable out-of-service allows others to play with our Present Value */
/* without changing the physical output */
static bool Out_Of_Service[MAX_BINARY_VALUES];

typedef struct binary_value_descr {
  uint32_t Instance;
  BACNET_CHARACTER_STRING Name;
  BACNET_CHARACTER_STRING Description;
} BINARY_VALUE_DESCR;

static BINARY_VALUE_DESCR BV_Descr[MAX_BINARY_VALUES];
static int BV_Max_Index = MAX_BINARY_VALUES;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Binary_Value_Properties_Required[] = { PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_PRESENT_VALUE, PROP_STATUS_FLAGS,
    PROP_EVENT_STATE, PROP_OUT_OF_SERVICE, -1 };

static const int Binary_Value_Properties_Optional[] = {
#if (BINARY_VALUE_COMMANDABLE_PV)
    PROP_DESCRIPTION,
    PROP_PRIORITY_ARRAY, PROP_RELINQUISH_DEFAULT,
#else
#endif
-1 };

static const int Binary_Value_Properties_Proprietary[] = { -1 };

/**
 * Initialize the pointers for the required, the optional and the properitary
 * value properties.
 *
 * @param pRequired - Pointer to the pointer of required values.
 * @param pOptional - Pointer to the pointer of optional values.
 * @param pProprietary - Pointer to the pointer of properitary values.
 */
void Binary_Value_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Binary_Value_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Binary_Value_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Binary_Value_Properties_Proprietary;
    }

    return;
}

/**
 * Initialize the binary values.
 */
void Binary_Value_Init(void)
{
    unsigned i, j;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;

        /* initialize all the analog output priority arrays to NULL */
        for (i = 0; i < MAX_BINARY_VALUES; i++) {
            memset(&BV_Descr[i], 0x00, sizeof(BINARY_VALUE_DESCR));
            BV_Descr[i].Instance = BACNET_INSTANCE(BACNET_ID_VALUE(i, OBJECT_BINARY_VALUE));

            for (j = 0; j < BACNET_MAX_PRIORITY; j++) {
                Binary_Value_Level[i][j] = BINARY_NULL;
            }
        }
    }

    return;
}

/**
 * Validate whether the given instance exists in our table.
 *
 * @param object_instance Object instance
 *
 * @return true/false
 */
bool Binary_Value_Valid_Instance(uint32_t object_instance)
{
    unsigned int index;

    for (index = 0; index < BV_Max_Index; index++) {
        if (BV_Descr[index].Instance == object_instance) {
            return true;
        }
    }

    return false;
}


/**
 * Return the count of analog values.
 *
 * @return Count of binary values.
 */
unsigned Binary_Value_Count(void)
{
    return BV_Max_Index;
}

/**
 * @brief Return the instance of an object indexed by index.
 *
 * @param index Object index
 *
 * @return Object instance
 */
uint32_t Binary_Value_Index_To_Instance(unsigned index)
{
    if (index < BV_Max_Index) {
        return BV_Descr[index].Instance;
    } else {
        PRINT("index out of bounds");
    }

    return 0;
}

/**
 * Initialize the analog inputs. Returns false if there are errors.
 *
 * @param pInit_data pointer to initialisation values
 *
 * @return true/false
 */
bool Binary_Value_Set(BACNET_OBJECT_LIST_INIT_T *pInit_data)
{
  unsigned i;

  if (!pInit_data) {
    return false;
  }

  if ((int) pInit_data->length > MAX_BINARY_VALUES) {
    PRINTF("pInit_data->length = %d > %d", (int) pInit_data->length, MAX_BINARY_VALUES);
    return false;
  }
    PRINTF("pInit_data->length = %d >= %d", (int) pInit_data->length, MAX_BINARY_VALUES);

  for (i = 0; i < pInit_data->length; i++) {
    if (pInit_data->Object_Init_Values[i].Object_Instance < BACNET_MAX_INSTANCE) {
      BV_Descr[i].Instance = pInit_data->Object_Init_Values[i].Object_Instance;
    } else {
      PRINTF("Object instance %u is too big", pInit_data->Object_Init_Values[i].Object_Instance);
      return false;
    }

    if (!characterstring_init_ansi(&BV_Descr[i].Name, pInit_data->Object_Init_Values[i].Object_Name)) {
      PRINTF("Fail to set Object name to \"%128s\"", pInit_data->Object_Init_Values[i].Object_Name);
      return false;
    }

    if (!characterstring_init_ansi(&BV_Descr[i].Description, pInit_data->Object_Init_Values[i].Description)) {
      PRINTF("Fail to set Object description to \"%128s\"", pInit_data->Object_Init_Values[i].Description);
      return false;
    }
  }

  BV_Max_Index = (int) pInit_data->length;

  return true;
}

/**
 * Return the index that corresponds to the object instance.
 *
 * @param instance Object Instance
 *
 * @return Object index
 */
unsigned Binary_Value_Instance_To_Index(uint32_t object_instance)
{
    unsigned index = 0;

    for (; index < BV_Max_Index && BV_Descr[index].Instance != object_instance; index++) ;

    return index;
}

/**
 * For a given object instance-number, return the present value.
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  Present value
 */
BACNET_BINARY_PV Binary_Value_Present_Value(uint32_t object_instance)
{
    BACNET_BINARY_PV value = RELINQUISH_DEFAULT;
    unsigned index = 0;
    unsigned i = 0;

    index = Binary_Value_Instance_To_Index(object_instance);
    if (index < MAX_BINARY_VALUES) {
        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
            if (Binary_Value_Level[index][i] != BINARY_NULL) {
                value = Binary_Value_Level[index][i];
                break;
            }
        }
    }

    return value;
}

/**
 * @brief Encode a BACnetARRAY property element
 * @param object_instance [in] BACnet network port object instance number
 * @param priority [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
static int Binary_Value_Priority_Array_Encode(
    uint32_t object_instance, BACNET_ARRAY_INDEX priority, uint8_t *apdu)
{
    int apdu_len = BACNET_STATUS_ERROR;
    BACNET_BINARY_PV value = RELINQUISH_DEFAULT;
    unsigned index = 0;

    index = Binary_Value_Instance_To_Index(object_instance);
    if ((index < BV_Max_Index) && (priority < BACNET_MAX_PRIORITY)) {
        if (Binary_Value_Level[index][priority] != BINARY_NULL) {
            apdu_len = encode_application_null(apdu);
        } else {
            value = Binary_Value_Level[index][priority];
            apdu_len = encode_application_enumerated(apdu, value);
        }
    }

    return apdu_len;
}

/**
 * For a given object instance-number, return the name.
 *
 * Note: the object name must be unique within this device.
 *
 * @param  object_instance - object-instance number of the object
 * @param  object_name - object name pointer
 *
 * @return  true/false
 */
bool Binary_Value_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    bool status = false;
    unsigned index = 0;

    if (!object_name) {
        return false;
    }

    index = Binary_Value_Instance_To_Index(object_instance);
    if (index < BV_Max_Index) {
        *object_name = BV_Descr[index].Name;
        status = true;
    }


    return status;
}

/**
 * For a given object instance-number, return the description.
 *
 * Note: the object name must be unique within this device.
 *
 * @param  object_instance - object-instance number of the object
 * @param  description - description pointer
 *
 * @return  true/false
 */
bool Binary_Value_Description(
    uint32_t object_instance, BACNET_CHARACTER_STRING *description)
{
    bool status = false;
    unsigned index = 0;

    if (!description) {
        return false;
    }

    index = Binary_Value_Instance_To_Index(object_instance);
    if (index < BV_Max_Index) {
        *description = BV_Descr[index].Description;
        status = true;
    }

    return status;
}

/**
 * Return the OOO value, if any.
 *
 * @param instance Object instance.
 *
 * @return true/false
 */
bool Binary_Value_Out_Of_Service(uint32_t instance)
{
    unsigned index = 0;
    bool oos_flag = false;

    index = Binary_Value_Instance_To_Index(instance);
    if (index < BV_Max_Index) {
        oos_flag = Out_Of_Service[index];
    }

    return oos_flag;
}

/**
 * Set the OOO value, if any.
 *
 * @param instance Object instance.
 * @param oos_flag New OOO value.
 */
void Binary_Value_Out_Of_Service_Set(uint32_t instance, bool oos_flag)
{
    unsigned index = 0;

    index = Binary_Value_Instance_To_Index(instance);
    if (index < BV_Max_Index) {
        Out_Of_Service[index] = oos_flag;
    }
}

/**
 * Return the requested property of the binary value.
 *
 * @param rpdata  Property requested, see for BACNET_READ_PROPERTY_DATA details.
 *
 * @return apdu len, or BACNET_STATUS_ERROR on error
 */
int Binary_Value_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0; /* return value */
    int apdu_size = 0;
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    BACNET_BINARY_PV present_value = BINARY_INACTIVE;
    unsigned object_index = 0;
    bool state = false;
    uint8_t *apdu = NULL;

    /* Valid data? */
    if (rpdata == NULL) {
        return 0;
    }
    if ((rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }

    /* Valid object index? */
    object_index = Binary_Value_Instance_To_Index(rpdata->object_instance);
    if (object_index >= BV_Max_Index) {
        rpdata->error_class = ERROR_CLASS_OBJECT;
        rpdata->error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return BACNET_STATUS_ERROR;
    }

    apdu = rpdata->application_data;
    apdu_size = rpdata->application_data_len;
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], OBJECT_BINARY_VALUE, rpdata->object_instance);
            break;
            /* note: Name and Description don't have to be the same.
               You could make Description writable and different */
        case PROP_OBJECT_NAME:
            if (Binary_Value_Object_Name(
                    rpdata->object_instance, &char_string)) {
                apdu_len =
                    encode_application_character_string(&apdu[0], &char_string);
            }
            break;
        case PROP_DESCRIPTION:
            if (Binary_Value_Description(
                    rpdata->object_instance, &char_string)) {
                apdu_len =
                    encode_application_character_string(&apdu[0], &char_string);
            }
            break;
        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0], OBJECT_BINARY_VALUE);
            break;
        case PROP_PRESENT_VALUE:
            present_value = Binary_Value_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], present_value);
            break;
        case PROP_STATUS_FLAGS:
            /* note: see the details in the standard on how to use these */
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            state = Binary_Value_Out_Of_Service(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, state);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_EVENT_STATE:
            /* note: see the details in the standard on how to use this */
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
            break;
        case PROP_OUT_OF_SERVICE:
            state = Binary_Value_Out_Of_Service(rpdata->object_instance);
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_PRIORITY_ARRAY:
            apdu_len = bacnet_array_encode(rpdata->object_instance,
                rpdata->array_index, Binary_Value_Priority_Array_Encode,
                BACNET_MAX_PRIORITY, apdu, apdu_size);
            if (apdu_len == BACNET_STATUS_ABORT) {
                rpdata->error_code =
                    ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
            } else if (apdu_len == BACNET_STATUS_ERROR) {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
            }
            break;
        case PROP_RELINQUISH_DEFAULT:
            present_value = RELINQUISH_DEFAULT;
            apdu_len = encode_application_enumerated(&apdu[0], present_value);
            break;
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }

    /* Only array properties can have array options. */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY_ARRAY) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/**
 * Set the requested property of the binary value.
 *
 * @param wp_data  Property requested, see for BACNET_WRITE_PROPERTY_DATA
 * details.
 *
 * @return true if successful
 */
bool Binary_Value_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    unsigned int object_index = 0;
    unsigned int priority = 0;
    BACNET_BINARY_PV level = BINARY_NULL;
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;

    /* Valid data? */
    if (wp_data == NULL) {
        return false;
    }
    if (wp_data->application_data_len == 0) {
        return false;
    }

    /* Decode the some of the request. */
    len = bacapp_decode_application_data(
        wp_data->application_data, wp_data->application_data_len, &value);
    /* FIXME: len < application_data_len: more data? */
    if (len < 0) {
        /* error while decoding - a value larger than we can handle */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    /* Only array properties can have array options. */
    if ((wp_data->object_property != PROP_PRIORITY_ARRAY) &&
        (wp_data->array_index != BACNET_ARRAY_ALL)) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }

    /* Valid object index? */
    object_index = Binary_Value_Instance_To_Index(wp_data->object_instance);
    if (object_index >= BV_Max_Index) {
        wp_data->error_class = ERROR_CLASS_OBJECT;
        wp_data->error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return false;
    }

    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                priority = wp_data->priority;
                /* Command priority 6 is reserved for use by Minimum On/Off
                   algorithm and may not be used for other purposes in any
                   object. */
                if (priority && (priority <= BACNET_MAX_PRIORITY) &&
                    (priority != 6 /* reserved */) &&
                    (value.type.Enumerated <= MAX_BINARY_PV)) {
                    level = (BACNET_BINARY_PV)value.type.Enumerated;
                    priority--;
                    Binary_Value_Level[object_index][priority] = level;
                    /* Note: you could set the physical output here if we
                       are the highest priority.
                       However, if Out of Service is TRUE, then don't set the
                       physical output.  This comment may apply to the
                       main loop (i.e. check out of service before changing
                       output) */
                    status = true;
                } else if (priority == 6) {
                    /* Command priority 6 is reserved for use by Minimum On/Off
                       algorithm and may not be used for other purposes in any
                       object. */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            } else {
                status = write_property_type_valid(
                    wp_data, &value, BACNET_APPLICATION_TAG_NULL);
                if (status) {
                    level = BINARY_NULL;
                    priority = wp_data->priority;
                    if (priority && (priority <= BACNET_MAX_PRIORITY)) {
                        priority--;
                        Binary_Value_Level[object_index][priority] = level;
                        /* Note: you could set the physical output here to the
                           next highest priority, or to the relinquish default
                           if no priorities are set. However, if Out of Service
                           is TRUE, then don't set the physical output.  This
                           comment may apply to the
                           main loop (i.e. check out of service before changing
                           output) */
                    } else {
                        status = false;
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                    }
                }
            }
            break;
        case PROP_OUT_OF_SERVICE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                Binary_Value_Out_Of_Service_Set(
                    wp_data->object_instance, value.type.Boolean);
            }
            break;
        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_NAME:
        case PROP_DESCRIPTION:
        case PROP_OBJECT_TYPE:
        case PROP_STATUS_FLAGS:
        case PROP_EVENT_STATE:
        case PROP_PRIORITY_ARRAY:
        case PROP_RELINQUISH_DEFAULT:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            break;
    }

    return status;
}
