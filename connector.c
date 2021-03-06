/* Rhizo-connector: A connector to different HF modems
 * Copyright (C) 2018 Rhizomatica
 * Author: Rafael Diniz <rafael@riseup.net>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 */

/**
 * @file connector.c
 * @author Rafael Diniz
 * @date 12 Apr 2018
 * @brief Rhizo HF Connector main file
 *
 * Rhizo HF Connector main C file.
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <dirent.h>

#include "connector.h"
#include "spool.h"
#include "vara.h"
#include "dstar.h"
#include "ardop.h"
#include "serial.h"

// temporary global variable to enable sockets closure
rhizo_conn *tmp_conn = NULL;

void finish(int s){
    fprintf(stderr, "\nExiting...\n");

    /* Do house cleaning work here */
    if (tmp_conn){
        if (tmp_conn->serial_keying)
        {
            key_off(tmp_conn->serial_fd, tmp_conn->radio_type);
            close(tmp_conn->serial_fd);
        }

        if (tmp_conn->data_socket){
            shutdown(tmp_conn->data_socket, SHUT_RDWR);
            close (tmp_conn->data_socket);
        }
        if (tmp_conn->control_socket){
            shutdown(tmp_conn->control_socket, SHUT_RDWR);
            close (tmp_conn->control_socket);
        }
    }

    exit(EXIT_SUCCESS);
}

void *modem_thread(void *conn)
{
    rhizo_conn *connector = (rhizo_conn *) conn;

    if (!strcmp("vara", connector->modem_type)){
        initialize_modem_vara(connector);
    }

    if (!strcmp("ardop", connector->modem_type)){
        initialize_modem_ardop(connector);
    }

    if (!strcmp("dstar", connector->modem_type)){
        initialize_modem_dstar(connector);
    }

    return NULL;
}

bool initialize_connector(rhizo_conn *connector){

    initialize_buffer(&connector->in_buffer, 26); // 64MB
    initialize_buffer(&connector->out_buffer, 26); // 64MB
    pthread_mutex_init(&connector->msg_path_queue_mutex, NULL);

    connector->connected = false;
    connector->waiting_for_connection = false;
    connector->serial_keying = false;
    connector->radio_type = RADIO_TYPE_ICOM;
    connector->tcp_ret_ok = true;
    connector->serial_fd = -1;
    connector->msg_path_queue_size = 0;
    connector->safe_state = 0;

    connector->timeout = TIMEOUT_DEFAULT;
    connector->ofdm_mode = true;
    connector->buffer_size = 0;
    return true;
}

int main (int argc, char *argv[])
{
    rhizo_conn connector;
    tmp_conn = &connector;

    initialize_connector(&connector);

    // Catch Ctrl+C
    signal (SIGINT,finish);

    fprintf(stderr, "Rhizomatica's HF Connector version 0.3 by Rafael Diniz -  rafael (AT) rhizomatica (DOT) org\n");
    fprintf(stderr, "License: GPLv3+\n\n");

    if (argc < 7)
    {
    manual:
        fprintf(stderr, "Usage modes: \n%s -r radio_modem_type -i input_spool_directory -o output_spool_directory -c callsign -d remote_callsign -a tnc_ip_address -p tcp_base_port\n", argv[0]);
        fprintf(stderr, "%s -h\n", argv[0]);
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, " -r [ardop,vara]           Choose modem/radio type.\n");
        fprintf(stderr, " -i input_spool_directory    Input spool directory (Messages to send).\n");
        fprintf(stderr, " -o output_spool_directory    Output spool directory (Received messages).\n");
        fprintf(stderr, " -c callsign                        Station Callsign (Eg: PU2HFF).\n");
        fprintf(stderr, " -d remote_callsign           Remote Station Callsign.\n");
        fprintf(stderr, " -a tnc_ip_address            IP address of the TNC,\n");
        fprintf(stderr, " -p tcp_base_port              TCP base port of the TNC. For VARA and ARDOP ports tcp_base_port and tcp_base_port+1 are used,\n");
        fprintf(stderr, " -t timeout                 Time to wait before disconnect when idling.\n");
        fprintf(stderr, " -f features                Enable/Disable features. Supported features: ofdm, noofdm (ARDOP ONLY).\n");
        fprintf(stderr, " -s serial_device           Set the serial device file path for keying the radio (VARA ONLY).\n");
        fprintf(stderr, " -b [icom,ubitx]            Sets radio type (supported: icom or ubitx)\n");
        fprintf(stderr, " -h                          Prints this help.\n");
        exit(EXIT_FAILURE);
    }

    char *last;
    int opt;
    while ((opt = getopt(argc, argv, "hr:i:o:c:d:p:a:t:f:s:b:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            goto manual;
            break;
        case 'c':
            strcpy(connector.call_sign, optarg);
            break;
        case 'd':
            strcpy(connector.remote_call_sign, optarg);
            break;
        case 'b':
            // icom is the default...
            if (!strcmp(optarg,"ubitx"))
                connector.radio_type = RADIO_TYPE_UBITX;
            break;
        case 't':
            connector.timeout = atoi(optarg);
            break;
        case 'p':
            connector.tcp_base_port = atoi(optarg);
            break;
        case 'a':
            strcpy(connector.ip_address, optarg);
            break;
        case 'r':
            strcpy(connector.modem_type, optarg);
            break;
        case 'i':
            strcpy(connector.input_directory, optarg);
            last = &connector.input_directory[strlen(connector.input_directory)-1];
            if (last[0] != '/'){
                last[1] = '/';
                last[2] = 0;
            }
            break;
        case 'o':
            strcpy(connector.output_directory, optarg);
            last = &connector.output_directory[strlen(connector.output_directory)-1];
            if (last[0] != '/'){
                last[1] = '/';
                last[2] = 0;
            }
            break;
        case 'f':
            if(strstr(optarg, "noofdm"))
                connector.ofdm_mode = false;
            else
                connector.ofdm_mode = true;
            break;
        case 's':
            connector.serial_keying = true;
            strcpy(connector.serial_path, optarg);
            break;
        default:
            goto manual;
        }
    }

    pthread_t tid[3];

    pthread_create(&tid[0], NULL, spool_input_directory_thread, (void *) &connector);
    pthread_create(&tid[1], NULL, spool_output_directory_thread, (void *) &connector);

    // pthread_create(&tid[2], NULL, modem_thread, (void *) &connector);
    modem_thread((void *) &connector);

    if (connector.tcp_ret_ok == false){
        // reconnect and call modem_thread again?
        // say something to the spool threads??
        // we cant guarantee nothing about data passed to tnc... pthread_cancel? select?
        // spool needs to re-read the input directory...
    }

    return EXIT_SUCCESS;
}
