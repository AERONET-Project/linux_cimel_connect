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

size_t CRC16_Compute(unsigned char *pBuffer, size_t length)
{
    unsigned char i;
    int bs;
    size_t crc = 0;
    while (length--)
    {
        crc ^= *pBuffer++;
        for (i = 0; i < 8; i++)
        {
            bs = crc & 1;
            crc >>= 1;
            if (bs)
            {
                crc ^= 0xA001;
            }
        }
    }
    return crc;
}
size_t CRC16_Compute_with_number(unsigned char number, unsigned char *pBuffer, size_t length)
{
    unsigned char i;
    int bs;
    size_t crc = 0;

    crc ^= number;
    for (i = 0; i < 8; i++)
    {
        bs = crc & 1;
        crc >>= 1;
        if (bs)
        {
            crc ^= 0xA001;
        }
    }

    while (length--)
    {
        crc ^= *pBuffer++;
        for (i = 0; i < 8; i++)
        {
            bs = crc & 1;
            crc >>= 1;
            if (bs)
            {
                crc ^= 0xA001;
            }
        }
    }
    return crc;
}

unsigned short CRC16_Compute_with_one_number(unsigned char number, unsigned char command_byte)
{
    unsigned char i;
    int bs;
    unsigned short crc = 0;

    crc ^= number;
    for (i = 0; i < 8; i++)
    {
        bs = crc & 1;
        crc >>= 1;
        if (bs)
        {
            crc ^= 0xA001;
        }
    }

    crc ^= command_byte;
    for (i = 0; i < 8; i++)
    {
        bs = crc & 1;
        crc >>= 1;
        if (bs)
        {
            crc ^= 0xA001;
        }
    }
    return crc;
}

void form_packet_to_send(unsigned char number, unsigned char command_byte, unsigned char *packet_send)
{

    size_t crc;
    char outstring[10];

    packet_send[0] = 0x01;
    packet_send[1] = number;
    packet_send[2] = 0x02;
    packet_send[3] = command_byte;

    packet_send[4] = 0x17;

    crc = CRC16_Compute_with_one_number(number, command_byte);
    sprintf(outstring, "%04lX", crc);

    packet_send[5] = outstring[2];
    packet_send[6] = outstring[3];
    packet_send[7] = outstring[0];
    packet_send[8] = outstring[1];

    packet_send[9] = 0x03;
}

unsigned char check_received_packet(unsigned char *packet_received, size_t length)
{

    size_t crc;
    char outstring[10];

    if (length < 10)
        return 0;
    if (length > 32767)
        return 0;

    if (packet_received[0] != 0x01)
        return 0;
    if (packet_received[2] != 0x02)
        return 0;

    if (packet_received[length - 1] != 0x03)
        return 0;
    if (packet_received[length - 6] != 0x17)
        return 0;

    crc = CRC16_Compute_with_number(packet_received[1], packet_received + 3, length - 9);

    sprintf(outstring, "%04lX", crc);

    if (packet_received[length - 5] != outstring[2])
        return 0;
    if (packet_received[length - 4] != outstring[3])
        return 0;
    if (packet_received[length - 3] != outstring[0])
        return 0;
    if (packet_received[length - 2] != outstring[1])
        return 0;

    return packet_received[1];
}

unsigned short convert_hex_ascii_coded_to_byte(unsigned char *code, unsigned char *res, int length)
{
    int new_len = length / 2;
    unsigned char *buf = code, *ch = res;

    while (new_len--)
    {
        *ch = 0;

        if ((*buf > 47) && (*buf < 58))
            (*ch) += (*buf - 48) * 16;
        else if ((*buf > 64) && (*buf < 71))
            (*ch) += (*buf - 55) * 16;
        else
            return 0;

        buf++;

        if ((*buf > 47) && (*buf < 58))
            (*ch) += *buf - 48;
        else if ((*buf > 64) && (*buf < 71))
            (*ch) += *buf - 55;
        else
            return 0;

        buf++;
        ch++;
    }

    return length / 2;
}

void close_my_port(MY_COM_PORT *mcport)
{

    if (!mcport->if_open_port)
        return;

    close(mcport->fd);
    mcport->if_open_port = 0;
}
/*
mcport must have port_name and packet_timeout  set. 

if_open_port has to be 0

*/
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

    cfsetspeed(&options, B9600);
    cfmakeraw(&options);

    tcsetattr(mcport->fd, TCSANOW, &options);

    mcport->if_open_port = 1;
    mcport->message_number = 0x21;

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

        read_bytes = read(mcport->fd, mcport->buf, 1);

        if (read_bytes == 1)
        {
            mcport->buf++;
            return 1;
        }
    }

    return 0;
}

/*
receive_packet_from_port returns:
-98 : port is not open
-99 : error on line. Port closed by reading_single_port_with_timeout
-97 : buffer overflow (> 3000 bytes received , with no stop
-1 : no stop byte (0x03) when communication stopped. packet will not be processed
0, -2, -3, -4: 

packet received failed consistency test

1 or more : packet good.


*/

int receive_packet_from_port(MY_COM_PORT *mcport, unsigned char command_byte)
{

    unsigned char number_out;
    int stop_byte_found = 0, read_bytes, retnum;

    if (!mcport->if_open_port)
        return -98;

    //form the data packet to communicate with the Cimel Model T control box
    form_packet_to_send(mcport->message_number, command_byte, mcport->packet_send);
    mcport->buf = mcport->packet_received;

    //send packet to the com port

    write(mcport->fd, mcport->packet_send, 10);

    //wait for the proper response from the control box

    while (!stop_byte_found)
    {
        retnum = reading_single_port_with_timeout(mcport);

        if (retnum == -3)
            return -99; // error on line. Port closed by reading_single_port_with_timeout

        if (retnum == -2)
            return -1;

        if (retnum == 1)
        {
            if (mcport->buf[-1] == 0x03)
                stop_byte_found = 1;

            mcport->length_ret = mcport->buf - mcport->packet_received;

            if (!stop_byte_found && (mcport->length_ret > 3000))
                return -97;
        }
    }

    //check data bytes consistency
    number_out = check_received_packet(mcport->packet_received, mcport->length_ret);

    if (!number_out)
        return 0;

    if (number_out != mcport->message_number)
        return -2;

    mcport->message_number++;
    if (mcport->message_number == 0x80)
        mcport->message_number = 0x21;

    if (command_byte + 0x80 != mcport->packet_received[3])
        return -3;

    //convert the data bytes
    mcport->length_data = convert_hex_ascii_coded_to_byte(mcport->packet_received + 4,
                                                          mcport->payload_received, mcport->length_ret - 10);

    if (!mcport->length_data)
        return -4;

    return (mcport->length_data);
}

int receive_cimel_time_from_port(MY_COM_PORT *mcport)
{
    struct tm mtim;

    mcport->cimel_time = 0;

    if (receive_packet_from_port(mcport, 'A') != 6)
        return 0;

    if ((mcport->payload_received[0] + 2000 <= 2017 || mcport->payload_received[0] + 2000 >= 2050) &&
            (mcport->payload_received[1] < 1 || mcport->payload_received[1] > 12) ||
        (mcport->payload_received[2] < 1 || mcport->payload_received[2] > 31) ||
        (mcport->payload_received[3] < 0 || mcport->payload_received[3] > 23) ||
        (mcport->payload_received[4] < 0 || mcport->payload_received[4] > 59) ||
        (mcport->payload_received[5] < 0 || mcport->payload_received[5] > 59))
        return 0;

    mtim.tm_year = mcport->payload_received[0] + 100;
    mtim.tm_mon = mcport->payload_received[1] - 1;
    mtim.tm_mday = mcport->payload_received[2];
    mtim.tm_hour = mcport->payload_received[3];
    mtim.tm_min = mcport->payload_received[4];
    mtim.tm_sec = mcport->payload_received[5];

    mcport->cimel_time = timegm(&mtim);

    if (!mcport->cimel_time)
        return 0;

    return 1;
}

time_t convert_bytes_to_time_t(unsigned char *buf)
{

    struct tm mtim;
    mtim.tm_year = buf[3] / 4 + 100;
    mtim.tm_mon = buf[3] % 4 * 4 + buf[2] / 64 - 1;
    mtim.tm_mday = buf[2] % 64 / 2;
    mtim.tm_hour = buf[2] % 2 * 16 + buf[1] / 16;
    mtim.tm_min = buf[1] % 16 * 4 + buf[0] / 64;
    mtim.tm_sec = buf[0] % 64;

    return timegm(&mtim);
}

unsigned char retrieve_new_record(unsigned char *buf, size_t record_size, RECORD_BUFFER *ptr)
{

    size_t i;

    if (buf[0] > 0x7F)
        return 0;

    ptr->record_size = buf[1] + (buf[2] % 64) * 256;

    if (ptr->record_size != record_size)
        return 0;
    if (buf[1] != buf[record_size - 2])
        return 0;
    if (buf[2] != buf[record_size - 1])
        return 0;

    if (buf[record_size - 3] != 0xFE)
        return 0;

    ptr->idbyte = buf[0] % 0x80;
    ptr->record_time = convert_bytes_to_time_t(buf + 3);

    ptr->buffer = (unsigned char *)malloc(record_size);

    for (i = 0; i < record_size; i++)
        ptr->buffer[i] = buf[i];

    return 1;
}

int receive_header_from_port(MY_COM_PORT *mcport, K8_BUFFER *ptr)
{
    int status;
    short *nums;

    status = receive_packet_from_port(mcport, 'G');

    /*
       Here all checks for the size of Header is removed (except it cannot be less than 56 - the smallest ever header of K8)
       We assume that if packet goes thru consistency check, shows id byte as 0x7C and is at least 56 bytes long, than it
        is a valid header. More checks will be done on "aeronet".
       For this program we only need cimel number (bytes 15, 16) and software version (bytes 9,10). They are on the same places in 
       all headers so far (as of this writing, August 5, 2020)

       Now if Cimel modify the size of header again, the current program will continue operating without recompile.

    */
    if (status < 56)
        return 0;

    if (!retrieve_new_record(mcport->payload_received, status, &ptr->header))
        return 0;

    if (ptr->header.idbyte != 0x7C)
    {
        free(ptr->header.buffer);
        return 0;
    }

    nums = (short *)(ptr->header.buffer + 15);
    ptr->cimel_number = nums[0];

    // form "eprom" from soft_ver_major (ptr->header.buffer[9]) and soft_ver_manor(ptr->header.buffer[10])
    sprintf(ptr->eprom, "SP810%02X%02X", ptr->header.buffer[9], ptr->header.buffer[10]);

    ptr->if_header = 1;

    return 1;
}

void init_k8_buffer(K8_BUFFER *ptr)
{
    ptr->num_records = ptr->allocated_records = ptr->if_header = 0;
    ptr->records = NULL;
    ptr->up_res.text_size = 0;
}

void free_k8_buffer(K8_BUFFER *ptr)
{
    int i;
    if (ptr->num_records)
    {
        for (i = 0; i < ptr->num_records; i++)
            free(ptr->records[i].buffer);
    }

    if (ptr->allocated_records)
    {
        free(ptr->records);
        free(ptr->header.buffer);
    }

    if (ptr->up_res.text_size)
        free(ptr->up_res.text);

    init_k8_buffer(ptr);
}

int retrieve_k8_buffer(MY_COM_PORT *mcport, K8_BUFFER *ptr, int max_num)
{
    unsigned char command_byte;
    int continue_retrieval = 1;

    if (!ptr->if_header)
    {
        if (!receive_header_from_port(mcport, ptr))
            return 0;
    }

    if (!ptr->allocated_records)
    {
        ptr->allocated_records = 20;

        ptr->records = (RECORD_BUFFER *)malloc(sizeof(RECORD_BUFFER) * ptr->allocated_records);
    }
    command_byte = 'C';

    while (continue_retrieval)
    {

        if (receive_packet_from_port(mcport, command_byte) < 1)
            continue_retrieval = 0;
        else
        {

            if (!retrieve_new_record(mcport->payload_received, mcport->length_data, ptr->records + ptr->num_records))
                continue_retrieval = 0;
            else
            {
                if (command_byte == 'C')
                    command_byte = 'D';

                if (ptr->records[ptr->num_records].record_time <= mcport->last_time)
                    continue_retrieval = 0;

                if (continue_retrieval)
                {
                    ptr->num_records++;
                    if (max_num && (ptr->num_records == max_num))
                        continue_retrieval = 0;
                    else if (ptr->num_records == ptr->allocated_records)
                    {
                        ptr->allocated_records += 20;
                        ptr->records = (RECORD_BUFFER *)realloc(ptr->records, sizeof(RECORD_BUFFER) * ptr->allocated_records);
                    }
                }
            }
        }
    }

    if (!ptr->num_records)
    {
        free_k8_buffer(ptr);
        return 0;
    }

    mcport->last_time = ptr->records->record_time;

    return 1;
}


int retrieve_k8_buffer_data_only(MY_COM_PORT *mcport, K8_BUFFER *ptr, int max_num)
{
    unsigned char command_byte;
    int continue_retrieval = 1;

    command_byte = 'C';

    while (continue_retrieval)
    {

        if (receive_packet_from_port(mcport, command_byte) < 1)
            continue_retrieval = 0;
        else
        {

            ptr->allocated_records ++;
            ptr->records = (RECORD_BUFFER *)realloc(ptr->records, sizeof(RECORD_BUFFER) * ptr->allocated_records);
 
            if (!retrieve_new_record(mcport->payload_received, mcport->length_data, ptr->records + ptr->num_records))
                continue_retrieval = 0;
            else
            {
                if (command_byte == 'C')
                    command_byte = 'D';

                if (ptr->records[ptr->num_records].record_time <= mcport->last_time)
                    continue_retrieval = 0;

                if (continue_retrieval)
                {
                    ptr->num_records++;
                    if (max_num && (ptr->num_records == max_num))
                        continue_retrieval = 0;
  
                }
            }
        }
    }

    if (!ptr->num_records) return 0;

    mcport->last_time = ptr->records->record_time;

    return 1;
}



static size_t analyze_aeronet_response(void *data, size_t size, size_t nmemb, void *userp)
{

    size_t realsize = size * nmemb;

    UPLOAD_RESPONSE *result = (UPLOAD_RESPONSE *)userp;
    result->text = (char *)malloc(realsize + 1);

    memcpy(result->text, data, realsize);
    result->text[realsize] = '\0';
    result->text_size = realsize;

    if (strstr(result->text, "The K8 file provided has been queued for processing"))
        result->status = 1;
    else
        result->status = 0;

    return result->text_size;
}

int libcurl_upload_k8_buffer_to_https(K8_BUFFER *k8b)
{
    CURL *curl;
    CURLcode res;

    curl_mime *multipart;
    curl_mimepart *part;

    unsigned char *buffer, *buf;
    size_t buf_size;
    int i;

    curl = curl_easy_init();

    if (!curl)
        return 0;

    if (!k8b->if_header)
        return 0;

    buf_size = k8b->header.record_size;

    for (i = 0; i < k8b->num_records; i++)
        buf_size += k8b->records[i].record_size;

    buf = buffer = (unsigned char *)malloc(buf_size);

    memcpy(buf, k8b->header.buffer, k8b->header.record_size);
    buf += k8b->header.record_size;

    for (i = k8b->num_records - 1; i >= 0; i--)
    {
        memcpy(buf, k8b->records[i].buffer, k8b->records[i].record_size);
        buf += k8b->records[i].record_size;
    }

    multipart = curl_mime_init(curl);

    part = curl_mime_addpart(multipart);
    curl_mime_name(part, "uploaded_file");
    curl_mime_data(part, k8b->file_name, CURL_ZERO_TERMINATED);

    part = curl_mime_addpart(multipart);

    curl_mime_name(part, "uploaded_file");
    curl_mime_data(part, buffer, buf_size);
    curl_mime_filename(part, k8b->file_name);
    curl_mime_type(part, "application/octet-stream");

    curl_easy_setopt(curl, CURLOPT_URL, "https://aeronet.gsfc.nasa.gov/cgi-bin/webfile_trans_auto");

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, multipart);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, analyze_aeronet_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&k8b->up_res);

    res = curl_easy_perform(curl);
    curl_mime_free(multipart);

    curl_easy_cleanup(curl);

    free(buffer);

    if (res != CURLE_OK)
        return 0;

    return 1;
}

int save_k8_buffer_on_disk(char *alt_name, K8_BUFFER *ptr)
{
    int i;
    FILE *out;
    char *save_file_name;
    
        if (!ptr->if_header)
        return 0;

    if (alt_name == NULL)
        save_file_name = ptr->real_file_name;
    else
        save_file_name = alt_name;

    out = fopen(save_file_name, "w");

    if (out == NULL)
        return 0;

    fwrite(ptr->header.buffer, 1, ptr->header.record_size, out);
    for (i = 0; i < ptr->num_records; i++)
        fwrite(ptr->records[i].buffer, 1, ptr->records[i].record_size, out);

    fclose(out);
    return 1;
}

void copy_records(RECORD_BUFFER *ptr1, RECORD_BUFFER *ptr2)
{

    ptr1->record_size = ptr2->record_size;

    ptr1->idbyte = ptr2->idbyte;
    ptr1->record_time = ptr2->record_time;

    ptr1->buffer = (unsigned char *)malloc(ptr1->record_size);
    memcpy(ptr1->buffer, ptr2->buffer, ptr1->record_size);
}

void combine_k8_buffers(K8_BUFFER *main_buffer, K8_BUFFER *new_buffer)
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

        if (main_buffer->allocated_records)
            free(main_buffer->records);

        main_buffer->allocated_records = new_num;
        main_buffer->num_records += new_buffer->num_records;

      
        main_buffer->records = new_records;
    }
}

time_t read_k8_buffer_from_disk(char *alt_name, K8_BUFFER *ptr)
{
    FILE *in;
    unsigned char *buffer, *buf, *bufend;
    char *read_file_name;
    struct stat bufff;

    size_t file_read, rec_size;

    short *nums;

    int num_recs = 0, ii, head_size, end_read;

    init_k8_buffer(ptr);

    if (alt_name == NULL)
        read_file_name = ptr->real_file_name;
    else
        read_file_name = alt_name;

    if (stat(read_file_name, &bufff))
        return 0;
    if (bufff.st_size < 56)
        return 0;

    in = fopen(read_file_name, "r");

    if (in == NULL)
        return 0;

    buffer = (unsigned char *)malloc(bufff.st_size);
    file_read = fread(buffer, 1, bufff.st_size, in);
    fclose(in);

    //   printf("file_read = %ld  file_size = %ld\n", file_read, file_size);

    if (file_read != bufff.st_size)
    {
        free(buffer);
        return 0;
    }

    buf = buffer;
    end_read = 0;
    bufend = buffer + bufff.st_size;

    while (!end_read && (buf < bufend))
    {
        rec_size = buf[1] + (buf[2] % 64) * 256;

        if ((buf - buffer) + rec_size - 1 >= bufff.st_size)
            end_read = 1;
        else if ((buf[1] != buf[rec_size - 2]) || (buf[2] != buf[rec_size - 1]))
            end_read = 1;
        else
        {
            if (!ptr->if_header)
            {
                if (!retrieve_new_record(buf, rec_size, &ptr->header))
                {
                    free(buffer);
                    return 0;
                }

                if (ptr->header.idbyte != 0x7C)
                {
                    free(ptr->header.buffer);
                    free(buffer);
                    return 0;
                }

                nums = (short *)(ptr->header.buffer + 15);
                ptr->cimel_number = nums[0];

                sprintf(ptr->eprom, "SP810%02X%02X", ptr->header.buffer[9], ptr->header.buffer[10]);
                ptr->if_header = 1;
                buf += rec_size;
            }
            else
            {
                ptr->allocated_records++;
                ptr->records = (RECORD_BUFFER *)realloc(ptr->records, sizeof(RECORD_BUFFER) * ptr->allocated_records);
                if (!retrieve_new_record(buf, rec_size, ptr->records + ptr->num_records))
                    end_read = 1;
                else
                {
                    ptr->num_records++;
                    buf += rec_size;
                }
            }
        }
    }

  

    free(buffer);

    return ptr->records->record_time;
}
