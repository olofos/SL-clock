#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "unity.h"
#include "display-message.h"
#include "wifi-task.h"
#include "status.h"
#include "log.h"

//////// Constants used in tests ///////////////////////////////////////////////


//////// Stubs /////////////////////////////////////////////////////////////////

extern enum wifi_state wifi_state;
extern struct wifi_ap *wifi_current_ap;
extern int wifi_ap_retries_left;

struct wifi_ap *wifi_first_ap = 0;

struct app_status app_status;

void wifi_ap_connect(const struct wifi_ap *ap)
{
}

void wifi_softap_enable(void)
{
}

void wifi_softap_disable(void)
{
}

void display_post_message(enum display_message message)
{
}

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
}

void tearDown(void)
{
}

//////// Test //////////////////////////////////////////////////////////////////

struct wifi_full_state
{
    enum wifi_state state;
    struct wifi_ap *current_ap;
    int retries_left;
};

struct transition
{
    struct wifi_full_state current_state;
    struct wifi_full_state next_state;
    enum wifi_event event;
};

static void test__wifi_handle_event__verify_transition_table(void)
{
    struct wifi_ap next_ap =    { .ssid = "NEXT",    .password = "PASS", .next = 0 };
    struct wifi_ap current_ap = { .ssid = "CURRENT", .password = "PASS", .next = &next_ap };
    struct wifi_ap first_ap =   { .ssid = "FIRST",   .password = "PASS", .next = 0 };

    wifi_first_ap = &first_ap;

    struct transition transition_tab[] = {
        {
            .current_state = { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = 0,      .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_NO_AP_FOUND,   .current_ap = 0,      .retries_left = 2 },
            .event = WIFI_EVENT_NO_EVENT
        },

        {
            .current_state = { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = &current_ap, .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_AP_CONNECTING, .current_ap = &current_ap, .retries_left = 2 },
            .event = WIFI_EVENT_NO_EVENT
        },

        {
            .current_state = { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = 0,      .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_ERROR,         .current_ap = 0,      .retries_left = 2 },
            .event = WIFI_EVENT_AP_CONNECTED
        },

        {
            .current_state = { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = 0,      .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_ERROR,         .current_ap = 0,      .retries_left = 2 },
            .event = WIFI_EVENT_AP_CONNECTION_FAILED
        },

        {
            .current_state = { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = 0,      .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_ERROR,         .current_ap = 0,      .retries_left = 2 },
            .event = WIFI_EVENT_AP_NOT_FOUND
        },

        {
            .current_state = { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = &current_ap,      .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_ERROR,         .current_ap = &current_ap,      .retries_left = 2 },
            .event = WIFI_EVENT_AP_CONNECTED
        },

        {
            .current_state = { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = &current_ap,      .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_ERROR,         .current_ap = &current_ap,      .retries_left = 2 },
            .event = WIFI_EVENT_AP_CONNECTION_FAILED
        },

        {
            .current_state = { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = &current_ap,      .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_ERROR,         .current_ap = &current_ap,      .retries_left = 2 },
            .event = WIFI_EVENT_AP_NOT_FOUND
        },




        {
            .current_state = { .state = WIFI_STATE_AP_CONNECTING, .current_ap = &current_ap, .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_AP_CONNECTING, .current_ap = &current_ap, .retries_left = 2 },
            .event = WIFI_EVENT_NO_EVENT
        },

        {
            .current_state = { .state = WIFI_STATE_AP_CONNECTING, .current_ap = &current_ap, .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_AP_CONNECTED,  .current_ap = &current_ap, .retries_left = 2 },
            .event = WIFI_EVENT_AP_CONNECTED
        },

        {
            .current_state = { .state = WIFI_STATE_AP_CONNECTING, .current_ap = &current_ap, .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = &current_ap, .retries_left = 1 },
            .event = WIFI_EVENT_AP_CONNECTION_FAILED
        },

        {
            .current_state = { .state = WIFI_STATE_AP_CONNECTING, .current_ap = &current_ap, .retries_left = 1 },
            .next_state =    { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = &next_ap   , .retries_left = WIFI_AP_NUMBER_OF_RETRIES },
            .event = WIFI_EVENT_AP_CONNECTION_FAILED
        },

        {
            .current_state = { .state = WIFI_STATE_AP_CONNECTING, .current_ap = &current_ap, .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = &current_ap, .retries_left = 1 },
            .event = WIFI_EVENT_AP_NOT_FOUND
        },

        {
            .current_state = { .state = WIFI_STATE_AP_CONNECTING, .current_ap = &current_ap, .retries_left = 1 },
            .next_state =    { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = &next_ap,    .retries_left = WIFI_AP_NUMBER_OF_RETRIES },
            .event = WIFI_EVENT_AP_NOT_FOUND
        },



        {
            .current_state = { .state = WIFI_STATE_AP_CONNECTED,  .current_ap = &current_ap, .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_AP_CONNECTED,  .current_ap = &current_ap, .retries_left = 2 },
            .event = WIFI_EVENT_NO_EVENT
        },

        {
            .current_state = { .state = WIFI_STATE_AP_CONNECTED,  .current_ap = &current_ap, .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = &current_ap, .retries_left = WIFI_AP_NUMBER_OF_RETRIES },
            .event = WIFI_EVENT_AP_CONNECTION_LOST
        },


        {
            .current_state = { .state = WIFI_STATE_NO_AP_FOUND,   .current_ap = &current_ap, .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = &first_ap,   .retries_left = WIFI_AP_NUMBER_OF_RETRIES },
            .event = WIFI_EVENT_NO_EVENT
        },



        {
            .current_state = { .state = WIFI_STATE_ERROR,         .current_ap = &current_ap, .retries_left = 2 },
            .next_state =    { .state = WIFI_STATE_NOT_CONNECTED, .current_ap = &first_ap,   .retries_left = WIFI_AP_NUMBER_OF_RETRIES },
            .event = WIFI_EVENT_NO_EVENT
        },

    };


    for(int i = 0; i < sizeof(transition_tab)/sizeof(transition_tab[0]); i++) {
        wifi_state           = transition_tab[i].current_state.state;
        wifi_current_ap      = transition_tab[i].current_state.current_ap;
        wifi_ap_retries_left = transition_tab[i].current_state.retries_left;

        wifi_handle_event(transition_tab[i].event);

        TEST_ASSERT_EQUAL_INT(transition_tab[i].next_state.state,        wifi_state);
        TEST_ASSERT_EQUAL_PTR(transition_tab[i].next_state.current_ap,   wifi_current_ap);
        TEST_ASSERT_EQUAL_INT(transition_tab[i].next_state.retries_left, wifi_ap_retries_left);
    }
}


void test__wifi_handle_event__error_event_should_lead_to_error_state(void)
{
    enum wifi_state states[] = {
        WIFI_STATE_NOT_CONNECTED,
        WIFI_STATE_AP_CONNECTED,
        WIFI_STATE_AP_CONNECTING,
        WIFI_STATE_NO_AP_FOUND,
        WIFI_STATE_ERROR,
    };

    for(int i = 0; i < sizeof(states)/sizeof(states[0]); i++) {
        wifi_state = states[i];
        wifi_current_ap = 0;
        wifi_ap_retries_left = 0;

        wifi_handle_event(WIFI_EVENT_ERROR);

        TEST_ASSERT_EQUAL_INT(WIFI_STATE_ERROR, wifi_state);
    }
}


//////// Main //////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test__wifi_handle_event__verify_transition_table);
    RUN_TEST(test__wifi_handle_event__error_event_should_lead_to_error_state);
    return UNITY_END();
}
