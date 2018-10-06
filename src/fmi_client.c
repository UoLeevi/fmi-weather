#include "fmi_client.h"
#include "uo_cb.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>

#define STRLEN(s) (sizeof(s)/sizeof(s[0]) - 1)

#define DUMMY_APIKEY "00000000-0000-0000-0000-000000000000"

#define FMI_HOSTNAME "data.fmi.fi"
#define FMI_ENDPOINT_WEATHER \
    "?request=getFeature&" \
    "storedquery_id=fmi::forecast::harmonie::surface::point::multipointcoverage&" \
    "timestep=60&" \
    "parameters=temperature,WeatherSymbol3&" \
    "place="

#define GML_POS_TAG "<gml:pos>"
#define GML_BEGINPOSITION_TAG "<gml:beginPosition>"
#define GML_DOUBLEORNILLREASONTUPLELIST_TAG "<gml:doubleOrNilReasonTupleList>"

static char FMI_API_PATH[] = "/fmi-apikey/" DUMMY_APIKEY "/wfs";

uintmax_t read_content_length(
    char *);

void *handle_response_get_current_weather(
    uo_http_res *http_response,
    void *state) 
{
    if (!http_response) {
        return NULL;
    }

    char *gml_pos = strstr(http_response->body, GML_POS_TAG);

    if (!gml_pos) {
        uo_http_res_destroy(http_response);
        return NULL;
    }

    char *gml_beginTime = strstr(http_response->body, GML_BEGINPOSITION_TAG);

    if (!gml_beginTime) {
        uo_http_res_destroy(http_response);
        return NULL;
    }

    char *gml_doubleOrNilReasonTupleList = strstr(http_response->body, GML_DOUBLEORNILLREASONTUPLELIST_TAG);

    if (!gml_doubleOrNilReasonTupleList) {
        uo_http_res_destroy(http_response);
        return NULL;
    }

    fmi_weather_t *weather = calloc(24, sizeof(fmi_weather_t));

    if (sscanf(gml_pos + STRLEN(GML_POS_TAG), "%lf %lf", 
        &weather->latitude, 
        &weather->longitude) != 2) {
        free(weather);
        uo_http_res_destroy(http_response);
        return NULL;
    }

    if (sscanf(gml_beginTime + STRLEN(GML_BEGINPOSITION_TAG), "%d-%d-%dT%d:%d:%dZ", 
        &weather->utc.tm_year,
        &weather->utc.tm_mon, 
        &weather->utc.tm_mday, 
        &weather->utc.tm_hour, 
        &weather->utc.tm_min,
        &weather->utc.tm_sec) != 6) {
        free(weather);
        uo_http_res_destroy(http_response);
        return NULL;
    }

    weather->utc.tm_year -= 1900;
    weather->utc.tm_mon -= 1;

    char *token = strtok(gml_doubleOrNilReasonTupleList + STRLEN(GML_DOUBLEORNILLREASONTUPLELIST_TAG), "\r\n\t ");

    char* endptr;

    for (int i = 0; i < 24; ++i)
    {
        weather[i].celsius = strtod(token, &endptr);
        token = strtok(NULL, "\r\n\t ");
        weather[i].weather_symbol = strtoul(token, &endptr, 10);
        token = strtok(NULL, "\r\n\t ");
    }

    uo_http_res_destroy(http_response);

    return weather;
}

void fmi_client_configure(
    char fmi_apikey[36]) 
{
    memcpy(strstr(FMI_API_PATH, DUMMY_APIKEY), fmi_apikey, strlen(DUMMY_APIKEY));
    uo_httpc_init(1);
}

fmi_client_t *fmi_client_create() 
{
    fmi_client_t *fmi_client = malloc(sizeof(fmi_client_t));
    fmi_client->http_client = uo_httpc_create(FMI_HOSTNAME, STRLEN(FMI_HOSTNAME));
    uo_httpc_set_header(fmi_client->http_client, HTTP_HEADER_ACCEPT, "application/xml", 15);
	
    return fmi_client;
}

void fmi_client_delete(
    fmi_client_t *fmi_client) 
{
    uo_httpc_destroy(fmi_client->http_client);
    free(fmi_client);
}

void fmi_client_get_current_weather(
    fmi_client_t *fmi_client, 
    char *place, 
    size_t place_len, 
    weather_callback_t callback, 
    void *state) 
{
    char starttime[32];

    time_t t = time(NULL);
    struct tm *utc = gmtime(&t);
    sprintf(starttime, "&starttime=%d-%02d-%02dT%02d:00:00Z", 
        utc->tm_year + 1900, 
        utc->tm_mon + 1,
        utc->tm_mday,
        utc->tm_hour);

    char path[STRLEN(FMI_API_PATH) + STRLEN(FMI_ENDPOINT_WEATHER) + place_len + STRLEN(starttime)];

    char *p = path;
    p = memcpy(p, FMI_API_PATH, STRLEN(FMI_API_PATH)) + STRLEN(FMI_API_PATH);
    p = memcpy(p, FMI_ENDPOINT_WEATHER, STRLEN(FMI_ENDPOINT_WEATHER)) + STRLEN(FMI_ENDPOINT_WEATHER);
    p = memcpy(p, place, place_len) + place_len;
    p = memcpy(p, starttime, STRLEN(starttime)) + STRLEN(starttime);

    free(place);
    
    assert(p - path == sizeof(path));

    uo_cb *cb = uo_cb_create(uo_cb_opt_invoke_once);
    uo_cb_append(cb, (void *(*)(void *, void *))handle_response_get_current_weather);
    uo_cb_append(cb, (void *(*)(void *, void *))callback);

    uo_httpc_get(fmi_client->http_client, path, sizeof(path), (void *(*)(uo_http_res *, void *))uo_cb_as_func(cb, state), state);
}
