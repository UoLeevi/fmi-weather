#ifndef FMI_CLIENT_H
#define FMI_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fmi_weather.h"
#include "uo_httpc.h"

typedef void (*weather_callback_t)(void *result, void *state);

typedef struct fmi_client_t {
	uo_httpc *http_client;
} fmi_client_t;

void fmi_client_configure(
	char[36]);

fmi_client_t *fmi_client_create(void);

void fmi_client_delete(
	fmi_client_t *);

void fmi_client_get_current_weather(
	fmi_client_t *fmi_client, 
	char *place, 
	size_t place_len, 
	weather_callback_t callback, 
	void *state);

#ifdef __cplusplus
}
#endif

#endif