#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_BUFFER 4096
#define MAX_COMMAND 64
#define MAX_FIELD 256
#define MAX_FIELDS 10
#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define MAX_EMAIL 100
#define MAX_SESSION_ID 64

// Command types
#define CMD_REGISTER "REGISTER"
#define CMD_LOGIN "LOGIN"
#define CMD_LOGOUT "LOGOUT"

// Response codes
#define RESPONSE_OK 200
#define RESPONSE_BAD_REQUEST 400
#define RESPONSE_UNAUTHORIZED 401
#define RESPONSE_CONFLICT 409
#define RESPONSE_UNPROCESSABLE 422
#define RESPONSE_SERVER_ERROR 500

// Request structure (Client -> Server)
typedef struct {
    char command[MAX_COMMAND];
    char fields[MAX_FIELDS][MAX_FIELD];
    int field_count;
} Request;

// Response structure (Server -> Client)
typedef struct {
    int code;
    char message[MAX_BUFFER];
    char extra_data[MAX_BUFFER];  // For session_id or other extra data
} Response;

// Protocol functions
int send_request(int sock, const char* command, char fields[][MAX_FIELD], int field_count);
int receive_request(int sock, Request* req);
int send_response(int sock, int code, const char* message, const char* extra_data);
int receive_response(int sock, Response* res);

#endif 