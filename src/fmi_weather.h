#ifndef FMI_WEATHER_H
#define FMI_WEATHER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <time.h>

typedef struct fmi_weather_t {
    struct tm utc;
    double celsius;
    uint8_t weather_symbol;
} fmi_weather_t;

#ifdef __cplusplus
}
#endif

#endif