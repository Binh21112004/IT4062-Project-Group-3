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
#include "../common/cJSON.h"

#define PORT 8888
#define MAX_CLIENTS 100

SessionManager sm;

// Handle REGISTER command
void handle_register(ServerContext* ctx, int client_sock, const char* json_data) {
    cJSON *request = cJSON_Parse(json_data);
    cJSON *response_json = NULL;
    char *response = NULL;
    
    if (request == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Invalid JSON");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_REGISTER, response);
        free(response);
        cJSON_Delete(response_json);
        return;
    }
    
    cJSON *username_item = cJSON_GetObjectItem(request, "username");
    cJSON *password_item = cJSON_GetObjectItem(request, "password");
    cJSON *email_item = cJSON_GetObjectItem(request, "email");
    
    // Validate required fields
    if (!cJSON_IsString(username_item) || !cJSON_IsString(password_item) || !cJSON_IsString(email_item)) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Missing required fields");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_REGISTER, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    const char *username = username_item->valuestring;
    const char *password = password_item->valuestring;
    const char *email = email_item->valuestring;
    
    // Create user
    int user_id = db_create_user(username, password, email);
    
    response_json = cJSON_CreateObject();
    
    if (user_id > 0) {
        // Success
        cJSON_AddNumberToObject(response_json, "code", 201);
        cJSON_AddStringToObject(response_json, "message", "Account created");
        printf("[REGISTER] User '%s' created successfully (ID: %d)\n", username, user_id);
    } else if (user_id == -2) {
        // Username exists
        cJSON_AddNumberToObject(response_json, "code", 409);
        cJSON_AddStringToObject(response_json, "message", "Username already exists");
        printf("[REGISTER] Failed - Username '%s' already exists\n", username);
    } else if (user_id == -3) {
        // Invalid username
        cJSON_AddNumberToObject(response_json, "code", 422);
        cJSON_AddStringToObject(response_json, "message", "Username contains invalid characters");
        printf("[REGISTER] Failed - Invalid username '%s'\n", username);
    } else {
        // Unknown error
        cJSON_AddNumberToObject(response_json, "code", 500);
        cJSON_AddStringToObject(response_json, "message", "Unknown error occurred");
        printf("[REGISTER] Failed - Unknown error\n");
    }
    
    response = cJSON_PrintUnformatted(response_json);
    send_message(client_sock, CMD_RES_REGISTER, response);
    
    free(response);
    cJSON_Delete(response_json);
    cJSON_Delete(request);
}

// Handle LOGIN command
void handle_login(ServerContext* ctx, int client_sock, const char* json_data) {
    cJSON *request = cJSON_Parse(json_data);
    cJSON *response_json = NULL;
    char *response = NULL;
    
    if (request == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Invalid JSON");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_LOGIN, response);
        free(response);
        cJSON_Delete(response_json);
        return;
    }
    
    cJSON *username_item = cJSON_GetObjectItem(request, "username");
    cJSON *password_item = cJSON_GetObjectItem(request, "password");
    
    if (!cJSON_IsString(username_item) || !cJSON_IsString(password_item)) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Missing required fields");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_LOGIN, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    const char *username = username_item->valuestring;
    const char *password = password_item->valuestring;
    
    User* user = db_find_user_by_username(username);
    
    response_json = cJSON_CreateObject();
    
    if (user == NULL) {
        // User not found
        cJSON_AddNumberToObject(response_json, "code", 404);
        cJSON_AddStringToObject(response_json, "message", "User not found");
        printf("[LOGIN] Failed - User '%s' not found\n", username);
    } else if (!db_verify_password(username, password)) {
        // Wrong password
        cJSON_AddNumberToObject(response_json, "code", 401);
        cJSON_AddStringToObject(response_json, "message", "Wrong password");
        printf("[LOGIN] Failed - Wrong password for user '%s'\n", username);
    } else if (session_is_user_logged_in(ctx->sm, user->user_id, client_sock)) {
        // User already logged in on another client (từ bài cũ)
        cJSON_AddNumberToObject(response_json, "code", 409);
        cJSON_AddStringToObject(response_json, "message", "Account is already logged in on another client");
        printf("[LOGIN] Failed - User '%s' already logged in elsewhere\n", username);
    } else {
        // Login successful
        char* token = session_create(ctx->sm, user->user_id, client_sock);
        
        if (token == NULL) {
            // Server full (no available session slots)
            cJSON_AddNumberToObject(response_json, "code", 503);
            cJSON_AddStringToObject(response_json, "message", "Server full, please try again later");
            printf("[LOGIN] Failed - No available session slots\n");
        } else {
            cJSON_AddNumberToObject(response_json, "code", 200);
            cJSON_AddStringToObject(response_json, "session_token", token);
            cJSON_AddNumberToObject(response_json, "user_id", user->user_id);
            printf("[LOGIN] User '%s' logged in successfully (ID: %d)\n", username, user->user_id);
        }
    }
    
    response = cJSON_PrintUnformatted(response_json);
    send_message(client_sock, CMD_RES_LOGIN, response);
    
    free(response);
    cJSON_Delete(response_json);
    cJSON_Delete(request);
}

// Handle LOGOUT command
void handle_logout(ServerContext* ctx, int client_sock, const char* json_data) {
    cJSON *request = cJSON_Parse(json_data);
    cJSON *response_json = NULL;
    char *response = NULL;
    
    if (request == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Invalid JSON");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_LOGOUT, response);
        free(response);
        cJSON_Delete(response_json);
        return;
    }
    
    cJSON *token_item = cJSON_GetObjectItem(request, "session_token");
    
    if (!cJSON_IsString(token_item)) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Missing session token");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_LOGOUT, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    const char *token = token_item->valuestring;
    
    // Find session to get user info before destroying
    Session* session = session_find_by_token(ctx->sm, token);
    
    if (session == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 401);
        cJSON_AddStringToObject(response_json, "message", "Invalid session");
        printf("[LOGOUT] Failed - Invalid session token\n");
    } else {
        int user_id = session->user_id;
        User* user = db_find_user_by_id(user_id);
        
        // Destroy session
        session_destroy(ctx->sm, token);
        
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 200);
        cJSON_AddStringToObject(response_json, "message", "Logout successful");
        
        if (user) {
            printf("[LOGOUT] User '%s' logged out successfully (ID: %d)\n", user->username, user_id);
        } else {
            printf("[LOGOUT] User ID %d logged out successfully\n", user_id);
        }
    }
    
    response = cJSON_PrintUnformatted(response_json);
    send_message(client_sock, CMD_RES_LOGOUT, response);
    
    free(response);
    cJSON_Delete(response_json);
    cJSON_Delete(request);
}

// Handle incoming client messages
void handle_client_message(ServerContext* ctx, int client_sock, Message* msg) {
    printf("[MESSAGE] Received command: %s\n", msg->command);
    
    if (strcmp(msg->command, CMD_REGISTER) == 0) {
        handle_register(ctx, client_sock, msg->json_data);
    } else if (strcmp(msg->command, CMD_LOGIN) == 0) {
        handle_login(ctx, client_sock, msg->json_data);
    } else if (strcmp(msg->command, CMD_LOGOUT) == 0) {
        handle_logout(ctx, client_sock, msg->json_data);
    } else {
        printf("[ERROR] Unknown command: %s\n", msg->command);
    }
}

// Thread function for handling each client
void* client_thread(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    
    Message msg;
    ServerContext ctx;
    
    ctx.sm = &sm;
    ctx.socket = client_sock;
    
    printf("[CLIENT] New client connected (socket: %d)\n", client_sock);
    
    // Handle client messages
    while (1) {
        int result = receive_message(client_sock, &msg);
        if (result <= 0) {
            printf("[CLIENT] Client disconnected (socket: %d)\n", client_sock);
            break;
        }
        
        handle_client_message(&ctx, client_sock, &msg);
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
    
    // Create socket
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
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        return 1;
    }
    
    // Listen
    if (listen(server_sock, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_sock);
        return 1;
    }
    
    printf("[SERVER] Listening on port %d\n", PORT);
    printf("[SERVER] Waiting for connections...\n\n");
    
    // Accept connections
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }
        
        // Create thread for client
        pthread_t thread;
        int* client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_sock;
        
        if (pthread_create(&thread, NULL, client_thread, client_sock_ptr) != 0) {
            perror("Thread creation failed");
            close(client_sock);
            free(client_sock_ptr);
        } else {
            pthread_detach(thread); // Detach thread
        }
    }
    
    // Cleanup
    close(server_sock);
    db_cleanup();
    return 0;
}
