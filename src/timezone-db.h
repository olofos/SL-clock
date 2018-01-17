#ifndef TIMEZONE_DB_H_
#define TIMEZONE_DB_H_

#define TIMEZONE_NAME_LEN 32

void timezone_db_task(void *pvParameters);
void timezone_set_timezone(const char *name);
const char *timezone_get_timezone(void);

#endif
