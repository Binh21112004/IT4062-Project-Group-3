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
void handle_send_friend_request(ServerContext* ctx, int client_sock, char** fields, int field_count);
void handle_accept_friend_request(ServerContext* ctx, int client_sock, char** fields, int field_count);
void handle_reject_friend_request(ServerContext* ctx, int client_sock, char** fields, int field_count);
void handle_unfriend(ServerContext* ctx, int client_sock, char** fields, int field_count);

void handle_client_request(ServerContext* ctx, int client_sock, const char* buffer);

void handle_create_event(ServerContext* ctx, int client_sock, char** fields, int field_count); // New
void handle_get_events(ServerContext* ctx, int client_sock, char** fields, int field_count); // New
void handle_get_event_detail(ServerContext* ctx, int client_sock, char** fields, int field_count); // Tuan 2
void handle_update_event(ServerContext* ctx, int client_sock, char** fields, int field_count); // Tuan 2
void handle_delete_event(ServerContext* ctx, int client_sock, char** fields, int field_count); // Tuan 2
void handle_get_friends(ServerContext* ctx, int client_sock, char** fields, int field_count); // Tuan 3
void handle_send_invitation_event(ServerContext* ctx, int client_sock, char** fields, int field_count); // Tuan3
void handle_accept_invitation_request(ServerContext* ctx, int client_sock, char** fields, int field_count); // Tuan3
void handle_join_event(ServerContext* ctx, int client_sock, char** fields, int field_count); // Tuan3
void handle_accept_join_request(ServerContext* ctx, int client_sock, char** fields, int field_count); // Tuan3
#endif 
