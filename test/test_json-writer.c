#include <stdbool.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

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


static void test__json_writer_http_init__initialises_the_structure(void **state)
{
    struct http_request request;
    json.current = 0xFF;
    json.user = NULL;

    json_writer_http_init(&json, &request);

    assert_ptr_equal(&request, json.user);
    assert_int_equal(0, json.current);
}

static void test__json_writer_begin_object__does_not_write_name_to_output_if_null(void **state)
{
    json_writer_begin_object(&json, NULL);

    assert_string_equal("{", output_string);
}

static void test__json_writer_begin_object__pushes_object_to_stack_if_name_is_null(void **state)
{
    json_writer_begin_object(&json, NULL);

    assert_int_equal(1, json.current);
    assert_int_equal(JSON_WRITER_OBJECT, json.stack[0]);
}


static void test__json_writer_begin_object__writes_to_output(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_object(&json, "test");

    assert_string_equal("{\"test\":{", output_string);
}

static void test__json_writer_begin_object__pushes_object_to_stack(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_object(&json, "test");

    assert_int_equal(2, json.current);
    assert_int_equal(JSON_WRITER_OBJECT, json.stack[1]);
}

static void test__json_writer_end_object__writes_to_output(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_end_object(&json);

    assert_string_equal("{}", output_string);
}

static void test__json_writer_end_object__pops_object_from_stack(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_end_object(&json);

    assert_int_equal(0, json.current);
}



static void test__json_writer_begin_array__does_not_write_name_to_output_if_null(void **state)
{
    json_writer_begin_array(&json, NULL);

    assert_string_equal("[", output_string);
}

static void test__json_writer_begin_array__pushes_array_to_stack_if_name_is_null(void **state)
{
    json_writer_begin_array(&json, NULL);

    assert_int_equal(1, json.current);
    assert_int_equal(JSON_WRITER_ARRAY, json.stack[0]);
}

static void test__json_writer_begin_array__writes_to_output(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_array(&json, "a");

    assert_string_equal("{\"a\":[", output_string);
}

static void test__json_writer_begin_array__pushes_array_to_stack(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_array(&json, "a");

    assert_int_equal(2, json.current);
    assert_int_equal(JSON_WRITER_ARRAY, json.stack[1]);
}

static void test__json_writer_end_array__writes_to_output(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_end_array(&json);

    assert_string_equal("[]", output_string);
}

static void test__json_writer_end_array__pops_array_from_stack(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_end_array(&json);

    assert_int_equal(0, json.current);
}




static void test__json_writer_write_string__writes_to_output(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_string(&json, "a", "b");

    assert_string_equal("{\"a\":\"b\"", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_string__writes_empty_string_to_output(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_string(&json, "a", "");

    assert_string_equal("{\"a\":\"\"", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_string__writes_to_output_if_name_is_null(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_string(&json, NULL, "b");

    assert_string_equal("[\"b\"", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_string__writes_null_to_output(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_string(&json, "a", NULL);

    assert_string_equal("{\"a\":null", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_string__writes_null_to_output_if_name_is_null(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_string(&json, NULL, NULL);

    assert_string_equal("[null", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_int__writes_to_output(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_int(&json, "a", 123);

    assert_string_equal("{\"a\":123", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_int__writes_to_output_if_name_is_null(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_int(&json, NULL, 123);

    assert_string_equal("[123", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_bool__writes_true_to_output(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_bool(&json, "a", 1);

    assert_string_equal("{\"a\":true", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_bool__writes_false_to_output(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_bool(&json, "a", 0);

    assert_string_equal("{\"a\":false", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_bool__writes_true_to_output_if_name_is_null(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_bool(&json, NULL, 1);

    assert_string_equal("[true", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_bool__writes_false_to_output_if_name_is_null(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_bool(&json, NULL, 0);

    assert_string_equal("[false", output_string);
    assert_int_equal(1, json.current);
}



static void test__json_writer_begin_object__writes_comma_to_output_in_object(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_object(&json, "b");
    json_writer_end_object(&json);
    json_writer_begin_object(&json, "c");

    assert_string_equal("{\"b\":{},\"c\":{", output_string);
}

static void test__json_writer_begin_array__writes_comma_to_output_in_object(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_begin_array(&json, "b");
    json_writer_end_array(&json);
    json_writer_begin_array(&json, "c");

    assert_string_equal("{\"b\":[],\"c\":[", output_string);
}

static void test__json_writer_write_string__writes_comma_to_output_in_object(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_string(&json, "a", "b");
    json_writer_write_string(&json, "a", "b");

    assert_string_equal("{\"a\":\"b\",\"a\":\"b\"", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_int__writes_comma_to_output_in_object(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_string(&json, "a", "b");
    json_writer_write_int(&json, "a", 12345);

    assert_string_equal("{\"a\":\"b\",\"a\":12345", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_bool__writes_comma_to_output_in_object(void **state)
{
    json_writer_begin_object(&json, NULL);
    json_writer_write_bool(&json, "a", 0);
    json_writer_write_bool(&json, "b", 0);
    json_writer_write_bool(&json, "c", 1);

    assert_string_equal("{\"a\":false,\"b\":false,\"c\":true", output_string);
    assert_int_equal(1, json.current);
}





static void test__json_writer_begin_object__writes_comma_to_output_in_array(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_begin_object(&json, NULL);
    json_writer_end_object(&json);
    json_writer_begin_object(&json, NULL);

    assert_string_equal("[{},{", output_string);
}

static void test__json_writer_begin_array__writes_comma_to_output_in_array(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_begin_array(&json, NULL);
    json_writer_end_array(&json);
    json_writer_begin_array(&json, NULL);

    assert_string_equal("[[],[", output_string);
}

static void test__json_writer_write_string__writes_comma_to_output_in_array(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_string(&json, NULL, "a");
    json_writer_write_string(&json, NULL, "b");

    assert_string_equal("[\"a\",\"b\"", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_int__writes_comma_to_output_in_array(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_string(&json, NULL, "a");
    json_writer_write_int(&json, NULL, 12345);

    assert_string_equal("[\"a\",12345", output_string);
    assert_int_equal(1, json.current);
}

static void test__json_writer_write_bool__writes_comma_to_output_in_array(void **state)
{
    json_writer_begin_array(&json, NULL);
    json_writer_write_bool(&json, NULL, 0);
    json_writer_write_bool(&json, NULL, 0);
    json_writer_write_bool(&json, NULL, 1);

    assert_string_equal("[false,false,true", output_string);
    assert_int_equal(1, json.current);
}


//////// Main //////////////////////////////////////////////////////////////////

int setup(void **state) {
    output_string = calloc(1024, 1);
    json.current = 0;
    json.write_string = (json_writer_io) http_write_string;
    return 0;
}

int teardown(void **state) {
    free(output_string);
    return 0;
}

const struct CMUnitTest tests_for_json_writer[] = {
    cmocka_unit_test_setup_teardown(test__json_writer_http_init__initialises_the_structure, setup, teardown),

    cmocka_unit_test_setup_teardown(test__json_writer_begin_object__does_not_write_name_to_output_if_null, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_begin_object__pushes_object_to_stack_if_name_is_null, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_begin_object__writes_to_output, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_begin_object__pushes_object_to_stack, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_end_object__writes_to_output, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_end_object__pops_object_from_stack, setup, teardown),

    cmocka_unit_test_setup_teardown(test__json_writer_begin_array__does_not_write_name_to_output_if_null, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_begin_array__pushes_array_to_stack_if_name_is_null, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_begin_array__writes_to_output, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_begin_array__pushes_array_to_stack, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_end_array__writes_to_output, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_end_array__pops_array_from_stack, setup, teardown),


    cmocka_unit_test_setup_teardown(test__json_writer_write_string__writes_to_output, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_string__writes_empty_string_to_output, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_string__writes_null_to_output, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_int__writes_to_output, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_bool__writes_true_to_output, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_bool__writes_false_to_output, setup, teardown),

    cmocka_unit_test_setup_teardown(test__json_writer_write_string__writes_to_output_if_name_is_null, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_string__writes_null_to_output_if_name_is_null, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_int__writes_to_output_if_name_is_null, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_bool__writes_true_to_output_if_name_is_null, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_bool__writes_false_to_output_if_name_is_null, setup, teardown),

    cmocka_unit_test_setup_teardown(test__json_writer_begin_object__writes_comma_to_output_in_object, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_begin_array__writes_comma_to_output_in_object, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_string__writes_comma_to_output_in_object, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_int__writes_comma_to_output_in_object, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_bool__writes_comma_to_output_in_object, setup, teardown),

    cmocka_unit_test_setup_teardown(test__json_writer_begin_object__writes_comma_to_output_in_array, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_begin_array__writes_comma_to_output_in_array, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_string__writes_comma_to_output_in_array, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_int__writes_comma_to_output_in_array, setup, teardown),
    cmocka_unit_test_setup_teardown(test__json_writer_write_bool__writes_comma_to_output_in_array, setup, teardown),
};


int main(void)
{
    int fails = 0;
    fails += cmocka_run_group_tests(tests_for_json_writer, NULL, NULL);

    return fails;
}
