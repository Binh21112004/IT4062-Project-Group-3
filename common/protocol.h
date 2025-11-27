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
#define CMD_FRIEND_INVITE "FRIEND_INVITE"
#define CMD_RES_FRIEND_INVITE "RES_FRIEND_INVITE"
#define CMD_RES_FRIEND_RESPOND "RES_FRIEND_RESPOND"
#define CMD_FRIEND_INVITE_NOTIFICATION "FRIEND_INVITE_NOTIFICATION"
#define CMD_FRIEND_RESPONSE "FRIEND_RESPONSE"
#define CMD_RES_FRIEND_RESPONSE "RES_FRIEND_RESPONSE"
#define CMD_FRIEND_ACCEPTED_NOTIFICATION "FRIEND_ACCEPTED_NOTIFICATION"
#define CMD_FRIEND_REMOVE "FRIEND_REMOVE"
#define CMD_RES_FRIEND_REMOVE "RES_FRIEND_REMOVE"
#define CMD_FRIEND_REMOVED_NOTIFICATION "FRIEND_REMOVED_NOTIFICATION"

// Message structure
typedef struct {
    char command[MAX_COMMAND];
    char json_data[MAX_JSON];
} Message;

// Function prototypes
int send_message(int sock, const char* command, const char* json_data);
int receive_message(int sock, Message* msg);

#endif // PROTOCOL_H
