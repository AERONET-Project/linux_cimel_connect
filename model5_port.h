#ifndef _MODEL5_PORT_H_

#define _MODEL5_PORT_H_ 1

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

#include <curl/curl.h>


#define PROG_VERSION "4.1"

/*
Versiomn history.
4.0 - first raspberry PI oriented version. All stdout output is supressed. Will work on other Linux systems.
4.1 - Fixed the case when upon start Cimel tries to upload the entore memory, but after reaching the bottom reverses back to the top. The variable "previous_time" is introduced to fix the issue.


*/



typedef struct
{
    int status;
    size_t text_size;
    char *text;
} UPLOAD_RESPONSE;

typedef struct 
{
    time_t aeronet_time;
    char pc_time_string[20];
    int aero_connect, aero_reconnect, aeronet_time_real;
} AERO_EXCHANGE;


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


typedef struct
{
    int fd, packet_timeout;

    char port_name[20], *dir_name, hostname[40], program_version[10];
    int if_open_port, time_interval;
    size_t length_ret, length_data;

    time_t cimel_time, last_time, check_time, previous_time;

    unsigned char packet_received[4000], packet_send[20], payload_received[2000], *buf, 
    message_number;

    char eprom[20];

    unsigned char buffer[2000], *begin, *end;
    int if_flag, header_flag, time_header_flag, empty_event_count,
		time_correction_flag, time_count, cimel_number;
} MY_COM_PORT;



size_t handle_aeronet_time_internally(unsigned char *buffer, size_t size, size_t nmemb, AERO_EXCHANGE *ptr);
time_t receive_aeronet_time(AERO_EXCHANGE *ptr);

void put_sys_time_to_buffer (time_t *sys_time, unsigned char *buffer);
time_t get_sys_time_from_buffer (unsigned char *buffer);

unsigned char  convert_char_to_BYTE (unsigned char ch);
unsigned char get_decimall(unsigned char ch);
unsigned char convert_block(unsigned char *buffer, unsigned char *result, int num);


time_t get_cimel_time(unsigned char *buffer, int length);
void close_my_port(MY_COM_PORT *mcport);
int open_my_com_port(MY_COM_PORT *mcport);

int reading_single_port_with_timeout(MY_COM_PORT *mcport);
void wait_for_new_packet (MY_COM_PORT *mcport);
void send_request_to_cimel (MY_COM_PORT *mcport, char *request, int num_bytes);
void init_port_receiption (MY_COM_PORT *mcport);
int check_if_packet_completed(MY_COM_PORT *mcport, AERO_EXCHANGE *aerex);


void copy_records(RECORD_BUFFER *ptr1, RECORD_BUFFER *ptr2);
void combine_k7_buffers(K7_BUFFER *main_buffer, K7_BUFFER *new_buffer);

void init_k7_buffer(K7_BUFFER *ptr);
void free_k7_buffer(K7_BUFFER *ptr);

int main_loop_cycle(MY_COM_PORT *mcport, AERO_EXCHANGE *aerex, K7_BUFFER *k7_buffer);
static size_t analyze_aeronet_response(void *data, size_t size, size_t nmemb, void *userp);
int libcurl_upload_k7_buffer_to_https(K7_BUFFER *k7b);
int save_k7_buffer_on_disk(char *dir_name, K7_BUFFER *ptr);
time_t read_k7_buffer_from_disk(char *dir_name, K7_BUFFER *ptr);

#endif
