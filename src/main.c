#include "fmiw_conf.h"
#include "fmi_client.h"
#include "uo_queue.h"
#include "uo_sock.h"
#include "uo_err.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <errno.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>

static int server_sockfd;
static bool is_closing;
static uo_queue *request_sockfd_queue;

void handle_signal(
    int sig) 
{
	is_closing = true;
	close(server_sockfd);

	signal(sig, SIG_DFL);
}

void send_weather_data(
    void *weather, 
    void *state) 
{
    const char 
        closing_msg[] = "CLOSING\r\n",
        error_msg[] = "ERROR\r\n";

    int *client_sockfd = state;

    if (weather)
    {
        char result[0x1000] = { 0 };
        char *p = result;
        fmi_weather_t *w = weather;

        p += snprintf(p, sizeof result - (p - result), "%lf,%lf\r\n",
            w->latitude,
            w->longitude);

        for (int i = 0; i < 24; ++i)
        {
            p += snprintf(p, sizeof result - (p - result), "%d-%02d-%02dT%02d:00:00Z,%.1lf,%d\r\n",
            w[i].utc.tm_year + 1900,
            w[i].utc.tm_mon + 1,
            w[i].utc.tm_mday,
            w[i].utc.tm_hour,
            w[i].celsius,
            w[i].weather_symbol);
        }   
        send(*client_sockfd, result, strlen(result), 0);
        free(weather);
    }
    else
        send(*client_sockfd, error_msg, sizeof error_msg - 1, 0); 

    send(*client_sockfd, closing_msg, sizeof closing_msg - 1, 0);
    shutdown(*client_sockfd, SHUT_WR);

    char *nullbuf;

    while(recv(*client_sockfd, nullbuf, 1, 0) > 0);

    shutdown(*client_sockfd, SHUT_RD);
    close(*client_sockfd);
    free(state);
}

void *serve(
    void *arg) 
{
    char buf[0x1000];
    uo_queue *conn_queue = arg;
    fmi_client_t *fmi_client = fmi_client_create();

    const char ready_msg[] = "READY\r\n";

    while (!is_closing) 
    {
        int *client_sockfd = uo_queue_dequeue(conn_queue, true);
        
        if (send(*client_sockfd, ready_msg, sizeof ready_msg - 1, 0) == -1) 
            uo_err_goto(err_close, "Error sending the READY message.")
        
        ssize_t recv_len = 0;
        char *end = NULL;
        char *p = buf;
        
        while (!end) 
        {
            if ((recv_len = recv(*client_sockfd, p, sizeof buf - (p - buf), 0)) == -1)
                uo_err_goto(err_close, "Error sending the READY message.")

            p += recv_len;
            *p = '\0';
            end = strstr(buf, "\r\n");
        }

        char *token = strtok(buf, " ");
        
        switch (token[0])
        {
            case 'W':
                token = strtok(NULL, "\0");
        }

        const size_t place_len = end - token;
        char *place = malloc(place_len);
        memcpy(place, token, place_len);

        fmi_client_get_current_weather(fmi_client, place, place_len, send_weather_data, client_sockfd);

        continue;

err_close:
        close(*client_sockfd);
        free(client_sockfd);
    }
}

int main(
    int argc, 
    char **argv) 
{
    fmiw_conf_t *fmiw_conf = fmiw_conf_create();
    if (!fmiw_conf)
        uo_err_exit("Error reading the configuration.");

    if (!uo_sock_init())
        uo_err_exit("Error initializing uo_sock.");

    fmi_client_configure(fmiw_conf->fmi_apikey);

    uo_queue *conn_queue = uo_queue_create(0x100);

    pthread_t thrd;
    pthread_create(&thrd, NULL, serve, conn_queue);

    /*	Start listening socket that will accept and queue new connections

        The socket is blocking dual-stack socket that will listen port that was set in configuration file. */
	server_sockfd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

	if (server_sockfd == -1)
		uo_err_exit("Unable to create socket");
	
	{
		int opt_IPV6_V6ONLY = false;
		uo_setsockopt(server_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &opt_IPV6_V6ONLY, sizeof opt_IPV6_V6ONLY);

		int opt_TCP_NODELAY = true;
		uo_setsockopt(server_sockfd, IPPROTO_TCP, TCP_NODELAY, &opt_TCP_NODELAY, sizeof opt_TCP_NODELAY);

        struct timeval opt_SO_RCVTIMEO = { .tv_sec = 20 };
		uo_setsockopt(server_sockfd, IPPROTO_IPV6, SO_RCVTIMEO, &opt_SO_RCVTIMEO, sizeof opt_SO_RCVTIMEO);

        struct timeval opt_SO_SNDTIMEO = { .tv_sec = 20 };
        uo_setsockopt(server_sockfd, IPPROTO_IPV6, SO_SNDTIMEO, &opt_SO_SNDTIMEO, sizeof opt_SO_SNDTIMEO);

		struct sockaddr_in6 addr = {
			.sin6_family = AF_INET6,
			.sin6_port = fmiw_conf->port,
			.sin6_addr = in6addr_any
		};

		if (bind(server_sockfd, (struct sockaddr *)&addr, sizeof addr) == -1)
			uo_err_exit("Unable to bind to socket!");

		/*	Setup signal handling
			
			TODO: Consider using sigaction instead of signal */
		signal(SIGINT, handle_signal);

		if (listen(server_sockfd, SOMAXCONN) == -1)
			uo_err_exit("Unable to listen on socket!\r\n");

		{
			char addrp[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &addr.sin6_addr, addrp, INET6_ADDRSTRLEN);
			uint16_t portp = ntohs(addr.sin6_port);

			printf("Listening on [%s]:%u.\r\n", addrp, portp);
		}

		while (!is_closing) 
        {
			/*	Accept connection

				Client address can be later acquired with 
				int getsockname(int sockfd, struct sockaddr *addrsocklen_t *" addrlen ); */
			int *client_sockfd = malloc(sizeof *client_sockfd);
            if ((*client_sockfd = accept(server_sockfd, NULL, NULL)) != -1)
                uo_queue_enqueue(conn_queue, client_sockfd, true);
            else
            {
                uo_err("Error on accepting connection.");
                free(client_sockfd);
            }
		}

        pthread_join(thrd, NULL);
    }

    uo_queue_destroy(conn_queue);

	return 0;
}
