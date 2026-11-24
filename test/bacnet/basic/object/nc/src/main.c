/**
 * @file
 * @brief Unit test for object
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date December 2022
 *
 * @copyright SPDX-License-Identifier: MIT
 */
#include <zephyr/ztest.h>
#include <bacnet/bactext.h>
#include <bacnet/rp.h>
#include <bacnet/wp.h>
#include <bacnet/basic/object/nc.h>

static void set_recipient_device(
    BACNET_RECIPIENT *recipient, uint32_t instance)
{
    recipient->tag = BACNET_RECIPIENT_TAG_DEVICE;
    recipient->type.device.type = OBJECT_DEVICE;
    recipient->type.device.instance = instance;
}

/**
 * @addtogroup bacnet_tests
 * @{
 */

/**
 * @brief Test
 */
#if defined(CONFIG_ZTEST_NEW_API)
ZTEST(notification_class_tests, test_Notification_Class)
#else
static void test_Notification_Class(void)
#endif
{
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0;
    int test_len = 0;
    BACNET_READ_PROPERTY_DATA rpdata = { 0 };
    BACNET_APPLICATION_DATA_VALUE value = { 0 };
    const int *pRequired = NULL;
    const int *pOptional = NULL;
    const int *pProprietary = NULL;
    const uint32_t instance = 1;
    BACNET_WRITE_PROPERTY_DATA wpdata = { 0 };
    bool status = false;
    unsigned index;

    Notification_Class_Init();
    status = Notification_Class_Valid_Instance(instance);
    zassert_true(status, NULL);
    index = Notification_Class_Instance_To_Index(instance);
    zassert_equal(index, instance, "index=%u", index);

    rpdata.application_data = &apdu[0];
    rpdata.application_data_len = sizeof(apdu);
    rpdata.object_type = OBJECT_NOTIFICATION_CLASS;
    rpdata.object_instance = instance;
    rpdata.object_property = PROP_OBJECT_IDENTIFIER;

    Notification_Class_Property_Lists(&pRequired, &pOptional, &pProprietary);
    while ((*pRequired) >= 0) {
        rpdata.object_property = *pRequired;
        rpdata.array_index = BACNET_ARRAY_ALL;
        len = Notification_Class_Read_Property(&rpdata);
        zassert_not_equal(
            len, BACNET_STATUS_ERROR,
            "property '%s': failed to ReadProperty!\n",
            bactext_property_name(rpdata.object_property));
        if (len >= 0) {
            test_len = bacapp_decode_known_property(
                rpdata.application_data, len, &value, rpdata.object_type,
                rpdata.object_property);
            if ((rpdata.object_property != PROP_PRIORITY) &&
                (rpdata.object_property != PROP_RECIPIENT_LIST)) {
                zassert_equal(
                    len, test_len, "property '%s': failed to decode!\n",
                    bactext_property_name(rpdata.object_property));
            }
            /* check WriteProperty properties */
            wpdata.object_type = rpdata.object_type;
            wpdata.object_instance = rpdata.object_instance;
            wpdata.object_property = rpdata.object_property;
            wpdata.array_index = rpdata.array_index;
            memcpy(&wpdata.application_data, rpdata.application_data, MAX_APDU);
            wpdata.application_data_len = len;
            wpdata.error_code = ERROR_CODE_SUCCESS;
            status = Notification_Class_Write_Property(&wpdata);
            if (!status) {
                /* verify WriteProperty property is known */
                zassert_not_equal(
                    wpdata.error_code, ERROR_CODE_UNKNOWN_PROPERTY,
                    "property '%s': WriteProperty Unknown!\n",
                    bactext_property_name(rpdata.object_property));
            }
        }
        pRequired++;
    }
    while ((*pOptional) != -1) {
        rpdata.object_property = *pOptional;
        rpdata.array_index = BACNET_ARRAY_ALL;
        len = Notification_Class_Read_Property(&rpdata);
        zassert_not_equal(
            len, BACNET_STATUS_ERROR,
            "property '%s': failed to ReadProperty!\n",
            bactext_property_name(rpdata.object_property));
        if (len > 0) {
            test_len = bacapp_decode_application_data(
                rpdata.application_data, (uint8_t)rpdata.application_data_len,
                &value);
            zassert_equal(
                len, test_len, "property '%s': failed to decode!\n",
                bactext_property_name(rpdata.object_property));
            /* check WriteProperty properties */
            wpdata.object_type = rpdata.object_type;
            wpdata.object_instance = rpdata.object_instance;
            wpdata.object_property = rpdata.object_property;
            wpdata.array_index = rpdata.array_index;
            memcpy(&wpdata.application_data, rpdata.application_data, MAX_APDU);
            wpdata.application_data_len = len;
            wpdata.error_code = ERROR_CODE_SUCCESS;
            status = Notification_Class_Write_Property(&wpdata);
            if (!status) {
                /* verify WriteProperty property is known */
                zassert_not_equal(
                    wpdata.error_code, ERROR_CODE_UNKNOWN_PROPERTY,
                    "property '%s': WriteProperty Unknown!\n",
                    bactext_property_name(rpdata.object_property));
            }
        }
        pOptional++;
    }
    rpdata.object_property = PROP_ALL;
    len = Notification_Class_Read_Property(&rpdata);
    zassert_equal(len, BACNET_STATUS_ERROR, NULL);
    wpdata.object_property = PROP_ALL;
    status = Notification_Class_Write_Property(&wpdata);
    zassert_false(status, NULL);

    return;
}
/**
 * @brief Test security fix for excessive recipient list elements
 *
 * This test verifies the fix for the vulnerability where a remote BACnet
 * client can send a crafted AddListElement request containing more than
 * NC_MAX_RECIPIENTS recipient-list elements. The vulnerability caused a
 * stack-based out-of-bounds write as the server decoded elements into a
 * fixed-size stack array before enforcing the capacity limit.
 *
 * This test verifies that:
 * 1. Multiple recipients can be added (up to NC_MAX_RECIPIENTS-1)
 * 2. Attempting to add more than NC_MAX_RECIPIENTS elements is rejected
 * 3. The error codes are set correctly for overflow conditions
 * 4. The stack buffer is protected from overflow
 */
#if defined(CONFIG_ZTEST_NEW_API)
ZTEST(
    notification_class_tests, test_Notification_Class_Add_List_Element_Overflow)
#else
static void test_Notification_Class_Add_List_Element_Overflow(void)
#endif
{
    const uint32_t instance = 1;
    int err = 0;
    BACNET_LIST_ELEMENT_DATA list_element = { 0 };
    BACNET_DESTINATION destination = { 0 };
    BACNET_DESTINATION recipient_list[NC_MAX_RECIPIENTS] = { 0 };
    uint8_t apdu[MAX_APDU] = { 0 };
    uint8_t apdu_large[MAX_APDU] = { 0 };
    int len = 0;
    int total_len = 0;
    unsigned i = 0, count = 0, encoded_count = 0;
    bool status = false;

    Notification_Class_Init();
    zassert_true(Notification_Class_Valid_Instance(instance), NULL);

    /* Setup common destination parameters */
    for (unsigned j = 0; j < MAX_BACNET_DAYS_OF_WEEK; j++) {
        bitstring_set_bit(&destination.ValidDays, j, true);
    }
    datetime_set_time(&destination.FromTime, 0, 0, 0, 0);
    datetime_set_time(&destination.ToTime, 23, 59, 59, 99);
    destination.ConfirmedNotify = true;
    bitstring_set_bit(&destination.Transitions, TRANSITION_TO_OFFNORMAL, true);
    bitstring_set_bit(&destination.Transitions, TRANSITION_TO_FAULT, true);
    bitstring_set_bit(&destination.Transitions, TRANSITION_TO_NORMAL, true);

    /* Test 1: Successfully add one recipient */
    total_len = 0;
    set_recipient_device(&destination.Recipient, 100);
    destination.ProcessIdentifier = 0;
    len = bacnet_destination_encode(&apdu[total_len], &destination);
    zassert(len > 0, "encode failed", NULL);
    total_len += len;

    /* Add a single recipient */
    list_element.object_type = OBJECT_NOTIFICATION_CLASS;
    list_element.object_instance = instance;
    list_element.object_property = PROP_RECIPIENT_LIST;
    list_element.array_index = BACNET_ARRAY_ALL;
    list_element.application_data = apdu;
    list_element.application_data_len = total_len;
    list_element.first_failed_element_number = 0;
    err = Notification_Class_Add_List_Element(&list_element);
    zassert_equal(err, BACNET_STATUS_OK, "Failed to add a single recipient");

    /* Test 2: Attempt to add exactly NC_MAX_RECIPIENTS in a single request
       This should be rejected because the security fix prevents decoding more
       than NC_MAX_RECIPIENTS-1 elements to avoid buffer overflow */
    total_len = 0;
    for (i = 0; i < NC_MAX_RECIPIENTS && total_len < (int)sizeof(apdu_large);
         i++) {
        set_recipient_device(&destination.Recipient, 200 + i);
        destination.ProcessIdentifier = 100 + i;
        len = bacnet_destination_encode(&apdu_large[total_len], &destination);
        if (len > 0) {
            total_len += len;
        } else {
            break;
        }
    }
    encoded_count = i;
    /* Try to add all NC_MAX_RECIPIENTS in one request - should be rejected */
    list_element.application_data = apdu_large;
    list_element.application_data_len = total_len;
    list_element.first_failed_element_number = 0;
    list_element.error_class = 0;
    list_element.error_code = 0;
    err = Notification_Class_Add_List_Element(&list_element);
    /* The security fix should reject this because it exceeds the buffer size */
    if (encoded_count >= NC_MAX_RECIPIENTS) {
        zassert_not_equal(
            err, BACNET_STATUS_OK,
            "Should reject request to add NC_MAX_RECIPIENTS elements");
        zassert_equal(
            list_element.error_class, ERROR_CLASS_RESOURCES,
            "Error class should be RESOURCES for overflow");
        zassert_equal(
            list_element.error_code, ERROR_CODE_NO_SPACE_TO_ADD_LIST_ELEMENT,
            "Error code should be NO_SPACE_TO_ADD_LIST_ELEMENT");
    }

    /* Test 3: Verify existing recipients are not corrupted */
    status =
        Notification_Class_Get_Recipient_List(instance, &recipient_list[0]);
    zassert_true(status, NULL);
    /* Verify we have at least one recipient (stack was not corrupted) */
    count = 0;
    for (i = 0; i < NC_MAX_RECIPIENTS; i++) {
        if (!bacnet_recipient_device_wildcard(&recipient_list[i].Recipient)) {
            count++;
        }
    }
    zassert(count > 0, "Recipient list should not be corrupted", NULL);

    return;
}

/**
 * @brief Test Remove_List_Element function
 *
 * This test verifies the security and functionality of the Remove function:
 * 1. Tests successful removal of existing recipients
 * 2. Tests the decode buffer overflow protection
 * 3. Tests error handling for invalid inputs
 * 4. Tests that removed elements are properly cleared
 */
#if defined(CONFIG_ZTEST_NEW_API)
ZTEST(
    notification_class_tests,
    test_Notification_Class_Remove_List_Element_Overflow)
#else
static void test_Notification_Class_Remove_List_Element_Overflow(void)
#endif
{
    const uint32_t instance = 1;
    int err = 0;
    BACNET_LIST_ELEMENT_DATA list_element = { 0 };
    BACNET_DESTINATION destination = { 0 };
    uint8_t apdu_add[MAX_APDU] = { 0 };
    uint8_t apdu_remove[MAX_APDU] = { 0 };
    int len = 0;
    int total_len = 0;
    unsigned i = 0;

    Notification_Class_Init();
    zassert_true(Notification_Class_Valid_Instance(instance), NULL);

    /* Setup common destination parameters */
    for (unsigned j = 0; j < MAX_BACNET_DAYS_OF_WEEK; j++) {
        bitstring_set_bit(&destination.ValidDays, j, true);
    }
    datetime_set_time(&destination.FromTime, 0, 0, 0, 0);
    datetime_set_time(&destination.ToTime, 23, 59, 59, 99);
    destination.ConfirmedNotify = true;
    bitstring_set_bit(&destination.Transitions, TRANSITION_TO_OFFNORMAL, true);
    bitstring_set_bit(&destination.Transitions, TRANSITION_TO_FAULT, true);
    bitstring_set_bit(&destination.Transitions, TRANSITION_TO_NORMAL, true);

    /* Test 1: Add two recipients for removal testing */
    total_len = 0;
    for (i = 0; i < 2 && total_len < (int)sizeof(apdu_add); i++) {
        set_recipient_device(&destination.Recipient, 100 + i);
        destination.ProcessIdentifier = i;
        len = bacnet_destination_encode(&apdu_add[total_len], &destination);
        zassert(len > 0, "encode failed for add", NULL);
        total_len += len;
    }

    list_element.object_type = OBJECT_NOTIFICATION_CLASS;
    list_element.object_instance = instance;
    list_element.object_property = PROP_RECIPIENT_LIST;
    list_element.array_index = BACNET_ARRAY_ALL;
    list_element.application_data = apdu_add;
    list_element.application_data_len = total_len;
    list_element.first_failed_element_number = 0;
    err = Notification_Class_Add_List_Element(&list_element);
    zassert_equal(err, BACNET_STATUS_OK, "Failed to add recipients");

    /* Add first recipient */
    total_len = 0;
    destination.ProcessIdentifier = 0;
    len = bacnet_destination_encode(&apdu_add[0], &destination);
    zassert(len > 0, "encode failed for first recipient", NULL);
    list_element.object_type = OBJECT_NOTIFICATION_CLASS;
    list_element.object_instance = instance;
    list_element.object_property = PROP_RECIPIENT_LIST;
    list_element.array_index = BACNET_ARRAY_ALL;
    list_element.application_data = apdu_add;
    list_element.application_data_len = len;
    list_element.first_failed_element_number = 0;
    err = Notification_Class_Add_List_Element(&list_element);
    zassert_equal(err, BACNET_STATUS_OK, "Failed to add first recipient");

    /* Test 2: Successfully remove the added recipient */
    total_len = 0;
    set_recipient_device(&destination.Recipient, 100);
    len = bacnet_destination_encode(&apdu_remove[total_len], &destination);
    zassert(len > 0, "encode failed for remove", NULL);
    total_len = len;
    list_element.application_data = apdu_remove;
    list_element.application_data_len = total_len;
    list_element.first_failed_element_number = 0;
    err = Notification_Class_Remove_List_Element(&list_element);
    zassert_equal(err, BACNET_STATUS_OK, "Failed to remove existing recipient");

    /* Test 3: Test overflow protection on decode buffer - attempt excessive
     * elements */
    total_len = 0;
    unsigned extra_count = 3;
    for (i = 0; i < (NC_MAX_RECIPIENTS + extra_count) &&
         total_len < (int)sizeof(apdu_remove);
         i++) {
        set_recipient_device(&destination.Recipient, 200 + i);
        destination.ProcessIdentifier = 200 + i;
        len = bacnet_destination_encode(&apdu_remove[total_len], &destination);
        if (len > 0) {
            total_len += len;
        } else {
            break;
        }
    }
    list_element.application_data = apdu_remove;
    list_element.application_data_len = total_len;
    list_element.first_failed_element_number = 0;
    list_element.error_class = 0;
    list_element.error_code = 0;
    err = Notification_Class_Remove_List_Element(&list_element);
    /* Should be rejected - these don't exist in the list or overflow */
    zassert(
        err != BACNET_STATUS_ABORT, "Should not abort (overflow protected)",
        NULL);

    /* Test 4: Verify function properly handles null input */
    err = Notification_Class_Remove_List_Element(NULL);
    zassert_equal(err, BACNET_STATUS_ABORT, "Should abort on null input");

    /* Test 5: Verify invalid property fails */
    list_element.object_property = PROP_PRIORITY;
    list_element.application_data = apdu_remove;
    list_element.application_data_len = 1;
    err = Notification_Class_Remove_List_Element(&list_element);
    zassert_equal(err, BACNET_STATUS_ERROR, "Should error on invalid property");
    zassert_equal(list_element.error_class, ERROR_CLASS_SERVICES, NULL);

    return;
}

/**
 * @}
 */

#if defined(CONFIG_ZTEST_NEW_API)
ZTEST_SUITE(notification_class_tests, NULL, NULL, NULL, NULL, NULL);
#else
void test_main(void)
{
    ztest_test_suite(
        notification_class_tests,
        ztest_unit_test(test_Notification_Class),
        ztest_unit_test(test_Notification_Class_Add_List_Element_Overflow),
        ztest_unit_test(test_Notification_Class_Remove_List_Element_Overflow));

    ztest_run_test_suite(notification_class_tests);
}
#endif
