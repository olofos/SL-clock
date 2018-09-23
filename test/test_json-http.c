#include <stdbool.h>
#include <string.h>

#include "unity.h"

// Hack to avoid including the implementation details of pdjson. This
// allows us to redefine struct json_stream, which simplifies mocking.
#define PDJSON_PRIVATE_H
#include "json.h"

#include "json-http.h"

#include "log.h"

//////// Stubs /////////////////////////////////////////////////////////////////

struct http_json_writer json;

void log_log(enum log_level level, enum log_system system, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);

    printf("\n");
}

int http_peek(struct http_request *request)
{
    return 0;
}

int http_getc(struct http_request *request)
{
    return 0;
}

void json_open_user(json_stream *json, json_user_io get, json_user_io peek, void *user)
{
}

char *output_string = 0;

int http_write_string(struct http_request *request, const char *str)
{
    strcat(output_string, str);
    return strlen(str);
}


//////// Mocks /////////////////////////////////////////////////////////////////


//////// Test //////////////////////////////////////////////////////////////////

static void test__http_json_init__initialises_the_structure(void)
{
    struct http_request request;
    json.current = 0xFF;
    json.request = NULL;

    http_json_init(&json, &request);

    TEST_ASSERT_EQUAL_PTR(&request, json.request);
    TEST_ASSERT_EQUAL_INT(0, json.current);
}

static void test__http_json_begin_object__does_not_write_name_to_output_if_null(void)
{
    http_json_begin_object(&json, NULL);

    TEST_ASSERT_EQUAL_STRING("{", output_string);
}

static void test__http_json_begin_object__pushes_object_to_stack_if_name_is_null(void)
{
    http_json_begin_object(&json, NULL);

    TEST_ASSERT_EQUAL_INT(1, json.current);
    TEST_ASSERT_EQUAL_INT(HTTP_JSON_OBJECT, json.stack[0]);
}


static void test__http_json_begin_object__writes_to_output(void)
{
    http_json_begin_object(&json, NULL);
    http_json_begin_object(&json, "test");

    TEST_ASSERT_EQUAL_STRING("{\"test\":{", output_string);
}

static void test__http_json_begin_object__pushes_object_to_stack(void)
{
    http_json_begin_object(&json, NULL);
    http_json_begin_object(&json, "test");

    TEST_ASSERT_EQUAL_INT(2, json.current);
    TEST_ASSERT_EQUAL_INT(HTTP_JSON_OBJECT, json.stack[1]);
}

static void test__http_json_end_object__writes_to_output(void)
{
    http_json_begin_object(&json, NULL);
    http_json_end_object(&json);

    TEST_ASSERT_EQUAL_STRING("{}", output_string);
}

static void test__http_json_end_object__pops_object_from_stack(void)
{
    http_json_begin_object(&json, NULL);
    http_json_end_object(&json);

    TEST_ASSERT_EQUAL_INT(0, json.current);
}



static void test__http_json_begin_array__does_not_write_name_to_output_if_null(void)
{
    http_json_begin_array(&json, NULL);

    TEST_ASSERT_EQUAL_STRING("[", output_string);
}

static void test__http_json_begin_array__pushes_array_to_stack_if_name_is_null(void)
{
    http_json_begin_array(&json, NULL);

    TEST_ASSERT_EQUAL_INT(1, json.current);
    TEST_ASSERT_EQUAL_INT(HTTP_JSON_ARRAY, json.stack[0]);
}

static void test__http_json_begin_array__writes_to_output(void)
{
    http_json_begin_object(&json, NULL);
    http_json_begin_array(&json, "a");

    TEST_ASSERT_EQUAL_STRING("{\"a\":[", output_string);
}

static void test__http_json_begin_array__pushes_array_to_stack(void)
{
    http_json_begin_object(&json, NULL);
    http_json_begin_array(&json, "a");

    TEST_ASSERT_EQUAL_INT(2, json.current);
    TEST_ASSERT_EQUAL_INT(HTTP_JSON_ARRAY, json.stack[1]);
}

static void test__http_json_end_array__writes_to_output(void)
{
    http_json_begin_array(&json, NULL);
    http_json_end_array(&json);

    TEST_ASSERT_EQUAL_STRING("[]", output_string);
}

static void test__http_json_end_array__pops_array_from_stack(void)
{
    http_json_begin_array(&json, NULL);
    http_json_end_array(&json);

    TEST_ASSERT_EQUAL_INT(0, json.current);
}




static void test__http_json_write_string__writes_to_output(void)
{
    http_json_begin_object(&json, NULL);
    http_json_write_string(&json, "a", "b");

    TEST_ASSERT_EQUAL_STRING("{\"a\":\"b\"", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__http_json_write_string__writes_null_to_output(void)
{
    http_json_begin_object(&json, NULL);
    http_json_write_string(&json, "a", NULL);

    TEST_ASSERT_EQUAL_STRING("{\"a\":null", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__http_json_write_int__writes_to_output(void)
{
    http_json_begin_object(&json, NULL);
    http_json_write_int(&json, "a", 123);

    TEST_ASSERT_EQUAL_STRING("{\"a\":123", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__http_json_write_bool__writes_true_to_output(void)
{
    http_json_begin_object(&json, NULL);
    http_json_write_bool(&json, "a", 1);

    TEST_ASSERT_EQUAL_STRING("{\"a\":true", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__http_json_write_bool__writes_false_to_output(void)
{
    http_json_begin_object(&json, NULL);
    http_json_write_bool(&json, "a", 0);

    TEST_ASSERT_EQUAL_STRING("{\"a\":false", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}



static void test__http_json_begin_object__writes_comma_to_output_in_object(void)
{
    http_json_begin_object(&json, NULL);
    http_json_begin_object(&json, "b");
    http_json_end_object(&json);
    http_json_begin_object(&json, "c");

    TEST_ASSERT_EQUAL_STRING("{\"b\":{},\"c\":{", output_string);
}

static void test__http_json_begin_array__writes_comma_to_output_in_object(void)
{
    http_json_begin_object(&json, NULL);
    http_json_begin_array(&json, "b");
    http_json_end_array(&json);
    http_json_begin_array(&json, "c");

    TEST_ASSERT_EQUAL_STRING("{\"b\":[],\"c\":[", output_string);
}

static void test__http_json_write_string__writes_comma_to_output_in_object(void)
{
    http_json_begin_object(&json, NULL);
    http_json_write_string(&json, "a", "b");
    http_json_write_string(&json, "a", "b");

    TEST_ASSERT_EQUAL_STRING("{\"a\":\"b\",\"a\":\"b\"", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__http_json_write_int__writes_comma_to_output_in_object(void)
{
    http_json_begin_object(&json, NULL);
    http_json_write_string(&json, "a", "b");
    http_json_write_int(&json, "a", 12345);

    TEST_ASSERT_EQUAL_STRING("{\"a\":\"b\",\"a\":12345", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__http_json_write_bool__writes_comma_to_output_in_object(void)
{
    http_json_begin_object(&json, NULL);
    http_json_write_bool(&json, "a", 0);
    http_json_write_bool(&json, "b", 0);
    http_json_write_bool(&json, "c", 1);

    TEST_ASSERT_EQUAL_STRING("{\"a\":false,\"b\":false,\"c\":true", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}





static void test__http_json_begin_object__writes_comma_to_output_in_array(void)
{
    http_json_begin_array(&json, NULL);
    http_json_begin_object(&json, "b");
    http_json_end_object(&json);
    http_json_begin_object(&json, "c");

    TEST_ASSERT_EQUAL_STRING("[\"b\":{},\"c\":{", output_string);
}

static void test__http_json_begin_array__writes_comma_to_output_in_array(void)
{
    http_json_begin_array(&json, NULL);
    http_json_begin_array(&json, "b");
    http_json_end_array(&json);
    http_json_begin_array(&json, "c");

    TEST_ASSERT_EQUAL_STRING("[\"b\":[],\"c\":[", output_string);
}

static void test__http_json_write_string__writes_comma_to_output_in_array(void)
{
    http_json_begin_array(&json, NULL);
    http_json_write_string(&json, "a", "b");
    http_json_write_string(&json, "a", "b");

    TEST_ASSERT_EQUAL_STRING("[\"a\":\"b\",\"a\":\"b\"", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__http_json_write_int__writes_comma_to_output_in_array(void)
{
    http_json_begin_array(&json, NULL);
    http_json_write_string(&json, "a", "b");
    http_json_write_int(&json, "a", 12345);

    TEST_ASSERT_EQUAL_STRING("[\"a\":\"b\",\"a\":12345", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__http_json_write_bool__writes_comma_to_output_in_array(void)
{
    http_json_begin_array(&json, NULL);
    http_json_write_bool(&json, "a", 0);
    http_json_write_bool(&json, "b", 0);
    http_json_write_bool(&json, "c", 1);

    TEST_ASSERT_EQUAL_STRING("[\"a\":false,\"b\":false,\"c\":true", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}


//////// Main //////////////////////////////////////////////////////////////////

void setUp(void) {
    output_string = calloc(1024, 1);
    json.current = 0;
}

void tearDown(void) {
    free(output_string);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test__http_json_init__initialises_the_structure);

    RUN_TEST(test__http_json_begin_object__does_not_write_name_to_output_if_null);
    RUN_TEST(test__http_json_begin_object__pushes_object_to_stack_if_name_is_null);
    RUN_TEST(test__http_json_begin_object__writes_to_output);
    RUN_TEST(test__http_json_begin_object__pushes_object_to_stack);
    RUN_TEST(test__http_json_end_object__writes_to_output);
    RUN_TEST(test__http_json_end_object__pops_object_from_stack);

    RUN_TEST(test__http_json_begin_array__does_not_write_name_to_output_if_null);
    RUN_TEST(test__http_json_begin_array__pushes_array_to_stack_if_name_is_null);
    RUN_TEST(test__http_json_begin_array__writes_to_output);
    RUN_TEST(test__http_json_begin_array__pushes_array_to_stack);
    RUN_TEST(test__http_json_end_array__writes_to_output);
    RUN_TEST(test__http_json_end_array__pops_array_from_stack);


    RUN_TEST(test__http_json_write_string__writes_to_output);
    RUN_TEST(test__http_json_write_string__writes_null_to_output);
    RUN_TEST(test__http_json_write_int__writes_to_output);
    RUN_TEST(test__http_json_write_bool__writes_true_to_output);
    RUN_TEST(test__http_json_write_bool__writes_false_to_output);

    RUN_TEST(test__http_json_begin_object__writes_comma_to_output_in_object);
    RUN_TEST(test__http_json_begin_array__writes_comma_to_output_in_object);
    RUN_TEST(test__http_json_write_string__writes_comma_to_output_in_object);
    RUN_TEST(test__http_json_write_int__writes_comma_to_output_in_object);
    RUN_TEST(test__http_json_write_bool__writes_comma_to_output_in_object);

    RUN_TEST(test__http_json_begin_object__writes_comma_to_output_in_array);
    RUN_TEST(test__http_json_begin_array__writes_comma_to_output_in_array);
    RUN_TEST(test__http_json_write_string__writes_comma_to_output_in_array);
    RUN_TEST(test__http_json_write_int__writes_comma_to_output_in_array);
    RUN_TEST(test__http_json_write_bool__writes_comma_to_output_in_array);

    return UNITY_END();
}
