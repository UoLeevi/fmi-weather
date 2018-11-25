#include "uo_conf.h"
#include "uo_ipcs.h"
#include "uo_httpc.h"
#include "uo_cb.h"
#include "uo_err.h"
#include "uo_mem.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define STRLEN(s) (sizeof(s)/sizeof(s[0]) - 1)

#define FMI_APIKEY "00000000-0000-0000-0000-000000000000"
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

static char FMI_API_PATH[] = "/fmi-apikey/" FMI_APIKEY "/wfs";

static uo_httpc *fmi_client;

static uo_ipcmsg *fmi_weather_parse_current_weather_res(
    uo_http_res *http_response,
    uo_cb *cb) 
{
    if (!http_response)
        return NULL;

    if (http_response->status_code != 200 && http_response->status_code != 301)
        uo_err_goto(err_http_res_destroy, "Error receiving the weather data.");

    char *gml_pos = strstr(http_response->body, GML_POS_TAG);
    if (!gml_pos) 
        uo_err_goto(err_http_res_destroy, "Error parsing the weather data.");
    gml_pos += STRLEN(GML_POS_TAG);

    char *gml_beginTime = strstr(http_response->body, GML_BEGINPOSITION_TAG);
    if (!gml_beginTime)
        uo_err_goto(err_http_res_destroy, "Error parsing the weather data.");
    gml_beginTime += STRLEN(GML_BEGINPOSITION_TAG);

    char *gml_doubleOrNilReasonTupleList = strstr(http_response->body, GML_DOUBLEORNILLREASONTUPLELIST_TAG);
    if (!gml_doubleOrNilReasonTupleList) 
        uo_err_goto(err_http_res_destroy, "Error parsing the weather data.");
    gml_doubleOrNilReasonTupleList += STRLEN(GML_DOUBLEORNILLREASONTUPLELIST_TAG);

    uo_ipcmsg *msg = malloc(sizeof *msg);
    msg->data = malloc(0x1000);

    char *p = msg->data;

    double latitude;
    double longitude;

    if (sscanf(gml_pos, "%lf %lf", &latitude, &longitude) != 2) 
        uo_err_goto(err_free_res, "Error parsing the weather data.");

    p += snprintf(p, 0x1000 - (p - msg->data), "%lf,%lf,", latitude, longitude);

    char *gml_beginTime_end = strchr(gml_beginTime, 'Z');
    if (!gml_beginTime_end)
        uo_err_goto(err_free_res, "Error parsing the weather data.");
    gml_beginTime_end++;

    uo_mem_write(p, gml_beginTime, gml_beginTime_end - gml_beginTime);
    uo_mem_write(p, "\r\n", STRLEN("\r\n"));

    char *token = strtok(gml_doubleOrNilReasonTupleList, "\r\n");
    double celsius;
    unsigned long weather_symbol;

    for (int i = 0; i < 24; ++i)
    {
        sscanf(token, " %lf %d", &celsius, &weather_symbol);
        p += snprintf(p, 0x1000 - (p - msg->data), "%.1lf,%d" "\r\n", celsius, weather_symbol);
        token = strtok(NULL, "\r\n\t\0");
    }

    uo_http_res_destroy(http_response);

    msg->data_len = p - msg->data;
    *p = '\0'; 
    msg->data = realloc(msg->data, msg->data_len + 1);
    msg->should_free = true;

    return msg;

err_free_res:
    free(msg->data);

err_http_res_destroy:
    uo_http_res_destroy(http_response);

    return NULL;
}

static void *fmi_weather_handle_cmd(
    uo_ipcmsg *cmd,
    uo_cb *uo_ipcmsg_cb)
{
    char *token = strtok(cmd->data, " ");
        
    switch (token[0])
    {
        case 'W':
        {
            char *place = strtok(NULL, "\0");
            const size_t place_len = cmd->data + cmd->data_len - place;

            char starttime[32];

            time_t t = time(NULL);
            struct tm *utc = gmtime(&t);
            sprintf(starttime, "&starttime=%d-%02d-%02dT%02d:00:00Z", 
                utc->tm_year + 1900, 
                utc->tm_mon + 1,
                utc->tm_mday,
                utc->tm_hour);

            size_t path_len = STRLEN(FMI_API_PATH) + STRLEN(FMI_ENDPOINT_WEATHER) + place_len + STRLEN(starttime);
            char *path = malloc(path_len);

            char *p = path;
            uo_mem_write(p, FMI_API_PATH, STRLEN(FMI_API_PATH));
            uo_mem_write(p, FMI_ENDPOINT_WEATHER, STRLEN(FMI_ENDPOINT_WEATHER));
            uo_mem_write(p, place, place_len);
            uo_mem_write(p, starttime, STRLEN(starttime));

            uo_cb_prepend(uo_ipcmsg_cb, (void *(*)(void *, uo_cb *))fmi_weather_parse_current_weather_res);

            uo_httpc_get(
                fmi_client, 
                path, 
                path_len, 
                uo_ipcmsg_cb);
        }
    }
}

int main(
    int argc, 
    char **argv)
{
    uo_ipc_init();
    uo_httpc_init(1);

    uo_conf *conf = uo_conf_create("fmi_weather.conf");

    char *apikey = uo_conf_get(conf, "fmi-apikey");
    memcpy(strstr(FMI_API_PATH, FMI_APIKEY), apikey, strlen(apikey));

    fmi_client = uo_httpc_create(FMI_HOSTNAME, STRLEN(FMI_HOSTNAME), UO_HTTPC_OPT_TLS);
    uo_httpc_set_header(fmi_client, HTTP_HEADER_ACCEPT, "application/xml", 15);

    char *port = uo_conf_get(conf, "port");
    uo_ipcs *ipcs = uo_ipcs_create(port, fmi_weather_handle_cmd);

    printf("Press 'q' to quit...");
    while(getchar() != 'q');

    ipcs->is_closing = true;

    return 0;
}