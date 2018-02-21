#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "unity.h"
#include "wifi-task.h"

#include "log.h"


//////// Constants used in tests ///////////////////////////////////////////////


//////// Stubs /////////////////////////////////////////////////////////////////

void log_log(enum log_level level, enum log_system system, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);

    printf("\n");
}

//////// Global variables used for testing /////////////////////////////////////

//////// Helper functions for testing //////////////////////////////////////////

void setUp(void)
{
    wifi_first_ap = 0;
}

void tearDown(void)
{
    struct wifi_ap *ap = wifi_first_ap;

    while(ap) {
        struct wifi_ap *next = ap->next;
        wifi_ap_free(ap);
        ap = next;
    }
}

//////// Test //////////////////////////////////////////////////////////////////

static void test__wifi_ap_add__should__add_the_ap_to_the_front_1(void)
{
    wifi_ap_add("SSID1", "PASS1");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NULL(wifi_first_ap->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
}

static void test__wifi_ap_add__should__add_the_ap_to_the_front_2(void)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NOT_NULL(wifi_first_ap->next);
    TEST_ASSERT_NULL(wifi_first_ap->next->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
    TEST_ASSERT_EQUAL_STRING("SSID2", wifi_first_ap->next->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS2", wifi_first_ap->next->password);
}

static void test__wifi_ap_add__should__do_nothing_if_ssid_is_null(void)
{
    wifi_ap_add(NULL, "PASS");

    TEST_ASSERT_NULL(wifi_first_ap);
}

static void test__wifi_ap_add__should__do_nothing_if_password_is_null(void)
{
    wifi_ap_add("SSID", NULL);

    TEST_ASSERT_NULL(wifi_first_ap);
}

static void test__wifi_ap_add__should__remove_duplicates_11(void)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_add("SSID1", "PASS1");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NULL(wifi_first_ap->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
}

static void test__wifi_ap_add__should__remove_duplicates_112(void)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_add("SSID1", "PASS1");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NOT_NULL(wifi_first_ap->next);
    TEST_ASSERT_NULL(wifi_first_ap->next->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
    TEST_ASSERT_EQUAL_STRING("SSID2", wifi_first_ap->next->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS2", wifi_first_ap->next->password);
}

static void test__wifi_ap_add__should__remove_duplicates_121(void)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NOT_NULL(wifi_first_ap->next);
    TEST_ASSERT_NULL(wifi_first_ap->next->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
    TEST_ASSERT_EQUAL_STRING("SSID2", wifi_first_ap->next->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS2", wifi_first_ap->next->password);
}

static void test__wifi_ap_add__should__remove_duplicates_122(void)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NOT_NULL(wifi_first_ap->next);
    TEST_ASSERT_NULL(wifi_first_ap->next->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
    TEST_ASSERT_EQUAL_STRING("SSID2", wifi_first_ap->next->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS2", wifi_first_ap->next->password);
}


static void test__wifi_ap_add_back__should__add_the_ap_to_the_back_1(void)
{
    wifi_ap_add_back("SSID1", "PASS1");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NULL(wifi_first_ap->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
}

static void test__wifi_ap_add_back__should__add_the_ap_to_the_back_2(void)
{
    wifi_ap_add_back("SSID1", "PASS1");
    wifi_ap_add_back("SSID2", "PASS2");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NOT_NULL(wifi_first_ap->next);
    TEST_ASSERT_NULL(wifi_first_ap->next->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
    TEST_ASSERT_EQUAL_STRING("SSID2", wifi_first_ap->next->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS2", wifi_first_ap->next->password);
}

static void test__wifi_ap_add_back__should__do_nothing_if_ssid_is_null(void)
{
    wifi_ap_add_back(NULL, "PASS");

    TEST_ASSERT_NULL(wifi_first_ap);
}

static void test__wifi_ap_add_back__should__do_nothing_if_password_is_null(void)
{
    wifi_ap_add_back("SSID", NULL);

    TEST_ASSERT_NULL(wifi_first_ap);
}




static void test__wifi_ap_number__should__return_the_correct_number_0(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, wifi_ap_number());
}

static void test__wifi_ap_number__should__return_the_correct_number_1(void)
{
    wifi_ap_add("SSID1", "PASS1");
    TEST_ASSERT_EQUAL_UINT16(1, wifi_ap_number());
}

static void test__wifi_ap_number__should__return_the_correct_number_2(void)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_add("SSID2", "PASS2");
    TEST_ASSERT_EQUAL_UINT16(2, wifi_ap_number());
}

static void test__wifi_ap_ssid__should__return_null_when_out_of_range_0(void)
{
    TEST_ASSERT_NULL(wifi_ap_ssid(0));
    TEST_ASSERT_NULL(wifi_ap_ssid(1));
    TEST_ASSERT_NULL(wifi_ap_ssid(2));
    TEST_ASSERT_NULL(wifi_ap_ssid(3));
}

static void test__wifi_ap_ssid__should__return_null_when_out_of_range_1(void)
{
    wifi_ap_add("SSID1", "PASS1");

    TEST_ASSERT_NULL(wifi_ap_ssid(1));
    TEST_ASSERT_NULL(wifi_ap_ssid(2));
    TEST_ASSERT_NULL(wifi_ap_ssid(3));
}

static void test__wifi_ap_ssid__should__return_null_when_out_of_range_2(void)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_add("SSID2", "PASS2");

    TEST_ASSERT_NULL(wifi_ap_ssid(2));
    TEST_ASSERT_NULL(wifi_ap_ssid(3));
}


static void test__wifi_ap_password__should__return_null_when_out_of_range_0(void)
{
    TEST_ASSERT_NULL(wifi_ap_password(0));
    TEST_ASSERT_NULL(wifi_ap_password(1));
    TEST_ASSERT_NULL(wifi_ap_password(2));
    TEST_ASSERT_NULL(wifi_ap_password(3));
}

static void test__wifi_ap_password__should__return_null_when_out_of_range_1(void)
{
    wifi_ap_add("SSID1", "PASS1");

    TEST_ASSERT_NULL(wifi_ap_password(1));
    TEST_ASSERT_NULL(wifi_ap_password(2));
    TEST_ASSERT_NULL(wifi_ap_password(3));
}

static void test__wifi_ap_password__should__return_null_when_out_of_range_2(void)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");

    TEST_ASSERT_NULL(wifi_ap_password(2));
    TEST_ASSERT_NULL(wifi_ap_password(3));
}


static void test__wifi_ap_remove__should__do_nothing_when_there_is_no_ap_added(void)
{
    wifi_ap_remove("SSID1");

    TEST_ASSERT_NULL(wifi_first_ap);
}

static void test__wifi_ap_remove__should__do_nothing_when_there_is_no_match_1(void)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NULL(wifi_first_ap->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
}

static void test__wifi_ap_remove__should__do_nothing_when_there_is_no_match_2(void)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NOT_NULL(wifi_first_ap->next);
    TEST_ASSERT_NULL(wifi_first_ap->next->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
    TEST_ASSERT_EQUAL_STRING("SSID2", wifi_first_ap->next->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS2", wifi_first_ap->next->password);
}

static void test__wifi_ap_remove__should__remove_the_first_entry_1(void)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID1");

    TEST_ASSERT_NULL(wifi_first_ap);
}

static void test__wifi_ap_remove__should__remove_the_first_entry_2(void)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID1");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NULL(wifi_first_ap->next);
    TEST_ASSERT_EQUAL_STRING(wifi_first_ap->ssid, "SSID2");
}

static void test__wifi_ap_remove__should__remove_the_second_entry_2(void)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID2");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NULL(wifi_first_ap->next);
    TEST_ASSERT_EQUAL_STRING(wifi_first_ap->ssid, "SSID1");
}

static void test__wifi_ap_remove__should__remove_the_second_entry_3(void)
{
    wifi_ap_add("SSID3", "PASS3");
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID2");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NOT_NULL(wifi_first_ap->next);
    TEST_ASSERT_NULL(wifi_first_ap->next->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
    TEST_ASSERT_EQUAL_STRING("SSID3", wifi_first_ap->next->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS3", wifi_first_ap->next->password);
}

static void test__wifi_ap_remove__should__remove_the_third_entry_3(void)
{
    wifi_ap_add("SSID3", "PASS3");
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID3");

    TEST_ASSERT_NOT_NULL(wifi_first_ap);
    TEST_ASSERT_NOT_NULL(wifi_first_ap->next);
    TEST_ASSERT_NULL(wifi_first_ap->next->next);
    TEST_ASSERT_EQUAL_STRING("SSID1", wifi_first_ap->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS1", wifi_first_ap->password);
    TEST_ASSERT_EQUAL_STRING("SSID2", wifi_first_ap->next->ssid);
    TEST_ASSERT_EQUAL_STRING("PASS2", wifi_first_ap->next->password);
}


//////// Main //////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test__wifi_ap_add__should__add_the_ap_to_the_front_1);
    RUN_TEST(test__wifi_ap_add__should__add_the_ap_to_the_front_2);

    RUN_TEST(test__wifi_ap_add__should__do_nothing_if_ssid_is_null);
    RUN_TEST(test__wifi_ap_add__should__do_nothing_if_password_is_null);

    RUN_TEST(test__wifi_ap_add__should__remove_duplicates_11);
    RUN_TEST(test__wifi_ap_add__should__remove_duplicates_112);
    RUN_TEST(test__wifi_ap_add__should__remove_duplicates_121);
    RUN_TEST(test__wifi_ap_add__should__remove_duplicates_122);


    RUN_TEST(test__wifi_ap_add_back__should__add_the_ap_to_the_back_1);
    RUN_TEST(test__wifi_ap_add_back__should__add_the_ap_to_the_back_2);

    RUN_TEST(test__wifi_ap_add_back__should__do_nothing_if_ssid_is_null);
    RUN_TEST(test__wifi_ap_add_back__should__do_nothing_if_password_is_null);


    RUN_TEST(test__wifi_ap_number__should__return_the_correct_number_0);
    RUN_TEST(test__wifi_ap_number__should__return_the_correct_number_1);
    RUN_TEST(test__wifi_ap_number__should__return_the_correct_number_2);

    RUN_TEST(test__wifi_ap_ssid__should__return_null_when_out_of_range_0);
    RUN_TEST(test__wifi_ap_ssid__should__return_null_when_out_of_range_1);
    RUN_TEST(test__wifi_ap_ssid__should__return_null_when_out_of_range_2);

    RUN_TEST(test__wifi_ap_password__should__return_null_when_out_of_range_0);
    RUN_TEST(test__wifi_ap_password__should__return_null_when_out_of_range_1);
    RUN_TEST(test__wifi_ap_password__should__return_null_when_out_of_range_2);

    RUN_TEST(test__wifi_ap_remove__should__do_nothing_when_there_is_no_ap_added);

    RUN_TEST(test__wifi_ap_remove__should__do_nothing_when_there_is_no_match_1);
    RUN_TEST(test__wifi_ap_remove__should__do_nothing_when_there_is_no_match_2);

    RUN_TEST(test__wifi_ap_remove__should__remove_the_first_entry_1);
    RUN_TEST(test__wifi_ap_remove__should__remove_the_first_entry_2);

    RUN_TEST(test__wifi_ap_remove__should__remove_the_second_entry_2);
    RUN_TEST(test__wifi_ap_remove__should__remove_the_second_entry_3);

    RUN_TEST(test__wifi_ap_remove__should__remove_the_third_entry_3);

    return UNITY_END();
}
