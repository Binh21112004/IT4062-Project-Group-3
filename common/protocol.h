#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_BUFFER 65536  // 64KB - Đủ lớn cho danh sách bạn bè, events, etc
#define MAX_COMMAND 64
#define MAX_FIELD 256
#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define MAX_EMAIL 100
#define MAX_SESSION_ID 64

#define LOG_FILE_NAME "log_nhom3.txt"
// Command types
#define CMD_REGISTER "REGISTER" 
#define CMD_LOGIN "LOGIN"
#define CMD_LOGOUT "LOGOUT"
#define CMD_CREATE_EVENT "CREATE_EVENT"
#define CMD_GET_EVENTS   "GET_EVENTS"
#define CMD_GET_EVENT_DETAIL "GET_EVENT_DETAIL"
#define CMD_UPDATE_EVENT     "UPDATE_EVENT"
#define CMD_DELETE_EVENT "DELETE_EVENT"
#define CMD_GET_FRIENDS "GET_FRIENDS"
#define CMD_SEND_INVITATION_EVENT "SEND_INVITATION_EVENT"
#define CMD_ACCEPT_INVITATION_REQUEST "ACCEPT_INVITATION_REQUEST"
#define CMD_JOIN_EVENT "JOIN_EVENT"
#define CMD_ACCEPT_JOIN_REQUEST "ACCEPT_JOIN_REQUEST"
#define CMD_SEND_FRIEND_REQUEST "SEND_FRIEND_REQUEST"
#define CMD_ACCEPT_FRIEND_REQUEST "ACCEPT_FRIEND_REQUEST"
#define CMD_REJECT_FRIEND_REQUEST "REJECT_FRIEND_REQUEST"
#define CMD_UNFRIEND "UNFRIEND"
#define CMD_GET_EVENTS_CREBYUSER "GET_EVENTS_CREBYUSER"

// Response codes
#define RESPONSE_OK 200
#define RESPONSE_BAD_REQUEST 400
#define RESPONSE_UNAUTHORIZED 401
#define RESPONSE_CONFLICT 409
#define RESPONSE_UNPROCESSABLE 422 // sai về định dạng mail, tên chứa kí tự đặc biệt
#define RESPONSE_SERVER_ERROR 500
#define RESPONSE_NOT_FOUND 404

// Protocol functions - Xử lý protocol bằng chuỗi với cấp phát động

// Low-level functions - Gửi/nhận chuỗi thô qua socket
// send_message: Gửi chuỗi đã format sẵn qua socket (đảm bảo gửi hết)
int send_message(int sock, const char* message);

// receive_message: Nhận chuỗi từ socket đến khi gặp \r\n (loại bỏ \r\n)
int receive_message(int sock, char* buffer, int buffer_size);

// Client functions - Gửi request và nhận response
// send_request: Gửi request với số lượng fields tùy ý
//   - Tự động cấp phát buffer động theo kích thước cần thiết
//   - Format: COMMAND|FIELD1|FIELD2|...\r\n
//   - Tự động free buffer sau khi gửi
int send_request(int sock, const char* command, const char** fields, int field_count);

// receive_response: Nhận response từ server
//   - Parse response: CODE|MESSAGE|EXTRA_DATA\r\n
//   - extra_data được cấp phát động (có thể rất lớn, dùng cho danh sách...)
//   - Trả về: con trỏ tới extra_data (hoặc NULL nếu không có)
//   - QUAN TRỌNG: Caller phải free(extra_data) sau khi dùng!
char* receive_response(int sock, int* code, char* message, int message_size);

// Server functions - Gửi response và parse request
// send_response: Gửi response tới client
//   - Tự động cấp phát buffer động
//   - Format: CODE|MESSAGE|EXTRA_DATA\r\n (hoặc CODE|MESSAGE\r\n nếu không có extra_data)
//   - extra_data có thể rất lớn (danh sách bạn bè, events, etc.)
//   - Tự động free buffer sau khi gửi
int send_response(int sock, int code, const char* message, const char* extra_data);

// parse_request: Parse chuỗi request thành command và fields (cấp phát động)
//   - Format input: COMMAND|FIELD1|FIELD2|...
//   - Tự động cấp phát mảng fields động (không giới hạn số lượng)
//   - Trả về: mảng con trỏ tới các fields (caller phải free)
//   - QUAN TRỌNG: Caller phải gọi free_fields() sau khi dùng!
char** parse_request(const char* buffer, char* command, int* field_count);

// free_fields: Giải phóng mảng fields được cấp phát từ parse_request
void free_fields(char** fields, int field_count);

// Set request hiện tại (để log dòng này khi trả response)
void protocol_set_current_request_for_log(const char* request_line);

// Send response + ghi log ra file log_nhom3.txt
int send_response_with_log(int client_sock, int code, const char* message, const char* extra_data);
#endif 