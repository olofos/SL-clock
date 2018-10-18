#ifndef HTTP_SERVER_TASK_H_
#define HTTP_SERVER_TASK_H_

void http_server_task(void *pvParameters);

struct http_request;
void http_server_write_simple_response(struct http_request* request, int status, const char* content_type, const char* reply);

#endif
