#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "json-writer.h"

#include "log.h"

//////// Stubs /////////////////////////////////////////////////////////////////

struct json_writer json;

char *output_string = 0;

int http_write_string(struct http_request *request, const char *str)
{
    strcat(output_string, str);
    return strlen(str);
}

//////// Mocks /////////////////////////////////////////////////////////////////


//////// Test //////////////////////////////////////////////////////////////////

static void test__json_writer_http_init__initialises_the_structure(void)
{
    struct http_request request;
    json.current = 0xFF;
    json.user = NULL;

    json_writer_http_init(&json, &request);

    TEST_ASSERT_EQUAL_PTR(&request, json.user);
    TEST_ASSERT_EQUAL_INT(0, json.current);
}

static void test__json_writer_begin_object__does_not_write_name_to_output_if_null(void)
{
    json_writer_begin_object(&json, NULL);

    TEST_ASSERT_EQUAL_STRING("{", output_string);
}

static void test__json_writer_begin_object__pushes_object_to_stack_if_name_is_null(void)
{
    json_writer_begin_object(&json, NULL);

    TEST_ASSERT_EQUAL_INT(1, json.current);
    TEST_ASSERT_EQUAL_INT(JSON_WRITER_OBJECT, json.stack[0]);
}


static void test__json_writer_begin_object__writes_to_output(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_object(&json, "test");

    TEST_ASSERT_EQUAL_STRING("{\"test\":{", output_string);
}

static void test__json_writer_begin_object__pushes_object_to_stack(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_object(&json, "test");

    TEST_ASSERT_EQUAL_INT(2, json.current);
    TEST_ASSERT_EQUAL_INT(JSON_WRITER_OBJECT, json.stack[1]);
}

static void test__json_writer_end_object__writes_to_output(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_end_object(&json);

    TEST_ASSERT_EQUAL_STRING("{}", output_string);
}

static void test__json_writer_end_object__pops_object_from_stack(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_end_object(&json);

    TEST_ASSERT_EQUAL_INT(0, json.current);
}



static void test__json_writer_begin_array__does_not_write_name_to_output_if_null(void)
{
    json_writer_begin_array(&json, NULL);

    TEST_ASSERT_EQUAL_STRING("[", output_string);
}

static void test__json_writer_begin_array__pushes_array_to_stack_if_name_is_null(void)
{
    json_writer_begin_array(&json, NULL);

    TEST_ASSERT_EQUAL_INT(1, json.current);
    TEST_ASSERT_EQUAL_INT(JSON_WRITER_ARRAY, json.stack[0]);
}

static void test__json_writer_begin_array__writes_to_output(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_array(&json, "a");

    TEST_ASSERT_EQUAL_STRING("{\"a\":[", output_string);
}

static void test__json_writer_begin_array__pushes_array_to_stack(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_array(&json, "a");

    TEST_ASSERT_EQUAL_INT(2, json.current);
    TEST_ASSERT_EQUAL_INT(JSON_WRITER_ARRAY, json.stack[1]);
}

static void test__json_writer_end_array__writes_to_output(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_end_array(&json);

    TEST_ASSERT_EQUAL_STRING("[]", output_string);
}

static void test__json_writer_end_array__pops_array_from_stack(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_end_array(&json);

    TEST_ASSERT_EQUAL_INT(0, json.current);
}




static void test__json_writer_write_string__writes_to_output(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_string(&json, "a", "b");

    TEST_ASSERT_EQUAL_STRING("{\"a\":\"b\"", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_string__writes_empty_string_to_output(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_string(&json, "a", "");

    TEST_ASSERT_EQUAL_STRING("{\"a\":\"\"", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_string__writes_to_output_if_name_is_null(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_string(&json, NULL, "b");

    TEST_ASSERT_EQUAL_STRING("[\"b\"", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_string__writes_null_to_output(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_string(&json, "a", NULL);

    TEST_ASSERT_EQUAL_STRING("{\"a\":null", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_string__writes_null_to_output_if_name_is_null(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_string(&json, NULL, NULL);

    TEST_ASSERT_EQUAL_STRING("[null", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_int__writes_to_output(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_int(&json, "a", 123);

    TEST_ASSERT_EQUAL_STRING("{\"a\":123", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_int__writes_to_output_if_name_is_null(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_int(&json, NULL, 123);

    TEST_ASSERT_EQUAL_STRING("[123", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_bool__writes_true_to_output(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_bool(&json, "a", 1);

    TEST_ASSERT_EQUAL_STRING("{\"a\":true", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_bool__writes_false_to_output(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_bool(&json, "a", 0);

    TEST_ASSERT_EQUAL_STRING("{\"a\":false", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_bool__writes_true_to_output_if_name_is_null(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_bool(&json, NULL, 1);

    TEST_ASSERT_EQUAL_STRING("[true", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_bool__writes_false_to_output_if_name_is_null(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_bool(&json, NULL, 0);

    TEST_ASSERT_EQUAL_STRING("[false", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}



static void test__json_writer_begin_object__writes_comma_to_output_in_object(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_object(&json, "b");
    json_writer_end_object(&json);
    json_writer_begin_object(&json, "c");

    TEST_ASSERT_EQUAL_STRING("{\"b\":{},\"c\":{", output_string);
}

static void test__json_writer_begin_array__writes_comma_to_output_in_object(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_array(&json, "b");
    json_writer_end_array(&json);
    json_writer_begin_array(&json, "c");

    TEST_ASSERT_EQUAL_STRING("{\"b\":[],\"c\":[", output_string);
}

static void test__json_writer_write_string__writes_comma_to_output_in_object(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_string(&json, "a", "b");
    json_writer_write_string(&json, "a", "b");

    TEST_ASSERT_EQUAL_STRING("{\"a\":\"b\",\"a\":\"b\"", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_int__writes_comma_to_output_in_object(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_string(&json, "a", "b");
    json_writer_write_int(&json, "a", 12345);

    TEST_ASSERT_EQUAL_STRING("{\"a\":\"b\",\"a\":12345", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_bool__writes_comma_to_output_in_object(void)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_bool(&json, "a", 0);
    json_writer_write_bool(&json, "b", 0);
    json_writer_write_bool(&json, "c", 1);

    TEST_ASSERT_EQUAL_STRING("{\"a\":false,\"b\":false,\"c\":true", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}





static void test__json_writer_begin_object__writes_comma_to_output_in_array(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_begin_object(&json, NULL);
    json_writer_end_object(&json);
    json_writer_begin_object(&json, NULL);

    TEST_ASSERT_EQUAL_STRING("[{},{", output_string);
}

static void test__json_writer_begin_array__writes_comma_to_output_in_array(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_begin_array(&json, NULL);
    json_writer_end_array(&json);
    json_writer_begin_array(&json, NULL);

    TEST_ASSERT_EQUAL_STRING("[[],[", output_string);
}

static void test__json_writer_write_string__writes_comma_to_output_in_array(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_string(&json, NULL, "a");
    json_writer_write_string(&json, NULL, "b");

    TEST_ASSERT_EQUAL_STRING("[\"a\",\"b\"", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_int__writes_comma_to_output_in_array(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_string(&json, NULL, "a");
    json_writer_write_int(&json, NULL, 12345);

    TEST_ASSERT_EQUAL_STRING("[\"a\",12345", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}

static void test__json_writer_write_bool__writes_comma_to_output_in_array(void)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_bool(&json, NULL, 0);
    json_writer_write_bool(&json, NULL, 0);
    json_writer_write_bool(&json, NULL, 1);

    TEST_ASSERT_EQUAL_STRING("[false,false,true", output_string);
    TEST_ASSERT_EQUAL_INT(1, json.current);
}


//////// Main //////////////////////////////////////////////////////////////////

void setUp(void) {
    output_string = calloc(1024, 1);
    json.current = 0;
    json.write_string = (json_writer_io) http_write_string;
}

void tearDown(void) {
    free(output_string);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test__json_writer_http_init__initialises_the_structure);

    RUN_TEST(test__json_writer_begin_object__does_not_write_name_to_output_if_null);
    RUN_TEST(test__json_writer_begin_object__pushes_object_to_stack_if_name_is_null);
    RUN_TEST(test__json_writer_begin_object__writes_to_output);
    RUN_TEST(test__json_writer_begin_object__pushes_object_to_stack);
    RUN_TEST(test__json_writer_end_object__writes_to_output);
    RUN_TEST(test__json_writer_end_object__pops_object_from_stack);

    RUN_TEST(test__json_writer_begin_array__does_not_write_name_to_output_if_null);
    RUN_TEST(test__json_writer_begin_array__pushes_array_to_stack_if_name_is_null);
    RUN_TEST(test__json_writer_begin_array__writes_to_output);
    RUN_TEST(test__json_writer_begin_array__pushes_array_to_stack);
    RUN_TEST(test__json_writer_end_array__writes_to_output);
    RUN_TEST(test__json_writer_end_array__pops_array_from_stack);


    RUN_TEST(test__json_writer_write_string__writes_to_output);
    RUN_TEST(test__json_writer_write_string__writes_empty_string_to_output);
    RUN_TEST(test__json_writer_write_string__writes_null_to_output);
    RUN_TEST(test__json_writer_write_int__writes_to_output);
    RUN_TEST(test__json_writer_write_bool__writes_true_to_output);
    RUN_TEST(test__json_writer_write_bool__writes_false_to_output);

    RUN_TEST(test__json_writer_write_string__writes_to_output_if_name_is_null);
    RUN_TEST(test__json_writer_write_string__writes_null_to_output_if_name_is_null);
    RUN_TEST(test__json_writer_write_int__writes_to_output_if_name_is_null);
    RUN_TEST(test__json_writer_write_bool__writes_true_to_output_if_name_is_null);
    RUN_TEST(test__json_writer_write_bool__writes_false_to_output_if_name_is_null);

    RUN_TEST(test__json_writer_begin_object__writes_comma_to_output_in_object);
    RUN_TEST(test__json_writer_begin_array__writes_comma_to_output_in_object);
    RUN_TEST(test__json_writer_write_string__writes_comma_to_output_in_object);
    RUN_TEST(test__json_writer_write_int__writes_comma_to_output_in_object);
    RUN_TEST(test__json_writer_write_bool__writes_comma_to_output_in_object);

    RUN_TEST(test__json_writer_begin_object__writes_comma_to_output_in_array);
    RUN_TEST(test__json_writer_begin_array__writes_comma_to_output_in_array);
    RUN_TEST(test__json_writer_write_string__writes_comma_to_output_in_array);
    RUN_TEST(test__json_writer_write_int__writes_comma_to_output_in_array);
    RUN_TEST(test__json_writer_write_bool__writes_comma_to_output_in_array);


    return UNITY_END();
}
