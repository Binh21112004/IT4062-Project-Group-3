#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/protocol.h"
#include "file_db.h"
#include "session.h"

#define PORT 8888
#define MAX_CLIENTS 100

typedef struct {
    int socket;
    SessionManager* sm;
} ServerContext;

// Handler functions
void handle_register(ServerContext* ctx, int client_sock, Request* req);
void handle_login(ServerContext* ctx, int client_sock, Request* req);
void handle_logout(ServerContext* ctx, int client_sock, Request* req);
void handle_client_request(ServerContext* ctx, int client_sock, Request* req);

#endif 
