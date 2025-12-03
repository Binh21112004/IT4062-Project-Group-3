#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server.h"
#include "config.h"
#include "../common/protocol.h"

#define PORT 8888
#define MAX_CLIENTS 100

SessionManager sm;

// Handle REGISTER 
void handle_register(ServerContext* ctx, int client_sock, char** fields, int field_count) {
    (void)ctx;  // Tránh warning unused parameter
    // REGISTER|username|password|email
    if (field_count != 3) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }
    
    const char* username = fields[0];
    const char* password = fields[1];
    const char* email = fields[2];
    
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
void handle_login(ServerContext* ctx, int client_sock, char** fields, int field_count) {
    // LOGIN|username|password
    if (field_count != 2) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }
    
    const char* username = fields[0];
    const char* password = fields[1];
    
    // Verify password first
    if (!db_verify_password(username, password)) {
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Invalid username or password", NULL);
        printf("[LOGIN] Failed - Invalid credentials for user '%s'\n", username);
        return;
    }
    
    // Get user info
    int user_id;
    char email[MAX_EMAIL];
    int is_active;
    
    if (db_find_user_by_username(username, &user_id, email, sizeof(email), &is_active) <= 0 || !is_active) {
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Invalid username or password", NULL);
        printf("[LOGIN] Failed - User '%s' not found or inactive\n", username);
        return;
    }
    
    // Check if user already logged in
    if (session_is_user_logged_in(ctx->sm, user_id, client_sock)) {
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Invalid username or password", NULL);
        printf("[LOGIN] Failed - User '%s' already logged in elsewhere\n", username);
        return;
    }
    
    // Create session
    char* token = session_create(ctx->sm, user_id, client_sock);
    
    if (token == NULL) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        printf("[LOGIN] Failed - No available session slots\n");
        return;
    }
    
   
    send_response(client_sock, RESPONSE_OK, "Login successful", token);
    printf("[LOGIN] User '%s' logged in successfully (ID: %d, Session: %s)\n", username, user_id, token);
}

// Handle LOGOUT 
void handle_logout(ServerContext* ctx, int client_sock, char** fields, int field_count) {
    // LOGOUT|session_id
    if (field_count != 1) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }
    
    const char* session_id = fields[0];
    
    // Find session
    Session* session = session_find_by_token(ctx->sm, session_id);
    
    if (session == NULL) {
        send_response(client_sock, RESPONSE_UNAUTHORIZED, "Invalid session ID", NULL);
        printf("[LOGOUT] Failed - Invalid session ID\n");
        return;
    }
    
    int user_id = session->user_id;
    
    // Get username for logging
    char username[MAX_USERNAME];
    char email[MAX_EMAIL];
    int is_active;
    int found = db_find_user_by_id(user_id, username, sizeof(username), email, sizeof(email), &is_active);
    
    // Destroy session
    session_destroy(ctx->sm, session_id);
    
    send_response(client_sock, RESPONSE_OK, "Logout successful", NULL);
    
    if (found > 0) {
        printf("[LOGOUT] User '%s' logged out successfully (ID: %d)\n", username, user_id);
    } else {
        printf("[LOGOUT] User ID %d logged out successfully\n", user_id);
    }
}

// Handle incoming client requests
void handle_client_request(ServerContext* ctx, int client_sock, const char* buffer) {
    char command[MAX_COMMAND];
    int field_count;
    
    // Parse chuỗi request (cấp phát động)
    char** fields = parse_request(buffer, command, &field_count);
    
    if (fields == NULL && field_count > 0) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }
    
    printf("[REQUEST] Received command: %s\n", command);
    
    if (strcmp(command, CMD_REGISTER) == 0) {
        handle_register(ctx, client_sock, fields, field_count);
    } else if (strcmp(command, CMD_LOGIN) == 0) {
        handle_login(ctx, client_sock, fields, field_count);
    } else if (strcmp(command, CMD_LOGOUT) == 0) {
        handle_logout(ctx, client_sock, fields, field_count);
    } else {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        printf("[ERROR] Unknown command: %s\n", command);
    }
    
    // Giải phóng fields
    free_fields(fields, field_count);
}

// Thread function for handling each client
void* client_thread(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    
    char buffer[MAX_BUFFER];
    ServerContext ctx;
    
    ctx.sm = &sm;
    ctx.socket = client_sock;
    
    printf("[CLIENT] New client connected (socket: %d)\n", client_sock);
    
    // Handle client requests
    while (1) {
        int result = receive_message(client_sock, buffer, MAX_BUFFER);
        if (result <= 0) {
            printf("[CLIENT] Client disconnected (socket: %d)\n", client_sock);
            break;
        }
        
        handle_client_request(&ctx, client_sock, buffer);
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
    
    // Load database configuration
    DatabaseConfig db_config;
    if (config_load_database("config/database.conf", &db_config) < 0) {
        fprintf(stderr, "Failed to load database configuration\n");
        return 1;
    }
    
    // Build connection string and initialize PostgreSQL database
    char* conninfo = config_build_conninfo(&db_config);
    printf("[CONFIG] Connecting to database: %s@%s:%s/%s\n", 
           db_config.user, db_config.host, db_config.port, db_config.dbname);
    
    if (db_init(conninfo) < 0) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    
    // Initialize session manager
    session_init(&sm);
    printf("[DATABASE] PostgreSQL database connected successfully\n");
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
