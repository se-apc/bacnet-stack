/**
 * @file
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2014
 * @brief The Integer Value object is an object with a present-value that
 * uses an INTEGER data type.
 * @copyright SPDX-License-Identifier: MIT
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/bacdcode.h"
#include "bacnet/bacapp.h"
#include "bacnet/bactext.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/services.h"
/* me! */
#include "bacnet/basic/object/iv.h"

#ifndef MAX_INTEGER_VALUES
#define MAX_INTEGER_VALUES 1
#endif

struct integer_object {
    bool Out_Of_Service : 1;
    int32_t Present_Value;
    uint16_t Units;
};
static struct integer_object Integer_Value[MAX_INTEGER_VALUES];

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Integer_Value_Properties_Required[] = { PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_PRESENT_VALUE, PROP_STATUS_FLAGS,
    PROP_UNITS, -1 };

static const int Integer_Value_Properties_Optional[] = { PROP_OUT_OF_SERVICE,
    -1 };

static const int Integer_Value_Properties_Proprietary[] = { -1 };

/**
 * Returns the list of required, optional, and proprietary properties.
 * Used by ReadPropertyMultiple service.
 *
 * @param pRequired - pointer to list of int terminated by -1, of
 * BACnet required properties for this object.
 * @param pOptional - pointer to list of int terminated by -1, of
 * BACnet optkional properties for this object.
 * @param pProprietary - pointer to list of int terminated by -1, of
 * BACnet proprietary properties for this object.
 */
void Integer_Value_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Integer_Value_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Integer_Value_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Integer_Value_Properties_Proprietary;
    }

    return;
}

/**
 * Determines if a given Integer Value instance is valid
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  true if the instance is valid, and false if not
 */
bool Integer_Value_Valid_Instance(uint32_t object_instance)
{
    unsigned int index;

    index = Integer_Value_Instance_To_Index(object_instance);
    if (index < MAX_INTEGER_VALUES) {
        return true;
    }

    return false;
}

/**
 * Determines the number of Integer Value objects
 *
 * @return  Number of Integer Value objects
 */
unsigned Integer_Value_Count(void)
{
    return MAX_INTEGER_VALUES;
}

/**
 * Determines the object instance-number for a given 0..N index
 * of Integer Value objects where N is Integer_Value_Count().
 *
 * @param  index - 0..MAX_INTEGER_VALUES value
 *
 * @return  object instance-number for the given index
 */
uint32_t Integer_Value_Index_To_Instance(unsigned index)
{
    return index;
}

/**
 * For a given object instance-number, determines a 0..N index
 * of Integer Value objects where N is Integer_Value_Count().
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  index for the given instance-number, or MAX_INTEGER_VALUES
 * if not valid.
 */
unsigned Integer_Value_Instance_To_Index(uint32_t object_instance)
{
    unsigned index = MAX_INTEGER_VALUES;

    if (object_instance < MAX_INTEGER_VALUES) {
        index = object_instance;
    }

    return index;
}

/**
 * For a given object instance-number, determines the present-value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  present-value of the object
 */
int32_t Integer_Value_Present_Value(uint32_t object_instance)
{
    int32_t value = 0;
    unsigned int index;

    index = Integer_Value_Instance_To_Index(object_instance);
    if (index < MAX_INTEGER_VALUES) {
        value = Integer_Value[index].Present_Value;
    }

    return value;
}

/**
 * For a given object instance-number, sets the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - integer value
 *
 * @return  true if values are within range and present-value is set.
 */
bool Integer_Value_Present_Value_Set(
    uint32_t object_instance, int32_t value, uint8_t priority)
{
    bool status = false;
    unsigned int index;

    (void)priority;
    index = Integer_Value_Instance_To_Index(object_instance);
    if (index < MAX_INTEGER_VALUES) {
        Integer_Value[index].Present_Value = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, loads the object-name into
 * a characterstring. Note that the object name must be unique
 * within this device.
 *
 * @param  object_instance - object-instance number of the object
 * @param  object_name - holds the object-name retrieved
 *
 * @return  true if object-name was retrieved
 */
bool Integer_Value_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    char text[32] = "";
    unsigned int index;
    bool status = false;

    index = Integer_Value_Instance_To_Index(object_instance);
    if (index < MAX_INTEGER_VALUES) {
        snprintf(text, sizeof(text), "INTEGER VALUE %lu", 
            (unsigned long)object_instance);
        status = characterstring_init_ansi(object_name, text);
    }

    return status;
}

/**
 * For a given object instance-number, returns the units property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  units property value
 */
uint16_t Integer_Value_Units(uint32_t instance)
{
    unsigned int index;
    uint16_t units = UNITS_NO_UNITS;

    index = Integer_Value_Instance_To_Index(instance);
    if (index < MAX_INTEGER_VALUES) {
        units = Integer_Value[index].Units;
    }

    return units;
}

/**
 * For a given object instance-number, sets the units property value
 *
 * @param object_instance - object-instance number of the object
 * @param units - units property value
 *
 * @return true if the units property value was set
 */
bool Integer_Value_Units_Set(uint32_t instance, uint16_t units)
{
    unsigned int index = 0;
    bool status = false;

    index = Integer_Value_Instance_To_Index(instance);
    if (index < MAX_INTEGER_VALUES) {
        Integer_Value[index].Units = units;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, returns the out-of-service
 * property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  out-of-service property value
 */
bool Integer_Value_Out_Of_Service(uint32_t instance)
{
    unsigned int index = 0;
    bool value = false;

    index = Integer_Value_Instance_To_Index(instance);
    if (index < MAX_INTEGER_VALUES) {
        value = Integer_Value[index].Out_Of_Service;
    }

    return value;
}

/**
 * For a given object instance-number, sets the out-of-service property value
 *
 * @param object_instance - object-instance number of the object
 * @param value - boolean out-of-service value
 *
 * @return true if the out-of-service property value was set
 */
void Integer_Value_Out_Of_Service_Set(uint32_t instance, bool value)
{
    unsigned int index = 0;

    index = Integer_Value_Instance_To_Index(instance);
    if (index < MAX_INTEGER_VALUES) {
        Integer_Value[index].Out_Of_Service = value;
    }
}

/**
 * ReadProperty handler for this object.  For the given ReadProperty
 * data, the application_data is loaded or the error flags are set.
 *
 * @param  rpdata - BACNET_READ_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 *
 * @return number of APDU bytes in the response, or
 * BACNET_STATUS_ERROR on error.
 */
int Integer_Value_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0; /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    uint8_t *apdu = NULL;
    uint32_t units = 0;
    int32_t integer_value = 0;
    bool state = false;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }

    apdu = rpdata->application_data;
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], OBJECT_INTEGER_VALUE, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Integer_Value_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0], OBJECT_INTEGER_VALUE);
            break;
        case PROP_PRESENT_VALUE:
            integer_value =
                Integer_Value_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_signed(&apdu[0], integer_value);
            break;
        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            state = Integer_Value_Out_Of_Service(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, state);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_OUT_OF_SERVICE:
            state = Integer_Value_Out_Of_Service(rpdata->object_instance);
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_UNITS:
            units = Integer_Value_Units(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], units);
            break;
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }
    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY_ARRAY) &&
        (rpdata->object_property != PROP_EVENT_TIME_STAMPS) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/**
 * WriteProperty handler for this object.  For the given WriteProperty
 * data, the application_data is loaded or the error flags are set.
 *
 * @param  wp_data - BACNET_WRITE_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 *
 * @return false if an error is loaded, true if no errors
 */
bool Integer_Value_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;

    /* decode the some of the request */
    len = bacapp_decode_application_data(
        wp_data->application_data, wp_data->application_data_len, &value);
    /* FIXME: len < application_data_len: more data? */
    if (len < 0) {
        /* error while decoding - a value larger than we can handle */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    if ((wp_data->object_property != PROP_PRIORITY_ARRAY) &&
        (wp_data->object_property != PROP_EVENT_TIME_STAMPS) &&
        (wp_data->array_index != BACNET_ARRAY_ALL)) {
        /*  only array properties can have array options */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }
    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_SIGNED_INT);
            if (status) {
                Integer_Value_Present_Value_Set(wp_data->object_instance,
                    value.type.Signed_Int, wp_data->priority);
            }
            break;
        case PROP_OUT_OF_SERVICE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                Integer_Value_Out_Of_Service_Set(
                    wp_data->object_instance, value.type.Boolean);
            }
            break;
        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_NAME:
        case PROP_OBJECT_TYPE:
        case PROP_STATUS_FLAGS:
        case PROP_UNITS:
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

/**
 * @brief For a given object instance-number, determines the COV status
 * @param  object_instance - object-instance number of the object
 * @return  true if the COV flag is set
 */
bool Integer_Value_Change_Of_Value(uint32_t object_instance)
{
    bool changed = false;
    struct integer_object *pObject = Integer_Value_Object(object_instance);

    if (pObject) {
        changed = pObject->Changed;
    }

    return changed;
}

/**
 * @brief For a given object instance-number, clears the COV flag
 * @param  object_instance - object-instance number of the object
 */
void Integer_Value_Change_Of_Value_Clear(uint32_t object_instance)
{
    struct integer_object *pObject = Integer_Value_Object(object_instance);

    if (pObject) {
        pObject->Changed = false;
    }
}

/**
 * @brief For a given object instance-number, returns the COV-Increment value
 * @param  object_instance - object-instance number of the object
 * @return  COV-Increment value
 */
uint32_t Integer_Value_COV_Increment(uint32_t object_instance)
{
    uint32_t value = 0;
    struct integer_object *pObject = Integer_Value_Object(object_instance);

    if (pObject) {
        value = pObject->COV_Increment;
    }

    return value;
}

/**
 * For a given object instance-number, loads the value_list with the COV data.
 *
 * @param  object_instance - object-instance number of the object
 * @param  value_list - list of COV data
 *
 * @return  true if the value list is encoded
 */
bool Integer_Value_Encode_Value_List(
    uint32_t object_instance, BACNET_PROPERTY_VALUE *value_list)
{
    bool status = false;
    struct integer_object *pObject = Integer_Value_Object(object_instance);

    if (pObject) {
        bool out_of_service = pObject->Out_Of_Service;
        uint32_t present_value = pObject->Present_Value;
        const bool in_alarm = false;
        const bool fault = false;
        const bool overridden = false;

        status = cov_value_list_encode_signed_int(value_list, present_value,
            in_alarm, fault, overridden, out_of_service);
    }

    return status;
}

/**
 * @brief For a given object instance-number, sets the COV-Increment value
 * @param  object_instance - object-instance number of the object
 * @param  value - COV-Increment value
 */
void Integer_Value_COV_Increment_Set(uint32_t object_instance, uint32_t value)
{
    struct integer_object *pObject = Integer_Value_Object(object_instance);

    if (pObject) {
        pObject->COV_Increment = value;
        Integer_Value_COV_Detect(pObject, pObject->Present_Value);
    }
}

/**
 * @brief Creates a Integer Value object
 * @param object_instance - object-instance number of the object
 * @return the object-instance that was created, or BACNET_MAX_INSTANCE
 */
uint32_t Integer_Value_Create(uint32_t object_instance)
{
    struct integer_object *pObject = NULL;

    if (object_instance > BACNET_MAX_INSTANCE) {
        return BACNET_MAX_INSTANCE;
    } else if (object_instance == BACNET_MAX_INSTANCE) {
        /* wildcard instance */
        /* the Object_Identifier property of the newly created object
            shall be initialized to a value that is unique within the
            responding BACnet-user device. The method used to generate
            the object identifier is a local matter.*/
        object_instance = Keylist_Next_Empty_Key(Object_List, 1);
    }
    pObject = Keylist_Data(Object_List, object_instance);
    if (!pObject) {
        pObject = calloc(1, sizeof(struct integer_object));
        if (pObject) {
            int index = Keylist_Data_Add(Object_List, object_instance, pObject);

            if (index < 0) {
                free(pObject);
                return BACNET_MAX_INSTANCE;
            }

            characterstring_init_ansi(&pObject->Name, "");
            characterstring_init_ansi(&pObject->Description, "");
            pObject->COV_Increment = 1;
            pObject->Present_Value = 0;
            pObject->Prior_Value   = 0;
            pObject->Units = UNITS_PERCENT;
            pObject->Out_Of_Service = false;
            pObject->Changed = false;

            /* add to list */
        } else {
            return BACNET_MAX_INSTANCE;
        }
    }

    return object_instance;
}

/**
 * @brief Deletes an Integer Value object
 * @param object_instance - object-instance number of the object
 * @return true if the object-instance was deleted
 */
bool Integer_Value_Delete(uint32_t object_instance)
{
    bool status = false;
    struct integer_object *pObject = Keylist_Data_Delete(Object_List, object_instance);

    if (pObject) {
        free(pObject);
        status = true;
    }

    return status;
}

/**
 * @brief Deletes all the Integer Values and their data
 */
void Integer_Value_Cleanup(void)
{
    if (Object_List) {
        struct integer_object *pObject;

        do {
            pObject = Keylist_Data_Pop(Object_List);
            if (pObject) {
                free(pObject);
            }
        } while (pObject);

        Keylist_Delete(Object_List);
        Object_List = NULL;
    }
}

/**
 * Initializes the Integer Value object data
 */
void Integer_Value_Init(void)
{
    unsigned index = 0;

    for (index = 0; index < MAX_INTEGER_VALUES; index++) {
        Integer_Value[index].Present_Value = 0;
        Integer_Value[index].Out_Of_Service = false;
        Integer_Value[index].Units = UNITS_NO_UNITS;
    }
}
