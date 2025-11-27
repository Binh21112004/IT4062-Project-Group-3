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
#define MAX_JSON 4000
#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define MAX_EMAIL 100
#define MAX_TOKEN 64

// Command types
#define CMD_REGISTER "REGISTER"
#define CMD_RES_REGISTER "RES_REGISTER"
#define CMD_LOGIN "LOGIN"
#define CMD_RES_LOGIN "RES_LOGIN"
#define CMD_LOGOUT "LOGOUT"
#define CMD_RES_LOGOUT "RES_LOGOUT"

// Message structure
typedef struct {
    char command[MAX_COMMAND];
    char json_data[MAX_JSON];
} Message;

// Function prototypes
int send_message(int sock, const char* command, const char* json_data);
int receive_message(int sock, Message* msg);

#endif // PROTOCOL_H
