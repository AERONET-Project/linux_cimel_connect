#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <termios.h>

#include <sys/ioctl.h>

#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <curl/curl.h>
#include <fcntl.h>
#include <errno.h>
#include "model5_port.h"

size_t handle_aeronet_time_internally(unsigned char *buffer, size_t size, size_t nmemb, AERO_EXCHANGE *ptr)
{

    int i;

    time_t aertime;
    size_t strsize = nmemb * size;
    char time_string[70], *small_string, *ch;

    if (strsize > 60)
        return 0;

    for (i = 0; i < strsize; i++)
        time_string[i] = buffer[i];
    time_string[i] = '\0';

    if (strncmp(time_string, "AERONET Time,", 13))
        return 0;

    small_string = time_string + 13;

    ch = small_string;
    while ((*ch != ',') && (*ch != '\0'))
        ch++;

    if (*ch != ',')
        return 0;

    *ch++ = '\0';

    if (!strncmp(ch, "PC Time,", 8))
        if (!strcmp(ch + 8, ptr->pc_time_string))
            ptr->aeronet_time_real = 1;

    aertime = atol(small_string);

    ptr->aeronet_time = aertime;
    return nmemb * size;
}

time_t receive_aeronet_time(AERO_EXCHANGE *ptr)
{
    CURL *curl;
    CURLcode res;
    time_t cr_time;
    struct tm mtim;

    char url_ref[100];

    cr_time = time(NULL);
    gmtime_r(&cr_time, &mtim);
    sprintf(ptr->pc_time_string, "%02d%02d%02d%02d%02d.000",
            mtim.tm_mon + 1, mtim.tm_mday, mtim.tm_hour, mtim.tm_min, mtim.tm_sec);

    sprintf(url_ref, "https://aeronet.gsfc.nasa.gov/cgi-bin/aeronet_time_new?pc_time=%s", ptr->pc_time_string);

    ptr->aeronet_time_real = 0;

    curl = curl_easy_init();

    if (!curl)
    {
        ptr->aero_reconnect = ptr->aero_connect = 0;

        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url_ref);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_aeronet_time_internally);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ptr);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        ptr->aero_reconnect = ptr->aero_connect = 0;
        return 0;
    }
    curl_easy_cleanup(curl);

    if (!ptr->aero_connect)
        ptr->aero_reconnect = ptr->aero_connect = 1;
    return ptr->aeronet_time;
}

void put_sys_time_to_buffer(time_t *sys_time, unsigned char *buffer)
{
    struct tm mtim;
    gmtime_r(sys_time, &mtim);
    buffer[0] = mtim.tm_year;
    buffer[1] = mtim.tm_mon + 1;
    buffer[2] = mtim.tm_mday;
    buffer[3] = mtim.tm_hour;
    buffer[4] = mtim.tm_min;
    buffer[5] = mtim.tm_sec;
}

time_t get_sys_time_from_buffer(unsigned char *buffer)
{
    struct tm mtim;

    mtim.tm_year = buffer[0];
    mtim.tm_mon = buffer[1] - 1;
    mtim.tm_mday = buffer[2];
    mtim.tm_hour = buffer[3];
    mtim.tm_min = buffer[4];
    mtim.tm_sec = buffer[5];

    return timegm(&mtim);
}

unsigned char convert_char_to_BYTE(unsigned char ch)
{
    if (ch < 48)
        return 0;
    if (ch < 58)
        return ch - 48;
    if (ch < 65)
        return 0;
    if (ch < 71)
        return ch - 55;
    return 0;
}

unsigned char get_decimall(unsigned char ch)
{

    return (ch / 16) * 10 + ch % 16;
}

unsigned char convert_block(unsigned char *buffer, unsigned char *result, int num)
{
    unsigned char *buf = buffer, *res = result, *stop_byte = buffer + num;

    while (buf != stop_byte)
    {
        *res = 16 * convert_char_to_BYTE(buf[0]) + convert_char_to_BYTE(buf[1]);

        res++;
        buf += 2;
    }
    return result[0];
}

time_t get_cimel_time(unsigned char *buffer, int length)
{
    struct tm mtim;

    if (length != 12)
        return 0;

    mtim.tm_hour = (buffer[0] - 48) * 10 + buffer[1] - 48;
    mtim.tm_min = (buffer[2] - 48) * 10 + buffer[3] - 48;
    mtim.tm_sec = (buffer[4] - 48) * 10 + buffer[5] - 48;

    mtim.tm_mday = (buffer[6] - 48) * 10 + buffer[7] - 48;
    mtim.tm_mon = (buffer[8] - 48) * 10 + buffer[9] - 48 - 1;
    mtim.tm_year = (buffer[10] - 48) * 10 + buffer[11] - 48 + 100;
    return timegm(&mtim);
}

void copy_records(RECORD_BUFFER *ptr1, RECORD_BUFFER *ptr2)
{

    ptr1->record_size = ptr2->record_size;

    ptr1->idbyte = ptr2->idbyte;
    ptr1->record_time = ptr2->record_time;

    ptr1->buffer = (unsigned char *)malloc(ptr1->record_size);
    memcpy(ptr1->buffer, ptr2->buffer, ptr1->record_size);
}

void combine_k7_buffers(K7_BUFFER *main_buffer, K7_BUFFER *new_buffer)
{
    int i, new_num, ii;
    RECORD_BUFFER *new_records;

    if (!new_buffer->if_header)
        return;

    if (!main_buffer->if_header)
    {
        copy_records(&main_buffer->header, &new_buffer->header);
        main_buffer->if_header = 1;
        main_buffer->cimel_number = new_buffer->cimel_number;
        strcpy(main_buffer->eprom, new_buffer->eprom);
        if (new_buffer->num_records)
        {
            main_buffer->allocated_records = main_buffer->num_records = new_buffer->num_records;
            main_buffer->records = (RECORD_BUFFER *)malloc(sizeof(RECORD_BUFFER) * main_buffer->allocated_records);
            for (i = 0; i < new_buffer->num_records; i++)
                copy_records(main_buffer->records + i, new_buffer->records + i);
        }
    }
    else if (new_buffer->num_records)
    {
        new_num = main_buffer->allocated_records + new_buffer->num_records;
        new_records = (RECORD_BUFFER *)malloc(sizeof(RECORD_BUFFER) * new_num);

        for (i = 0; i < new_buffer->num_records; i++)
            copy_records(new_records + i, new_buffer->records + i);
        if (main_buffer->num_records)
            for (i = 0; i < main_buffer->num_records; i++)
                copy_records(new_records + i + new_buffer->num_records, main_buffer->records + i);

        main_buffer->allocated_records = main_buffer->num_records = new_num;

        if (main_buffer->allocated_records)
            free(main_buffer->records);

        main_buffer->records = new_records;
    }
}

void init_k7_buffer(K7_BUFFER *ptr)
{
    ptr->num_records = ptr->allocated_records = ptr->if_header = 0;
    ptr->records = NULL;
    ptr->up_res.text_size = 0;
}

void free_k7_buffer(K7_BUFFER *ptr)
{
    int i;

    if (!ptr->if_header)
        return;

    free(ptr->header.buffer);

    if (ptr->num_records)
    {
        for (i = 0; i < ptr->num_records; i++)
            free(ptr->records[i].buffer);
    }

    if (ptr->allocated_records)
    {
        free(ptr->records);
    }

    if (ptr->up_res.text_size)
        free(ptr->up_res.text);

    init_k7_buffer(ptr);
}

void close_my_port(MY_COM_PORT *mcport)
{

    if (!mcport->if_open_port)
        return;

    close(mcport->fd);
    mcport->if_open_port = 0;
}

int open_my_com_port(MY_COM_PORT *mcport)
{

    struct termios options;

    if (mcport->if_open_port)
        return 1;

    mcport->fd = open(mcport->port_name, O_RDWR | O_NOCTTY | O_NDELAY);

    if (mcport->fd < 1)
        return 0;

    fcntl(mcport->fd, F_SETFL, 0);

    tcflush(mcport->fd, TCIOFLUSH);

    tcgetattr(mcport->fd, &options);

    cfsetspeed(&options, B1200);
    cfmakeraw(&options);

    tcsetattr(mcport->fd, TCSANOW, &options);

    mcport->if_open_port = 1;
    mcport->check_time = time(NULL);

    return 1;
}

/*
reading_single_port_with_timeout returns:
-3 : error (port is closed) calling function should abort 
-2, 0 : no response from port during packet_timeout seconds.

1 - proper reading toi mcport->buf[-1] (buf is incremented)

*/

int reading_single_port_with_timeout(MY_COM_PORT *mcport)
{

    fd_set rfds;
    struct timeval timeout;
    int retval, read_bytes;
    unsigned char byte;

    FD_ZERO(&rfds);
    FD_SET(mcport->fd, &rfds);

    timeout.tv_sec = mcport->packet_timeout;
    timeout.tv_usec = 0;
    retval = select(mcport->fd + 1, &rfds, NULL, NULL, &timeout);

    if (retval == -1)
    {
        close_my_port(mcport);
        return -3;
    }

    if (retval == 0)
        return -2;

    if (FD_ISSET(mcport->fd, &rfds))
    {

        read_bytes = read(mcport->fd, &byte, 1);

        if (read_bytes == 1)
        {

            if ((mcport->begin == NULL) && (byte == 2))
                mcport->begin = mcport->buf;
            if ((mcport->end == NULL) && (byte == 23))
                mcport->end = mcport->buf;

            *mcport->buf++ = byte;
            return 1;
        }
    }

    return 0;
}

int check_buffer_for_checksum(MY_COM_PORT *mcport)
{

    unsigned char *buf, checksum, checks;

    //rintf ("\nChecksum shows :  %c%c\n", mcport->end[1], mcport->end[2]);

    convert_block(mcport->end + 1, &checksum, 2);

    buf = mcport->begin;
    checks = 0;

    while (buf != mcport->end + 1)
        checks ^= *buf++;

    return (checks == checksum);
}

void init_port_receiption(MY_COM_PORT *mcport)
{
    mcport->header_flag = mcport->time_header_flag =
        mcport->empty_event_count = mcport->time_correction_flag =
            mcport->time_count = 0;

       mcport->previous_time = 0;
}

void wait_for_new_packet(MY_COM_PORT *mcport)
{
    mcport->begin = mcport->end = NULL;
    mcport->buf = mcport->packet_received;
}

void send_request_to_cimel(MY_COM_PORT *mcport, char *request, int num_bytes)
{
    write(mcport->fd, request, num_bytes);
    wait_for_new_packet(mcport);
}

/*
return : 
-99 : packet received, sequence not right : abort K7 waiting (sends "ZTZ")
0 - packet not completed
-1 - packet completed, checksum is wrong,  sends "uT" to repeat last packet

1 - received header, sends "HT"  sets header_flag to 1 decide on time_correction_flag
2 - received header time (1st), sets time_header_flag to 1, sends jT - start collecting data

3 - received cimel times (3 times) 

4 - received cimel time (3rd time) or no correction and k7 completed

5 - empty event increment sends jT - continue collecting data. 

6 - received data record
-2 - recived data records with errors,  sends jT - continue collecting data. 

*/

int check_if_packet_completed(MY_COM_PORT *mcport, AERO_EXCHANGE *aerex)
{

    int header_size, i;
    time_t aeronet_time;
    struct tm mtim;
    char time_correction_string[20];

    if ((mcport->end == NULL) || (mcport->begin == NULL) || (mcport->buf - mcport->end < 3))
        return 0;

    if (!check_buffer_for_checksum(mcport))
    {
        send_request_to_cimel(mcport, "uT", 2);
        return -1;
    }

    mcport->length_ret = mcport->end - mcport->begin;

    if (mcport->begin[1] == 'S')
    {
        if (mcport->header_flag || (mcport->length_ret % 2))
        {
            send_request_to_cimel(mcport, "ZTZ", 3);
            init_port_receiption(mcport);
            return -99;
        }
        mcport->length_data = (mcport->length_ret - 12) / 2;
        convert_block(mcport->begin + 12, mcport->payload_received, mcport->length_ret - 12);

        mcport->header_flag = 1;

        send_request_to_cimel(mcport, "HT", 2);

        return 1;
    }

    if ((mcport->begin[1] == 'h') || (mcport->begin[1] == 'H') || (mcport->begin[1] == 'R'))
    {
        if (!mcport->header_flag)
        {
            send_request_to_cimel(mcport, "ZTZ", 3);
            init_port_receiption(mcport);
            return -98;
        }

        if (!mcport->time_header_flag)
        {
            mcport->cimel_time = get_cimel_time(mcport->begin + 2, mcport->length_ret - 2);
            mcport->time_header_flag = 1;
            send_request_to_cimel(mcport, "jT", 2);
            return 2;
        }

        if (!mcport->time_correction_flag)
        {
            send_request_to_cimel(mcport, "ZTZ", 3);
            init_port_receiption(mcport);
            return 4;
        }
        if (mcport->time_count == 1)
        {
            mcport->cimel_time = get_cimel_time(mcport->begin + 2, mcport->length_ret - 2);
            aeronet_time = receive_aeronet_time(aerex);
            if (aeronet_time)
            {
                gmtime_r(&aeronet_time, &mtim);

                sprintf(time_correction_string, "R1234%02d%02d%02d%02d%02d%02dT",
                        mtim.tm_hour, mtim.tm_min,
                        mtim.tm_sec, mtim.tm_mday,
                        mtim.tm_mon + 1, mtim.tm_year % 100);
                mcport->begin = mcport->end = NULL;
                mcport->buf = mcport->packet_received;

                send_request_to_cimel(mcport, time_correction_string, 18);
                mcport->time_count++;
                return 3;
            }
        }

        if (mcport->time_count == 3)
        {
            send_request_to_cimel(mcport, "ZTZ", 3);
            init_port_receiption(mcport);
            return 4;
        }

        mcport->time_count++;
        send_request_to_cimel(mcport, "HT", 2);

        return 3;
    }

    if (mcport->begin[1] != '1')
    {
        send_request_to_cimel(mcport, "ZTZ", 3);
        init_port_receiption(mcport);
        return -101 - mcport->begin[1];
    }

    if (!mcport->header_flag || (!mcport->time_header_flag))
    {
        send_request_to_cimel(mcport, "ZTZ", 3);
        init_port_receiption(mcport);
        return -96;
    }

    mcport->length_data = (mcport->length_ret - 2) / 2;
    convert_block(mcport->begin + 2, mcport->payload_received, mcport->length_ret - 2);

    if ((mcport->payload_received[1] != mcport->length_data) || (mcport->payload_received[mcport->length_data - 1] != mcport->length_data) || (mcport->payload_received[0] == 0) || (mcport->payload_received[0] == 255))
    {
        mcport->empty_event_count++;
        if (mcport->empty_event_count == 2)
        {
            if (mcport->time_correction_flag)
            {
                send_request_to_cimel(mcport, "HT", 2);
                return 3;
            }
            send_request_to_cimel(mcport, "ZTZ", 3);
            init_port_receiption(mcport);
            return 4;
        }
        send_request_to_cimel(mcport, "jT", 2);
        return 5;
    }
    mcport->empty_event_count = 0;

    mtim.tm_year = get_decimall(mcport->payload_received[4]) + 100;
    mtim.tm_mon = get_decimall(mcport->payload_received[5]) - 1;
    mtim.tm_mday = get_decimall(mcport->payload_received[6]);
    mtim.tm_hour = get_decimall(mcport->payload_received[7]);
    mtim.tm_min = get_decimall(mcport->payload_received[8]);
    mtim.tm_sec = get_decimall(mcport->payload_received[9]);

    mcport->cimel_time = timegm(&mtim);

    if ((mcport->cimel_time <= mcport->last_time) || ((mcport->previous_time != 0) && (mcport->cimel_time > mcport->previous_time)))
    {
        if (mcport->time_correction_flag)
        {
            send_request_to_cimel(mcport, "HT", 2);
            return 3;
        }
        send_request_to_cimel(mcport, "ZTZ", 3);
        init_port_receiption(mcport);
        return 4;
    }

     mcport->previous_time = mcport->cimel_time;

    send_request_to_cimel(mcport, "jT", 2);
    return 6;
}

/*

returns:
0 - no events or normal flow of data download, or aborted K7 file so continue
1 - reached time interval upon the above condition

2 - completed K7 buffer
3 - reached time interval upon the above condition

8 - received proper header , will continue
 

*/

int main_loop_cycle(MY_COM_PORT *mcport, AERO_EXCHANGE *aerex, K7_BUFFER *k7_buffer)
{

    int retval, i, status;
    time_t new_time, aeronet_time;
    RECORD_BUFFER *ptr;

    retval = reading_single_port_with_timeout(mcport);

    switch (retval)
    {
    case 0:
    case -2:

        /*
perform timeout. if time_interval reached, then action - upload hourle and/or daily file 

*/

        new_time = time(NULL);
        if (new_time > mcport->check_time + mcport->time_interval)
        {
            mcport->check_time = new_time;
            return 1;
        }

        return 0;

        break;

    case -3:
        //printf("Exit\n");
        exit(0);

    case 1:
        status = check_if_packet_completed(mcport, aerex);

        if (status)
        {
            new_time = time(NULL);

          

            if (status == 1)
            {

                k7_buffer->header.buffer = (unsigned char *)malloc(256);
                k7_buffer->header.record_size = 256;
                k7_buffer->if_header = 1;

                memcpy(k7_buffer->header.buffer, mcport->payload_received, mcport->length_data);
                for (i = mcport->length_data; i < 256; i++)
                    k7_buffer->header.buffer[i] = 0;

                return 0;
            }
            else if (status == 2)
            {
                k7_buffer->header.record_time = mcport->cimel_time;

                put_sys_time_to_buffer(&mcport->cimel_time, k7_buffer->header.buffer + 144);
                put_sys_time_to_buffer(&new_time, k7_buffer->header.buffer + 150);
                sprintf(k7_buffer->header.buffer + 162, "%s", mcport->hostname);
                sprintf(k7_buffer->header.buffer + 203, "%s", mcport->program_version);

                aeronet_time = receive_aeronet_time(aerex);

                if (aeronet_time)
                {
                    put_sys_time_to_buffer(&aeronet_time, k7_buffer->header.buffer + 156);
                    if ((aeronet_time > mcport->cimel_time + 10) || (aeronet_time < mcport->cimel_time - 10))
                    {
                        mcport->time_correction_flag = 1;
                    }
                    /*
                    printf("Time difference = %ld", aeronet_time - mcport->cimel_time);
                    if (mcport->time_correction_flag)
                        printf(" Time correction  suggested");
                    printf("\n");
                    */
                    
                }
                else
                {
                    for (i = 156; i < 162; i++)
                        k7_buffer->header.buffer[156] = 255;
                }

                k7_buffer->cimel_number = k7_buffer->header.buffer[3] * 256 + k7_buffer->header.buffer[4];
                for (i = 0; i < 8; i++)
                    k7_buffer->eprom[i] = k7_buffer->header.buffer[i + 128];
                k7_buffer->eprom[8] = '\0';

                //printf("Port %s - header %s  %d\n", mcport->port_name, k7_buffer->eprom, k7_buffer->cimel_number);
                return 8;
            }
            else if (status == 6)
            {
                k7_buffer->allocated_records++;
                k7_buffer->records = (RECORD_BUFFER *)realloc(k7_buffer->records, sizeof(RECORD_BUFFER) * k7_buffer->allocated_records);

                ptr = k7_buffer->records + k7_buffer->num_records;

                ptr->buffer = (unsigned char *)malloc(mcport->length_data);
                memcpy(ptr->buffer, mcport->payload_received, mcport->length_data);
                ptr->idbyte = ptr->buffer[0];
                ptr->record_size = mcport->length_data;
                ptr->record_time = mcport->cimel_time;

                k7_buffer->num_records++;

                //printf("num = %d  idbyte = %d  time = %s", k7_buffer->num_records, ptr->idbyte, ctime(&ptr->record_time));

                return 0;
            }
            else if ((status == -1) || (status == -2) || (status == 3) || (status == 5))
                return 0;
            else if (status < -80)
            {
                free_k7_buffer(k7_buffer);

                if (new_time > mcport->check_time + mcport->time_interval)
                {
                    mcport->check_time = new_time;
                    return 1;
                }

                return 0;
            }
            else if (status == 4)
            {

                if (k7_buffer->num_records)
                    mcport->last_time = k7_buffer->records->record_time;

                if (new_time > mcport->check_time + mcport->time_interval)
                {
                    mcport->check_time = new_time;
                    return 3;
                }

                return 2;
            }
        }
        return 0;
        break;
    }
    return 0;
}

/*
return : 
-99 : packet received, sequence not right : abort K7 waiting (sends "ZTZ")
0 - packet not completed
-1 - packet completed, checksum is wrong,  sends "uT" to repeat last packet

1 - received header, sends "HT"  sets header_flag to 1 decide on time_correction_flag
2 - received header time (1st), sets time_header_flag to 1, sends jT - start collecting data

3 - received cimel times (3 times) 

4 - received cimel time (3rd time) or no correction and k7 completed

5 - empty event increment sends jT - continue collecting data. 

6 - received data record

*/

static size_t analyze_aeronet_response(void *data, size_t size, size_t nmemb, void *userp)
{

    size_t realsize = size * nmemb;

    UPLOAD_RESPONSE *result = (UPLOAD_RESPONSE *)userp;
    result->text = (char *)malloc(realsize + 1);

    memcpy(result->text, data, realsize);
    result->text[realsize] = '\0';
    result->text_size = realsize;

    if (strstr(result->text, "file provided has been queued for processing"))
        result->status = 1;
    else
        result->status = 0;

    return result->text_size;
}

int libcurl_upload_k7_buffer_to_https(K7_BUFFER *k7b)
{
    CURL *curl;
    CURLcode res;

    curl_mime *multipart;
    curl_mimepart *part;

    unsigned char *buffer, *buf;
    size_t buf_size;
    int i;

    if (!k7b->if_header)
        return 0;

    curl = curl_easy_init();

    if (!curl)
        return 0;

    buf_size = k7b->header.record_size;

    for (i = 0; i < k7b->num_records; i++)
        buf_size += k7b->records[i].record_size;

    buf = buffer = (unsigned char *)malloc(buf_size);

    memcpy(buf, k7b->header.buffer, k7b->header.record_size);
    buf += k7b->header.record_size;

    for (i = k7b->num_records - 1; i >= 0; i--)
    {
        memcpy(buf, k7b->records[i].buffer, k7b->records[i].record_size);
        buf += k7b->records[i].record_size;
    }

    multipart = curl_mime_init(curl);

    part = curl_mime_addpart(multipart);
    curl_mime_name(part, "uploaded_file");
    curl_mime_data(part, k7b->file_name, CURL_ZERO_TERMINATED);

    part = curl_mime_addpart(multipart);

    curl_mime_name(part, "uploaded_file");
    curl_mime_data(part, buffer, buf_size);
    curl_mime_filename(part, k7b->file_name);
    curl_mime_type(part, "application/octet-stream");

    curl_easy_setopt(curl, CURLOPT_URL, "https://aeronet.gsfc.nasa.gov/cgi-bin/webfile_trans_auto");

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, multipart);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, analyze_aeronet_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&k7b->up_res);

    res = curl_easy_perform(curl);
    curl_mime_free(multipart);

    curl_easy_cleanup(curl);

    free(buffer);

    if (res != CURLE_OK)
        return 0;

    return 1;
}

int save_k7_buffer_on_disk(char *dir_name, K7_BUFFER *ptr)
{
    int i;
    FILE *out;
    char *real_file_name;

    if (!ptr->if_header)
        return 0;

    real_file_name = (char *)malloc(strlen(dir_name) + strlen(ptr->file_name) + 3);

    sprintf(real_file_name, "%s/%s", dir_name, ptr->file_name);

    out = fopen(real_file_name, "w");
    free(real_file_name);

    if (out == NULL)
        return 0;

    fwrite(ptr->header.buffer, 1, ptr->header.record_size, out);
    for (i = 0; i < ptr->num_records; i++)
        fwrite(ptr->records[i].buffer, 1, ptr->records[i].record_size, out);

    fclose(out);
    return 1;
}

/*

typedef struct {
	unsigned char *buffer, idbyte;
	int record_size;
	time_t record_time;
} RECORD_BUFFER;

typedef struct 
{
	
    RECORD_BUFFER header, *records;
    int cimel_number, 
    num_records, allocated_records, if_header;
    char eprom[20], file_name[100];
    UPLOAD_RESPONSE up_res;

} K7_BUFFER;

*/

time_t read_k7_buffer_from_disk(char *dir_name, K7_BUFFER *ptr)
{
    FILE *in;
    unsigned char *buffer, *buf, *bufend;
    char *real_file_name;
    struct stat bufff;
    RECORD_BUFFER *record;
    size_t file_read, rec_size;

    struct tm mtim;

    int end_read, i;

    init_k7_buffer(ptr);

    real_file_name = (char *)malloc(strlen(dir_name) + strlen(ptr->file_name) + 3);

    sprintf(real_file_name, "%s/%s", dir_name, ptr->file_name);

    if (stat(real_file_name, &bufff))
    {
        free(real_file_name);
        return 0;
    }

    if (bufff.st_size < 256)
    {
        free(real_file_name);
        return 0;
    }

    in = fopen(real_file_name, "r");
    free(real_file_name);

    if (in == NULL)
        return 0;
    buffer = (unsigned char *)malloc(bufff.st_size);
    file_read = fread(buffer, 1, bufff.st_size, in);
    fclose(in);

    //printf("file_read = %ld  file_size = %ld\n", file_read, bufff.st_size);

    if (file_read != bufff.st_size)
    {
        free(buffer);
        return 0;
    }

    ptr->header.buffer = (unsigned char *)malloc(256);
    ptr->header.record_size = 256;
    memcpy(ptr->header.buffer, buffer, 256);
    ptr->header.record_time = get_sys_time_from_buffer(ptr->header.buffer + 144);
    ptr->cimel_number = ptr->header.buffer[3] * 256 + ptr->header.buffer[4];
    for (i = 0; i < 8; i++)
        ptr->eprom[i] = ptr->header.buffer[i + 128];
    ptr->eprom[8] = '\0';

    ptr->if_header = 1;

    buf = buffer + 256;
    end_read = 0;

    bufend = buffer + bufff.st_size;

    while (!end_read && (buf < bufend))
    {

        rec_size = buf[1];
        if (buf + rec_size >= bufend)
            end_read = 1;
        else if (buf[rec_size - 1] != rec_size)
            end_read = 1;
        else
        {
            /* code */
            ptr->allocated_records++;
            ptr->records = (RECORD_BUFFER *)realloc(ptr->records, sizeof(RECORD_BUFFER) * ptr->allocated_records);
            record = ptr->records + ptr->num_records;
            record->buffer = (unsigned char *)malloc(rec_size);
            memcpy(record->buffer, buf, rec_size);

            mtim.tm_year = get_decimall(buf[4]) + 100;
            mtim.tm_mon = get_decimall(buf[5]) - 1;
            mtim.tm_mday = get_decimall(buf[6]);
            mtim.tm_hour = get_decimall(buf[7]);
            mtim.tm_min = get_decimall(buf[8]);
            mtim.tm_sec = get_decimall(buf[9]);
            record->record_time = timegm(&mtim);

            record->idbyte = record->buffer[0];
            record->record_size = rec_size;

            ptr->num_records++;

            buf += rec_size;
        }
    }

    free(buffer);

    return ptr->records->record_time;
}
