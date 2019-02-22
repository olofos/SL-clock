#ifndef HTTP_SERVER_URL_HANDLERS_H_
#define HTTP_SERVER_URL_HANDLERS_H_

enum http_cgi_state cgi_wifi_scan(struct http_request* request);
enum http_cgi_state cgi_wifi_list(struct http_request* request);
enum http_cgi_state cgi_journey_config(struct http_request* request);
enum http_cgi_state cgi_status(struct http_request* request);
enum http_cgi_state cgi_log(struct http_request* request);
enum http_cgi_state cgi_syslog_config(struct http_request* request);
enum http_cgi_state cgi_led_matrix_config(struct http_request* request);
enum http_cgi_state cgi_led_matrix_status(struct http_request* request);


#endif
