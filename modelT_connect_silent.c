#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <string.h>

#include <curl/curl.h>

#include "modelT_port.h"

int main(int argc, char **argv)
{
    MY_COM_PORT mcport;
    char backup_dir[500], last_time_file[500], *homedir = getenv("HOME"), file_nameh[400],
                                               file_named[400];

    K8_BUFFER  k8bm, k8bh, k8bd;
    struct tm mtim;

    int dev_init, iarg, ii;
    time_t pc_time, new_time, last_time;

    if (argc < 2)
        exit(0);

    sprintf(last_time_file, "%s/last_time.k8", homedir);

    sprintf(backup_dir, "%s/backup", homedir);

    mcport.time_interval = 900; // default : 15 minutes

    mcport.if_open_port = 0;
    sprintf(mcport.port_name, "/dev/tty%s", argv[1]);

    if (argc > 2)
        for (iarg = 2; iarg < argc; iarg++)
        {
            if (!strncmp(argv[iarg], "dir=", 4))
                strcpy(backup_dir, argv[iarg] + 4);
            else if (!strncmp(argv[iarg], "int=", 4))
                mcport.time_interval = atoi(argv[iarg] + 4);
        }

    
    sprintf(k8bm.real_file_name, "%s/", backup_dir);
    sprintf(k8bh.real_file_name, "%s/", backup_dir);
    sprintf(k8bd.real_file_name, "%s/", backup_dir);

    ii = strlen(backup_dir) + 1;

    
    k8bm.file_name = k8bm.real_file_name + ii;
    k8bh.file_name = k8bh.real_file_name + ii;
    k8bd.file_name = k8bd.real_file_name + ii;

    gethostname(mcport.hostname, 39);
    mcport.hostname[39] = '\0';

    last_time = read_k8_buffer_from_disk(last_time_file, &k8bm);

    if (last_time)
    {
         mcport.cimel_number = k8bm.cimel_number;
        strcpy(mcport.eprom, k8bm.eprom);
        mcport.last_time = last_time;

        pc_time = time(NULL);
        gmtime_r(&pc_time, &mtim);

        sprintf(k8bh.file_name, "%s_%04d_%d%02d%02d_%02d.K8", k8bm.eprom, k8bm.cimel_number,
                mtim.tm_year + 1900, mtim.tm_mon + 1, mtim.tm_mday, mtim.tm_hour);
        init_k8_buffer(&k8bh);

        sprintf(k8bd.file_name, "%s_%04d_%d%02d%02d.K8", k8bm.eprom, k8bm.cimel_number,
                mtim.tm_year + 1900, mtim.tm_mon + 1, mtim.tm_mday);
        read_k8_buffer_from_disk(NULL, &k8bd);

        free_k8_buffer(&k8bm);
    }
    else
    {
    
        mcport.cimel_number = -1;
        mcport.last_time = 0;

        init_k8_buffer(&k8bh);
        init_k8_buffer(&k8bd);
    }

    strcpy(mcport.program_version, PROG_VERSION);

    mcport.packet_timeout = 15;

    while (1)
    {
        init_k8_buffer(&k8bm);
        pc_time = time(NULL);
        gmtime_r(&pc_time, &mtim);

        if (open_my_com_port(&mcport))
        {
            if (receive_header_from_port(&mcport, &k8bm))
            {
                if (mcport.cimel_number == -1)
                    dev_init = 1;
                else if ((mcport.cimel_number != k8bm.cimel_number) ||
                         strcmp(mcport.eprom, k8bm.eprom))
                {
                    save_k8_buffer_on_disk(NULL, &k8bd);
                    libcurl_upload_k8_buffer_to_https(&k8bd);

                    free_k8_buffer(&k8bh);
                    free_k8_buffer(&k8bd);
                    dev_init = 2;
                }
                else
                    dev_init = 0;

                if (dev_init)
                {
                   // printf("Redefined  Cimel number = %d  eprom = %s\n", k8bm.cimel_number, k8bm.eprom);
                    mcport.cimel_number = k8bm.cimel_number;
                    strcpy(mcport.eprom, k8bm.eprom);

                    sprintf(k8bh.file_name, "%s_%04d_%d%02d%02d_%02d.K8", k8bm.eprom, k8bm.cimel_number,
                            mtim.tm_year + 1900, mtim.tm_mon + 1, mtim.tm_mday, mtim.tm_hour);
                    sprintf(k8bd.file_name, "%s_%04d_%d%02d%02d.K8", k8bm.eprom, k8bm.cimel_number,
                            mtim.tm_year + 1900, mtim.tm_mon + 1, mtim.tm_mday);
                }
                if (retrieve_k8_buffer_data_only(&mcport, &k8bm, 300))
                {
                   
                    sprintf(k8bm.file_name, "%s_%04d_%d%02d%02d_%02d%02d.K8", k8bm.eprom, k8bm.cimel_number,
                            mtim.tm_year + 1900, mtim.tm_mon + 1, mtim.tm_mday, mtim.tm_hour, mtim.tm_min);
                    save_k8_buffer_on_disk(last_time_file, &k8bm);
                    libcurl_upload_k8_buffer_to_https(&k8bm);

                    combine_k8_buffers(&k8bh, &k8bm);
                    combine_k8_buffers(&k8bd, &k8bm);
                    save_k8_buffer_on_disk(NULL, &k8bd);

                    free_k8_buffer(&k8bm);
                }
            }
            close_my_port(&mcport);
        }
        sprintf(file_nameh, "%s_%04d_%d%02d%02d_%02d.K8", mcport.eprom, mcport.cimel_number,
                mtim.tm_year + 1900, mtim.tm_mon + 1, mtim.tm_mday, mtim.tm_hour);

        if (strcmp(file_nameh, k8bh.file_name))
        {
            libcurl_upload_k8_buffer_to_https(&k8bh);
            free_k8_buffer(&k8bh);

            strcpy(k8bh.file_name, file_nameh);
        }
        sprintf(file_named, "%s_%04d_%d%02d%02d.K8", mcport.eprom, mcport.cimel_number,
                mtim.tm_year + 1900, mtim.tm_mon + 1, mtim.tm_mday);
        if (strcmp(file_named, k8bd.file_name))
        {

            libcurl_upload_k8_buffer_to_https(&k8bd);
            free_k8_buffer(&k8bd);

            strcpy(k8bd.file_name, file_named);
        }

        sleep(mcport.time_interval);
    }
}
