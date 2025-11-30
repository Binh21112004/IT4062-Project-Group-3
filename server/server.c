#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server.h"
#include "../common/protocol.h"

#define PORT 8888
#define MAX_CLIENTS 100

SessionManager sm;

// Handle REGISTER 
void handle_register(ServerContext* ctx, int client_sock, Request* req) {
    // REGISTER|username|password|email
    if (req->field_count != 3) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }
    
    const char* username = req->fields[0];
    const char* password = req->fields[1];
    const char* email = req->fields[2];
    
    // Validate username for special characters
    if (!db_validate_username(username)) {
        send_response(client_sock, RESPONSE_UNPROCESSABLE, "Username contains special characters", NULL);
        printf("[REGISTER] Failed - Username '%s' contains special characters\n", username);
        return;
    }
    
    // Validate email format
    if (!db_validate_email(email)) {
        send_response(client_sock, RESPONSE_UNPROCESSABLE, "Invalid email format", NULL);
        printf("[REGISTER] Failed - Invalid email format: %s\n", email);
        return;
    }
    
    // Create user
    int user_id = db_create_user(username, password, email);
    
    if (user_id > 0) {
       
        send_response(client_sock, RESPONSE_OK, "Registration successful", NULL);
        printf("[REGISTER] User '%s' created successfully (ID: %d)\n", username, user_id);
    } else if (user_id == -2) {
        
        send_response(client_sock, RESPONSE_CONFLICT, "Username already exists", NULL);
        printf("[REGISTER] Failed - Username '%s' already exists\n", username);
    } else if (user_id == -3) {
       
        send_response(client_sock, RESPONSE_UNPROCESSABLE, "Username contains special characters", NULL);
        printf("[REGISTER] Failed - Invalid username '%s'\n", username);
    } else if (user_id == -4) {
       
        send_response(client_sock, RESPONSE_UNPROCESSABLE, "Invalid email format", NULL);
        printf("[REGISTER] Failed - Invalid email '%s'\n", email);
    } else {
        
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        printf("[REGISTER] Failed - Unknown error\n");
    }
}

// Handle LOGIN 
void handle_login(ServerContext* ctx, int client_sock, Request* req) {
    // LOGIN|username|password
    if (req->field_count != 2) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }
    
    const char* username = req->fields[0];
    const char* password = req->fields[1];
    
    User* user = db_find_user_by_username(username);
    
    if (user == NULL || !db_verify_password(username, password)) {
        // User not found or wrong password
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Invalid username or password", NULL);
        printf("[LOGIN] Failed - Invalid credentials for user '%s'\n", username);
        return;
    }
    
    // Check if user already logged in
    if (session_is_user_logged_in(ctx->sm, user->user_id, client_sock)) {
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Invalid username or password", NULL);
        printf("[LOGIN] Failed - User '%s' already logged in elsewhere\n", username);
        return;
    }
    
    // Create session
    char* token = session_create(ctx->sm, user->user_id, client_sock);
    
    if (token == NULL) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        printf("[LOGIN] Failed - No available session slots\n");
        return;
    }
    
   
    send_response(client_sock, RESPONSE_OK, "Login successful", token);
    printf("[LOGIN] User '%s' logged in successfully (ID: %d, Session: %s)\n", username, user->user_id, token);
}

// Handle LOGOUT 
void handle_logout(ServerContext* ctx, int client_sock, Request* req) {
    // LOGOUT|session_id
    if (req->field_count != 1) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }
    
    const char* session_id = req->fields[0];
    
    // Find session
    Session* session = session_find_by_token(ctx->sm, session_id);
    
    if (session == NULL) {
        send_response(client_sock, RESPONSE_UNAUTHORIZED, "Invalid session ID", NULL);
        printf("[LOGOUT] Failed - Invalid session ID\n");
        return;
    }
    
    int user_id = session->user_id;
    User* user = db_find_user_by_id(user_id);
    
    // Destroy session
    session_destroy(ctx->sm, session_id);
    
    send_response(client_sock, RESPONSE_OK, "Logout successful", NULL);
    
    if (user) {
        printf("[LOGOUT] User '%s' logged out successfully (ID: %d)\n", user->username, user_id);
    } else {
        printf("[LOGOUT] User ID %d logged out successfully\n", user_id);
    }
}

// Handle incoming client requests
void handle_client_request(ServerContext* ctx, int client_sock, Request* req) {
    printf("[REQUEST] Received command: %s\n", req->command);
    
    if (strcmp(req->command, CMD_REGISTER) == 0) {
        handle_register(ctx, client_sock, req);
    } else if (strcmp(req->command, CMD_LOGIN) == 0) {
        handle_login(ctx, client_sock, req);
    } else if (strcmp(req->command, CMD_LOGOUT) == 0) {
        handle_logout(ctx, client_sock, req);
    } else {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        printf("[ERROR] Unknown command: %s\n", req->command);
    }
}

// Thread function for handling each client
void* client_thread(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    
    Request req;
    ServerContext ctx;
    
    ctx.sm = &sm;
    ctx.socket = client_sock;
    
    printf("[CLIENT] New client connected (socket: %d)\n", client_sock);
    
    // Handle client requests
    while (1) {
        int result = receive_request(client_sock, &req);
        if (result <= 0) {
            printf("[CLIENT] Client disconnected (socket: %d)\n", client_sock);
            break;
        }
        
        handle_client_request(&ctx, client_sock, &req);
    }
    
    // Cleanup session when client disconnects
    Session* session = session_find_by_socket(&sm, client_sock);
    if (session != NULL) {
        session_destroy(&sm, session->token);
        printf("[SESSION] Session destroyed for socket %d\n", client_sock);
    }
    
    close(client_sock);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    printf("=== TCP Socket Server ===\n");
    printf("Initializing...\n");
    
    // Initialize database and session manager
    db_init();
    session_init(&sm);
    printf("[DATABASE] File-based database initialized\n");
    printf("[SESSION] Session manager initialized\n");
    
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        close(server_sock);
        return 1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        return 1;
    }
    
    if (listen(server_sock, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_sock);
        return 1;
    }
    
    printf("[SERVER] Listening on port %d\n", PORT);
    printf("[SERVER] Waiting for connections...\n\n");
    
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }
        
        pthread_t thread;
        int* client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_sock;
        
        if (pthread_create(&thread, NULL, client_thread, client_sock_ptr) != 0) {
            perror("Thread creation failed");
            close(client_sock);
            free(client_sock_ptr);
        } else {
            pthread_detach(thread);
        }
    }
    
    close(server_sock);
    db_cleanup();
    return 0;
}
