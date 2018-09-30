#ifndef FMIW_CONF_H
#define FMIW_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define FMIW_CONF_FILENAME "fmi-weather.conf"

typedef struct fmiw_conf_t {
	char fmi_apikey[36];
	uint16_t port;
} fmiw_conf_t;

fmiw_conf_t *fmiw_conf_create();
void fmiw_conf_delete(fmiw_conf_t *);

#ifdef __cplusplus
}
#endif

#endif