#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <cmocka.h>

// Hack to avoid including the implementation details of pdjson. This
// allows us to redefine struct json_stream, which simplifies mocking.
#define PDJSON_PRIVATE_H
#include "json.h"

#include "json-util.h"

#include "log.h"

//////// Stubs /////////////////////////////////////////////////////////////////

void log_log(enum log_level level, enum log_system system, const char *fmt, ...)
{
}

//////// Mocks /////////////////////////////////////////////////////////////////

struct json_stream
{
    int num_tokens;
    enum json_type *tokens;

    int num_strings;
    char **strings;

    int current_token;
    int current_string;
};

enum json_type json_next(json_stream *json)
{
    if(json->current_token < json->num_tokens) {
        return json->tokens[json->current_token++];
    } else {
        return JSON_ERROR;
    }
}

const char *json_get_string(json_stream *json, size_t *length)
{
    if(json->current_string < json->num_strings) {
        return json->strings[json->current_string++];
    } else {
        return NULL;
    }
}

//////// Test //////////////////////////////////////////////////////////////////

static void test__json_expect__should__return_true_when_matching(void **state)
{
    enum json_type tokens[] = {
        JSON_OBJECT, JSON_OBJECT_END, JSON_ARRAY, JSON_ARRAY_END, JSON_STRING, JSON_NUMBER, JSON_TRUE, JSON_FALSE, JSON_NULL, JSON_DONE
    };

    json_stream json;
    json.num_tokens = sizeof(tokens)/sizeof(tokens[0]);
    json.tokens = tokens;
    json.current_token = 0;

    for(int i = 0; i < json.num_tokens; i++) {
        assert_true(json_expect(&json, tokens[i]));
    }
}

static void test__json_expect__should__return_false_when_not_matching(void **state)
{
    enum json_type tokens[] = {
        JSON_OBJECT, JSON_OBJECT_END, JSON_ARRAY, JSON_ARRAY_END, JSON_STRING, JSON_NUMBER, JSON_TRUE, JSON_FALSE, JSON_NULL, JSON_DONE
    };

    json_stream json;
    json.num_tokens = sizeof(tokens)/sizeof(tokens[0]);
    json.tokens = tokens;
    json.current_token = 0;

    for(int i = 0; i < json.num_tokens; i++) {
        assert_false(json_expect(&json, JSON_ERROR));
    }
}

static void test__json_skip__should__skip_a_single_token_that_is_not_an_object_or_an_array(void **state)
{
    enum json_type tokens[] = {
        JSON_STRING, JSON_NUMBER, JSON_TRUE, JSON_FALSE, JSON_NULL, JSON_DONE
    };

    json_stream json;
    json.num_tokens = sizeof(tokens)/sizeof(tokens[0]);
    json.tokens = tokens;
    json.current_token = 0;

    for(int i = 0; i < json.num_tokens; i++) {
        json_skip(&json);
        assert_int_equal(i+1, json.current_token);
    }
}

static void test__json_skip__should__skip_a_full_object(void **state)
{
    enum json_type tokens[] = {
        JSON_OBJECT, JSON_STRING, JSON_STRING, JSON_OBJECT_END, JSON_STRING, JSON_STRING
    };

    json_stream json;
    json.num_tokens = sizeof(tokens)/sizeof(tokens[0]);
    json.tokens = tokens;
    json.current_token = 0;

    json_skip(&json);

    assert_int_equal(4, json.current_token);
}

static void test__json_skip__should__skip_a_full_nested_object(void **state)
{
    enum json_type tokens[] = {
        JSON_OBJECT, JSON_OBJECT, JSON_STRING, JSON_STRING, JSON_OBJECT_END, JSON_ARRAY, JSON_ARRAY_END, JSON_OBJECT_END, JSON_STRING, JSON_STRING
    };

    json_stream json;
    json.num_tokens = sizeof(tokens)/sizeof(tokens[0]);
    json.tokens = tokens;
    json.current_token = 0;

    json_skip(&json);

    assert_int_equal(8, json.current_token);
}

static void test__json_skip__should__skip_a_full_array(void **state)
{
    enum json_type tokens[] = {
        JSON_ARRAY, JSON_STRING, JSON_STRING, JSON_ARRAY_END, JSON_STRING, JSON_STRING
    };

    json_stream json;
    json.num_tokens = sizeof(tokens)/sizeof(tokens[0]);
    json.tokens = tokens;
    json.current_token = 0;

    json_skip(&json);

    assert_int_equal(4, json.current_token);
}

static void test__json_skip__should__skip_a_full_nested_array(void **state)
{
    enum json_type tokens[] = {
        JSON_ARRAY, JSON_OBJECT, JSON_STRING, JSON_STRING, JSON_OBJECT_END, JSON_ARRAY, JSON_ARRAY_END, JSON_ARRAY_END, JSON_STRING, JSON_STRING
    };

    json_stream json;
    json.num_tokens = sizeof(tokens)/sizeof(tokens[0]);
    json.tokens = tokens;
    json.current_token = 0;

    json_skip(&json);

    assert_int_equal(8, json.current_token);
}

// TODO: test what happens when we get a premature JSON_DONE or JSON_ERROR


static void test__json_find_names__should__return_the_next_matching_string(void **state)
{
    enum json_type tokens[] = {
        JSON_STRING, JSON_NULL, JSON_STRING, JSON_NULL, JSON_STRING, JSON_NULL, JSON_OBJECT_END
    };

    char *strings[] = {
        "0", "1", "2"
    };

    json_stream json;
    json.num_tokens = sizeof(tokens)/sizeof(tokens[0]);
    json.tokens = tokens;
    json.current_token = 0;
    json.strings = strings;
    json.num_strings = sizeof(strings)/sizeof(strings[0]);
    json.current_string = 0;

    int n;

    n = json_find_names(&json, (const char *[]){"0", "2"}, 2);
    json_skip(&json);
    assert_int_equal(0, n);
    assert_int_equal(json.current_string, 1);

    n = json_find_names(&json, (const char *[]){"0", "2"}, 2);
    json_skip(&json);
    assert_int_equal(1, n);
    assert_int_equal(json.current_string, 3);
}

static void test__json_find_names__should__return_negative_when_no_match_found(void **state)
{
    enum json_type tokens[] = {
        JSON_STRING, JSON_NULL, JSON_STRING, JSON_NULL, JSON_STRING, JSON_NULL, JSON_OBJECT_END
    };

    char *strings[] = {
        "0", "1", "2"
    };

    json_stream json;
    json.num_tokens = sizeof(tokens)/sizeof(tokens[0]);
    json.tokens = tokens;
    json.current_token = 0;
    json.strings = strings;
    json.num_strings = sizeof(strings)/sizeof(strings[0]);
    json.current_string = 0;

    int n;

    n = json_find_names(&json, (const char *[]){"3", "4"}, 2);
    assert_true(n < 0);
}

const struct CMUnitTest tests_for_json_util[] = {
    cmocka_unit_test(test__json_expect__should__return_true_when_matching),
    cmocka_unit_test(test__json_expect__should__return_false_when_not_matching),

    cmocka_unit_test(test__json_skip__should__skip_a_single_token_that_is_not_an_object_or_an_array),
    cmocka_unit_test(test__json_skip__should__skip_a_full_object),
    cmocka_unit_test(test__json_skip__should__skip_a_full_array),

    cmocka_unit_test(test__json_skip__should__skip_a_full_nested_object),
    cmocka_unit_test(test__json_skip__should__skip_a_full_nested_array),

    cmocka_unit_test(test__json_find_names__should__return_the_next_matching_string),
    cmocka_unit_test(test__json_find_names__should__return_negative_when_no_match_found),
};


//////// Main //////////////////////////////////////////////////////////////////


int main(void)
{
    int fails = 0;
    fails += cmocka_run_group_tests(tests_for_json_util, NULL, NULL);

    return fails;
}
