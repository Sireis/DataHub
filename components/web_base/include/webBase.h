#pragma once
#include <esp_https_server.h>

void webBase_init();
void webBase_start();

httpd_handle_t webBase_getServer();
void webBase_registerOnWebserverCreated(void (*handler)(httpd_handle_t* server));
void webBase_overrideIndex(esp_err_t (*handler)(httpd_req_t* r));
void webBase_overrideStyles(esp_err_t (*handler)(httpd_req_t* r));
void webBase_overrideFavicon(esp_err_t (*handler)(httpd_req_t* r));