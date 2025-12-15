#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/protocol.h"
#include "postgres_db.h"
#include "session.h"

#define PORT 8888
#define MAX_CLIENTS 100

typedef struct {
    int socket;
    SessionManager* sm;
} ServerContext;

// Handler functions
void handle_register(ServerContext* ctx, int client_sock, char** fields, int field_count);
void handle_login(ServerContext* ctx, int client_sock, char** fields, int field_count);
void handle_logout(ServerContext* ctx, int client_sock, char** fields, int field_count);
void handle_client_request(ServerContext* ctx, int client_sock, const char* buffer);
void handle_create_event(ServerContext* ctx, int client_sock, char** fields, int field_count); // New
void handle_get_events(ServerContext* ctx, int client_sock, char** fields, int field_count); // New
void handle_get_event_detail(ServerContext* ctx, int client_sock, char** fields, int field_count); // Tuan 2

#endif 
