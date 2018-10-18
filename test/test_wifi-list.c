#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <cmocka.h>

#include <string.h>
#include <stdint.h>
#include <stdio.h>

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

int setup(void **state)
{
    wifi_first_ap = 0;
    wifi_first_scan_ap = 0;

    return 0;
}

int teardown(void **state)
{
    struct wifi_ap *ap = wifi_first_ap;

    while(ap) {
        struct wifi_ap *next = ap->next;
        wifi_ap_free(ap);
        ap = next;
    }

    wifi_scan_free_all();

    return 0;
}

//////// Test //////////////////////////////////////////////////////////////////

static void test__wifi_ap_add__should__add_the_ap_to_the_front_1(void **state)
{
    wifi_ap_add("SSID1", "PASS1");

    assert_non_null(wifi_first_ap);
    assert_null(wifi_first_ap->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
}

static void test__wifi_ap_add__should__add_the_ap_to_the_front_2(void **state)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");

    assert_non_null(wifi_first_ap);
    assert_non_null(wifi_first_ap->next);
    assert_null(wifi_first_ap->next->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
    assert_string_equal("SSID2", wifi_first_ap->next->ssid);
    assert_string_equal("PASS2", wifi_first_ap->next->password);
}

static void test__wifi_ap_add__should__do_nothing_if_ssid_is_null(void **state)
{
    wifi_ap_add(NULL, "PASS");

    assert_null(wifi_first_ap);
}

static void test__wifi_ap_add__should__do_nothing_if_password_is_null(void **state)
{
    wifi_ap_add("SSID", NULL);

    assert_null(wifi_first_ap);
}

static void test__wifi_ap_add__should__remove_duplicates_11(void **state)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_add("SSID1", "PASS1");

    assert_non_null(wifi_first_ap);
    assert_null(wifi_first_ap->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
}

static void test__wifi_ap_add__should__remove_duplicates_112(void **state)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_add("SSID1", "PASS1");

    assert_non_null(wifi_first_ap);
    assert_non_null(wifi_first_ap->next);
    assert_null(wifi_first_ap->next->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
    assert_string_equal("SSID2", wifi_first_ap->next->ssid);
    assert_string_equal("PASS2", wifi_first_ap->next->password);
}

static void test__wifi_ap_add__should__remove_duplicates_121(void **state)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");

    assert_non_null(wifi_first_ap);
    assert_non_null(wifi_first_ap->next);
    assert_null(wifi_first_ap->next->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
    assert_string_equal("SSID2", wifi_first_ap->next->ssid);
    assert_string_equal("PASS2", wifi_first_ap->next->password);
}

static void test__wifi_ap_add__should__remove_duplicates_122(void **state)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");

    assert_non_null(wifi_first_ap);
    assert_non_null(wifi_first_ap->next);
    assert_null(wifi_first_ap->next->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
    assert_string_equal("SSID2", wifi_first_ap->next->ssid);
    assert_string_equal("PASS2", wifi_first_ap->next->password);
}


static void test__wifi_ap_add_back__should__add_the_ap_to_the_back_1(void **state)
{
    wifi_ap_add_back("SSID1", "PASS1");

    assert_non_null(wifi_first_ap);
    assert_null(wifi_first_ap->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
}

static void test__wifi_ap_add_back__should__add_the_ap_to_the_back_2(void **state)
{
    wifi_ap_add_back("SSID1", "PASS1");
    wifi_ap_add_back("SSID2", "PASS2");

    assert_non_null(wifi_first_ap);
    assert_non_null(wifi_first_ap->next);
    assert_null(wifi_first_ap->next->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
    assert_string_equal("SSID2", wifi_first_ap->next->ssid);
    assert_string_equal("PASS2", wifi_first_ap->next->password);
}

static void test__wifi_ap_add_back__should__do_nothing_if_ssid_is_null(void **state)
{
    wifi_ap_add_back(NULL, "PASS");

    assert_null(wifi_first_ap);
}

static void test__wifi_ap_add_back__should__do_nothing_if_password_is_null(void **state)
{
    wifi_ap_add_back("SSID", NULL);

    assert_null(wifi_first_ap);
}




static void test__wifi_ap_number__should__return_the_correct_number_0(void **state)
{
    assert_int_equal(0, wifi_ap_number());
}

static void test__wifi_ap_number__should__return_the_correct_number_1(void **state)
{
    wifi_ap_add("SSID1", "PASS1");
    assert_int_equal(1, wifi_ap_number());
}

static void test__wifi_ap_number__should__return_the_correct_number_2(void **state)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_add("SSID2", "PASS2");
    assert_int_equal(2, wifi_ap_number());
}

static void test__wifi_ap_ssid__should__return_null_when_out_of_range_0(void **state)
{
    assert_null(wifi_ap_ssid(0));
    assert_null(wifi_ap_ssid(1));
    assert_null(wifi_ap_ssid(2));
    assert_null(wifi_ap_ssid(3));
}

static void test__wifi_ap_ssid__should__return_null_when_out_of_range_1(void **state)
{
    wifi_ap_add("SSID1", "PASS1");

    assert_null(wifi_ap_ssid(1));
    assert_null(wifi_ap_ssid(2));
    assert_null(wifi_ap_ssid(3));
}

static void test__wifi_ap_ssid__should__return_null_when_out_of_range_2(void **state)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_add("SSID2", "PASS2");

    assert_null(wifi_ap_ssid(2));
    assert_null(wifi_ap_ssid(3));
}


static void test__wifi_ap_password__should__return_null_when_out_of_range_0(void **state)
{
    assert_null(wifi_ap_password(0));
    assert_null(wifi_ap_password(1));
    assert_null(wifi_ap_password(2));
    assert_null(wifi_ap_password(3));
}

static void test__wifi_ap_password__should__return_null_when_out_of_range_1(void **state)
{
    wifi_ap_add("SSID1", "PASS1");

    assert_null(wifi_ap_password(1));
    assert_null(wifi_ap_password(2));
    assert_null(wifi_ap_password(3));
}

static void test__wifi_ap_password__should__return_null_when_out_of_range_2(void **state)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");

    assert_null(wifi_ap_password(2));
    assert_null(wifi_ap_password(3));
}


static void test__wifi_ap_remove__should__do_nothing_when_there_is_no_ap_added(void **state)
{
    wifi_ap_remove("SSID1");

    assert_null(wifi_first_ap);
}

static void test__wifi_ap_remove__should__do_nothing_when_there_is_no_match_1(void **state)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID");

    assert_non_null(wifi_first_ap);
    assert_null(wifi_first_ap->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
}

static void test__wifi_ap_remove__should__do_nothing_when_there_is_no_match_2(void **state)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID");

    assert_non_null(wifi_first_ap);
    assert_non_null(wifi_first_ap->next);
    assert_null(wifi_first_ap->next->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
    assert_string_equal("SSID2", wifi_first_ap->next->ssid);
    assert_string_equal("PASS2", wifi_first_ap->next->password);
}

static void test__wifi_ap_remove__should__remove_the_first_entry_1(void **state)
{
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID1");

    assert_null(wifi_first_ap);
}

static void test__wifi_ap_remove__should__remove_the_first_entry_2(void **state)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID1");

    assert_non_null(wifi_first_ap);
    assert_null(wifi_first_ap->next);
    assert_string_equal(wifi_first_ap->ssid, "SSID2");
}

static void test__wifi_ap_remove__should__remove_the_second_entry_2(void **state)
{
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID2");

    assert_non_null(wifi_first_ap);
    assert_null(wifi_first_ap->next);
    assert_string_equal(wifi_first_ap->ssid, "SSID1");
}

static void test__wifi_ap_remove__should__remove_the_second_entry_3(void **state)
{
    wifi_ap_add("SSID3", "PASS3");
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID2");

    assert_non_null(wifi_first_ap);
    assert_non_null(wifi_first_ap->next);
    assert_null(wifi_first_ap->next->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
    assert_string_equal("SSID3", wifi_first_ap->next->ssid);
    assert_string_equal("PASS3", wifi_first_ap->next->password);
}

static void test__wifi_ap_remove__should__remove_the_third_entry_3(void **state)
{
    wifi_ap_add("SSID3", "PASS3");
    wifi_ap_add("SSID2", "PASS2");
    wifi_ap_add("SSID1", "PASS1");
    wifi_ap_remove("SSID3");

    assert_non_null(wifi_first_ap);
    assert_non_null(wifi_first_ap->next);
    assert_null(wifi_first_ap->next->next);
    assert_string_equal("SSID1", wifi_first_ap->ssid);
    assert_string_equal("PASS1", wifi_first_ap->password);
    assert_string_equal("SSID2", wifi_first_ap->next->ssid);
    assert_string_equal("PASS2", wifi_first_ap->next->password);
}


static void test__wifi_scan_add__should_add_when_empty(void **state)
{
    wifi_scan_add("SSID", -10, 0);

    assert_non_null(wifi_first_scan_ap);
    assert_null(wifi_first_scan_ap->next);

    assert_string_equal("SSID", wifi_first_scan_ap->ssid);
    assert_int_equal(-10, wifi_first_scan_ap->rssi);
    assert_int_equal(0, wifi_first_scan_ap->authmode);
}

static void test__wifi_scan_add__should_add_when_not_empty(void **state)
{
    wifi_scan_add("SSID0", 0, 0);
    wifi_scan_add("SSID1", 1, 1);

    assert_non_null(wifi_first_scan_ap);
    assert_non_null(wifi_first_scan_ap->next);
    assert_null(wifi_first_scan_ap->next->next);

    assert_string_equal("SSID1", wifi_first_scan_ap->ssid);
    assert_string_equal("SSID0", wifi_first_scan_ap->next->ssid);


    assert_int_equal(1, wifi_first_scan_ap->rssi);
    assert_int_equal(0, wifi_first_scan_ap->next->rssi);

    assert_int_equal(1, wifi_first_scan_ap->authmode);
    assert_int_equal(0, wifi_first_scan_ap->next->authmode);
}


const struct CMUnitTest tests_for_wifi_list[] = {
    cmocka_unit_test_setup_teardown(test__wifi_ap_add__should__add_the_ap_to_the_front_1, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_add__should__add_the_ap_to_the_front_2, setup, teardown),

    cmocka_unit_test_setup_teardown(test__wifi_ap_add__should__do_nothing_if_ssid_is_null, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_add__should__do_nothing_if_password_is_null, setup, teardown),

    cmocka_unit_test_setup_teardown(test__wifi_ap_add__should__remove_duplicates_11, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_add__should__remove_duplicates_112, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_add__should__remove_duplicates_121, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_add__should__remove_duplicates_122, setup, teardown),


    cmocka_unit_test_setup_teardown(test__wifi_ap_add_back__should__add_the_ap_to_the_back_1, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_add_back__should__add_the_ap_to_the_back_2, setup, teardown),

    cmocka_unit_test_setup_teardown(test__wifi_ap_add_back__should__do_nothing_if_ssid_is_null, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_add_back__should__do_nothing_if_password_is_null, setup, teardown),


    cmocka_unit_test_setup_teardown(test__wifi_ap_number__should__return_the_correct_number_0, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_number__should__return_the_correct_number_1, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_number__should__return_the_correct_number_2, setup, teardown),

    cmocka_unit_test_setup_teardown(test__wifi_ap_ssid__should__return_null_when_out_of_range_0, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_ssid__should__return_null_when_out_of_range_1, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_ssid__should__return_null_when_out_of_range_2, setup, teardown),

    cmocka_unit_test_setup_teardown(test__wifi_ap_password__should__return_null_when_out_of_range_0, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_password__should__return_null_when_out_of_range_1, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_password__should__return_null_when_out_of_range_2, setup, teardown),

    cmocka_unit_test_setup_teardown(test__wifi_ap_remove__should__do_nothing_when_there_is_no_ap_added, setup, teardown),

    cmocka_unit_test_setup_teardown(test__wifi_ap_remove__should__do_nothing_when_there_is_no_match_1, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_remove__should__do_nothing_when_there_is_no_match_2, setup, teardown),

    cmocka_unit_test_setup_teardown(test__wifi_ap_remove__should__remove_the_first_entry_1, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_remove__should__remove_the_first_entry_2, setup, teardown),

    cmocka_unit_test_setup_teardown(test__wifi_ap_remove__should__remove_the_second_entry_2, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_ap_remove__should__remove_the_second_entry_3, setup, teardown),

    cmocka_unit_test_setup_teardown(test__wifi_ap_remove__should__remove_the_third_entry_3, setup, teardown),


    cmocka_unit_test_setup_teardown(test__wifi_scan_add__should_add_when_empty, setup, teardown),
    cmocka_unit_test_setup_teardown(test__wifi_scan_add__should_add_when_not_empty, setup, teardown),
};

//////// Main //////////////////////////////////////////////////////////////////


int main(void)
{
    int fails = 0;
    fails += cmocka_run_group_tests(tests_for_wifi_list, NULL, NULL);

    return fails;
}
