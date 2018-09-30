#include "fmiw_conf.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#if defined WIN32 || _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

static const char path_separator =
#ifdef WIN32
    '\\';
#else
    '/';
#endif

typedef enum conf_token_t {
    NONE,
    FMI_APIKEY,
    FILE_PATH,
    PORT
} conf_token_t;

conf_token_t conf_token_parse(char *token);

fmiw_conf_t *fmiw_conf_create() {

    struct stat sb;
    if (stat(FMIW_CONF_FILENAME, &sb) == -1) {
        // error getting file stats
        return NULL;
    }

    FILE *fp = fopen(FMIW_CONF_FILENAME, "rb");
    if (!fp)
        // error opening the conf file
        return NULL;

    char bytes[sb.st_size + 1];
    bytes[sb.st_size] = '\0'; // strtok expects null terminated string

    if (fread(bytes, sizeof(char), sb.st_size, fp) != sb.st_size || ferror(fp)) {
        // error reading the file
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    fmiw_conf_t *fmiw_conf = malloc(sizeof(fmiw_conf_t));
    memset(fmiw_conf, 0, sizeof(fmiw_conf_t));

    const char *delim = "\r\n\t ";
    char *token = strtok(bytes, delim);
        
    while (token) {
        conf_token_t conf_token = conf_token_parse(token);

        if (!conf_token) {
            free(fmiw_conf);
            return NULL;
        }

        switch (conf_token) {

            case FMI_APIKEY: {
                // set fmi_apikey
                conf_token = conf_token_parse(strtok(NULL, delim));

                switch (conf_token) {
                    
                    case FILE_PATH: {
                        char *fmi_apikey_path = strtok(NULL, delim);

                        FILE *fp_key = fopen(fmi_apikey_path, "r");

                        if (!fp_key) {
                            // error opening the fmi_apikey file
                            free(fmiw_conf);
                            return NULL;
                        }

                        if (fread(fmiw_conf->fmi_apikey, sizeof(char), sizeof fmiw_conf->fmi_apikey, fp_key) != sizeof fmiw_conf->fmi_apikey || ferror(fp_key)) {
                            // error reading the file
                            fclose(fp_key);
                            free(fmiw_conf);
                            return NULL;
                        }

                        break;
                    }
                    
                    default:
                        // invalid token
                        free(fmiw_conf);
                        return NULL;
                }

                break;
            }

            case PORT: {
                char *endptr, *p = strtok(NULL, delim);
                fmiw_conf->port = htons(strtoul(p, &endptr, 10));
            }
            
        }

        token = strtok(NULL, delim);
    }

    if (!fmiw_conf->fmi_apikey || !fmiw_conf->port) { 
        // apikey is missing
        free(fmiw_conf);
        return NULL;
    }

    return fmiw_conf;
}

void fmiw_conf_delete(fmiw_conf_t *fmiw_conf) {

    free(fmiw_conf);
}

conf_token_t conf_token_parse(char *token) {

    if (!token)
        return NONE;
    
    if (strcmp(token, "fmi-apikey") == 0)
        return FMI_APIKEY;

    if (strcmp(token, "file") == 0)
        return FILE_PATH;

    if (strcmp(token, "port") == 0)
        return PORT;

    else
        // unrecognized token
        return NONE;
}