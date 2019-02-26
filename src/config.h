#ifndef CONFIG_H_
#define CONFIG_H_

void config_load(const char *filename);
void config_save(const char *filename);

struct json_stream;
struct json_writer;

typedef int (*config_load_func)(struct json_stream *);
typedef void (*config_save_func)(struct json_writer *, const char *name);

int config_load_wifi(struct json_stream *json);
int config_load_journies(struct json_stream *json);
int config_load_timezone(struct json_stream *json);
int config_load_led_matrix(struct json_stream *json);

void config_save_wifi(struct json_writer *json, const char *name);
void config_save_journies(struct json_writer *json, const char *name);
void config_save_timezone(struct json_writer *json, const char *name);
void config_save_led_matrix(struct json_writer *json, const char *name);

#endif
