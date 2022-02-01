#ifndef _MODELT_PORT_H_

#define _MODELT_PORT_H_ 1

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>

#include <curl/curl.h>


#define PROG_VERSION "4.0"

typedef struct
{
    unsigned char *buffer, idbyte;
    int record_size;
    time_t record_time;
} RECORD_BUFFER;

typedef struct
{
    int status;
    size_t text_size;
    char *text;
} UPLOAD_RESPONSE;

typedef struct
{
    RECORD_BUFFER header, *records;
    int cimel_number, 
    num_records, allocated_records, if_header;
    char eprom[20], *file_name, real_file_name[500];
    UPLOAD_RESPONSE up_res;
} K8_BUFFER;

typedef struct
{
    int fd, packet_timeout;

    char port_name[20], *dir_name, hostname[40], program_version[10];
    int if_open_port, time_interval;
    size_t length_ret, length_data;

    time_t cimel_time, last_time, check_time;

    unsigned char packet_received[4000], packet_send[20], payload_received[2000], *buf, message_number;

    char eprom[20];

    unsigned char *begin, *end;
    int if_flag, header_flag, time_header_flag, empty_event_count,
		time_correction_flag, time_count, cimel_number;


} MY_COM_PORT;

size_t CRC16_Compute(unsigned char *pBuffer, size_t length);
size_t CRC16_Compute_with_number(unsigned char number, unsigned char *pBuffer, size_t length);
unsigned short CRC16_Compute_with_one_number(unsigned char number, unsigned char command_byte);
void form_packet_to_send(unsigned char number, unsigned char command_byte, unsigned char *packet_send);
unsigned char check_received_packet(unsigned char *packet_received, size_t length);
unsigned short convert_hex_ascii_coded_to_byte(unsigned char *code, unsigned char *res, int length);

void close_my_port(MY_COM_PORT *mcport);
int open_my_com_port(MY_COM_PORT *mcport);

int reading_single_port_with_timeout(MY_COM_PORT *mcport);
int receive_packet_from_port(MY_COM_PORT *mcport, unsigned char command_byte);

int receive_cimel_time_from_port(MY_COM_PORT *mcport);
time_t convert_bytes_to_time_t(unsigned char *buf);

unsigned char retrieve_new_record(unsigned char *buf, size_t record_size, RECORD_BUFFER *ptr);

int receive_header_from_port(MY_COM_PORT *mcport, K8_BUFFER *ptr);
int retrieve_k8_buffer_data_only(MY_COM_PORT *mcport, K8_BUFFER *ptr, int max_num);


void init_k8_buffer(K8_BUFFER *ptr);
void free_k8_buffer(K8_BUFFER *ptr);

int retrieve_k8_buffer(MY_COM_PORT *mcport, K8_BUFFER *ptr, int max_num);

static size_t analyze_aeronet_response(void *data, size_t size, size_t nmemb, void *userp);

int libcurl_upload_k8_buffer_to_https(K8_BUFFER *k8b);
int save_k8_buffer_on_disk(char *alt_name, K8_BUFFER *ptr);

void copy_records(RECORD_BUFFER *ptr1, RECORD_BUFFER *ptr2);
void combine_k8_buffers(K8_BUFFER *main_buffer, K8_BUFFER *new_buffer);

time_t read_k8_buffer_from_disk(char *alt_name, K8_BUFFER *ptr);


#endif
