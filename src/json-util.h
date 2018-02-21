#ifndef JSON_UTIL_H_
#define JSON_UTIL_H_

const char *json_type_to_string(enum json_type type);
bool json_expect(json_stream *json, enum json_type type);
void json_skip_until_end_of_object(json_stream *json);
void json_skip_until_end_of_array(json_stream *json);
void json_skip(json_stream *json);
int json_find_names(json_stream *json, const char * names[], int num);
bool json_find_name(json_stream *json, const char *name);

#endif
