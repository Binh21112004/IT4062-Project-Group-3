#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/protocol.h"
#include "../common/cJSON.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888

int client_sock;
char session_token[MAX_TOKEN] = {0};
int user_id = 0;
int running = 1;

// Thread for receiving messages from server
void* receive_thread(void* arg) {
    (void)arg;
    Message msg;
    
    while (running) {
        int result = receive_message(client_sock, &msg);
        if (result <= 0) {
            printf("\n[ERROR] Disconnected from server\n");
            running = 0;
            break;
        }
        
        // Handle notifications
        if (strcmp(msg.command, CMD_FRIEND_INVITE_NOTIFICATION) == 0) {
            printf("\n=== NEW FRIEND REQUEST ===\n");
            cJSON *json = cJSON_Parse(msg.json_data);
            if (json) {
                cJSON *from_user_item = cJSON_GetObjectItem(json, "from_user");
                cJSON *request_id_item = cJSON_GetObjectItem(json, "request_id");
                if (from_user_item && request_id_item) {
                    printf("From: %s\n", from_user_item->valuestring);
                    printf("Request ID: %d\n", (int)request_id_item->valuedouble);
                }
                cJSON_Delete(json);
            }
            printf("========================\n");
            printf("> ");
            fflush(stdout);
        } else if (strcmp(msg.command, CMD_FRIEND_ACCEPTED_NOTIFICATION) == 0) {
            printf("\n=== FRIEND REQUEST ACCEPTED ===\n");
            cJSON *json = cJSON_Parse(msg.json_data);
            if (json) {
                cJSON *from_user_item = cJSON_GetObjectItem(json, "from_user");
                if (from_user_item) {
                    printf("User '%s' accepted your friend request!\n", from_user_item->valuestring);
                }
                cJSON_Delete(json);
            }
            printf("==============================\n");
            printf("> ");
            fflush(stdout);
        } else if (strcmp(msg.command, CMD_FRIEND_REMOVED_NOTIFICATION) == 0) {
            printf("\n=== FRIENDSHIP REMOVED ===\n");
            cJSON *json = cJSON_Parse(msg.json_data);
            if (json) {
                cJSON *from_user_item = cJSON_GetObjectItem(json, "from_user");
                if (from_user_item) {
                    printf("User '%s' removed you as a friend\n", from_user_item->valuestring);
                }
                cJSON_Delete(json);
            }
            printf("========================\n");
            printf("> ");
            fflush(stdout);
        } else {
            // Regular responses - will be handled in main thread
            continue;
        }
    }
    
    return 0;
}

void print_menu() {
    printf("\n=== MENU ===\n");
    printf("1. Register\n");
    printf("2. Login\n");
    printf("3. Send friend request\n");
    printf("4. Accept friend request\n");
    printf("5. Reject friend request\n");
    printf("6. Remove friend\n");
    printf("0. Exit\n");
    printf("============\n");
}

void do_register() {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char email[MAX_EMAIL];
    Message msg;
    
    printf("Username: ");
    fflush(stdout);
    if (scanf("%49s", username) != 1) return;
    while (getchar() != '\n');  // Clear input buffer
    
    printf("Password: ");
    fflush(stdout);
    if (scanf("%49s", password) != 1) return;
    while (getchar() != '\n');
    
    printf("Email: ");
    fflush(stdout);
    if (scanf("%99s", email) != 1) return;
    while (getchar() != '\n');
    
    // Build JSON
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "username", username);
    cJSON_AddStringToObject(req, "password", password);
    cJSON_AddStringToObject(req, "email", email);
    char *json_data = cJSON_PrintUnformatted(req);
    
    // Send request
    send_message(client_sock, CMD_REGISTER, json_data);
    
    // Cleanup request
    free(json_data);
    cJSON_Delete(req);
    
    // Wait for response
    if (receive_message(client_sock, &msg) > 0) {
        cJSON *json = cJSON_Parse(msg.json_data);
        if (json) {
            cJSON *code_item = cJSON_GetObjectItem(json, "code");
            cJSON *message_item = cJSON_GetObjectItem(json, "message");
            
            if (code_item && message_item) {
                int code = (int)code_item->valuedouble;
                if (code == 201) {
                    printf("\n[SUCCESS] %s\n", message_item->valuestring);
                } else {
                    printf("\n[ERROR] %s (Code: %d)\n", message_item->valuestring, code);
                }
            }
            cJSON_Delete(json);
        }
    }
}

void do_login() {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    Message msg;
    
    printf("Username: ");
    fflush(stdout);
    if (scanf("%49s", username) != 1) return;
    while (getchar() != '\n');
    
    printf("Password: ");
    fflush(stdout);
    if (scanf("%49s", password) != 1) return;
    while (getchar() != '\n');
    
    // Build JSON
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "username", username);
    cJSON_AddStringToObject(req, "password", password);
    char *json_data = cJSON_PrintUnformatted(req);
    
    // Send request
    send_message(client_sock, CMD_LOGIN, json_data);
    
    // Cleanup request
    free(json_data);
    cJSON_Delete(req);
    
    // Wait for response
    if (receive_message(client_sock, &msg) > 0) {
        cJSON *json = cJSON_Parse(msg.json_data);
        if (json) {
            cJSON *code_item = cJSON_GetObjectItem(json, "code");
            
            if (code_item) {
                int code = (int)code_item->valuedouble;
                if (code == 200) {
                    cJSON *token_item = cJSON_GetObjectItem(json, "session_token");
                    cJSON *user_id_item = cJSON_GetObjectItem(json, "user_id");
                    if (token_item && user_id_item) {
                        strcpy(session_token, token_item->valuestring);
                        user_id = (int)user_id_item->valuedouble;
                        printf("\n[SUCCESS] Logged in successfully!\n");
                        printf("User ID: %d\n", user_id);
                    }
                } else {
                    cJSON *message_item = cJSON_GetObjectItem(json, "message");
                    if (message_item) {
                        printf("\n[ERROR] %s (Code: %d)\n", message_item->valuestring, code);
                    }
                }
            }
            cJSON_Delete(json);
        }
    }
}

void do_send_friend_request() {
    if (strlen(session_token) == 0) {
        printf("\n[ERROR] Please login first!\n");
        return;
    }
    
    char target_username[MAX_USERNAME];
    Message msg;
    
    printf("Target username: ");
    fflush(stdout);
    if (scanf("%49s", target_username) != 1) return;
    while (getchar() != '\n');
    
    // Build JSON
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "session_token", session_token);
    cJSON_AddStringToObject(req, "target_user", target_username);
    char *json_data = cJSON_PrintUnformatted(req);
    
    // Send request
    send_message(client_sock, CMD_FRIEND_INVITE, json_data);
    
    // Cleanup request
    free(json_data);
    cJSON_Delete(req);
    
    // Wait for response
    if (receive_message(client_sock, &msg) > 0) {
        cJSON *json = cJSON_Parse(msg.json_data);
        if (json) {
            cJSON *code_item = cJSON_GetObjectItem(json, "code");
            cJSON *message_item = cJSON_GetObjectItem(json, "message");
            
            if (code_item && message_item) {
                int code = (int)code_item->valuedouble;
                if (code == 111) {
                    cJSON *request_id_item = cJSON_GetObjectItem(json, "request_id");
                    if (request_id_item) {
                        printf("\n[SUCCESS] %s (Request ID: %d)\n", message_item->valuestring, (int)request_id_item->valuedouble);
                    }
                } else {
                    printf("\n[ERROR] %s (Code: %d)\n", message_item->valuestring, code);
                }
            }
            cJSON_Delete(json);
        }
    }
}

void do_accept_friend_request() {
    if (strlen(session_token) == 0) {
        printf("\n[ERROR] Please login first!\n");
        return;
    }
    
    int request_id;
    Message msg;
    
    printf("Request ID: ");
    fflush(stdout);
    if (scanf("%d", &request_id) != 1) return;
    while (getchar() != '\n');
    
    // Build JSON
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "session_token", session_token);
    cJSON_AddNumberToObject(req, "request_id", request_id);
    cJSON_AddBoolToObject(req, "accept", 1);
    char *json_data = cJSON_PrintUnformatted(req);
    
    // Send request
    send_message(client_sock, CMD_FRIEND_RESPONSE, json_data);
    
    // Cleanup request
    free(json_data);
    cJSON_Delete(req);
    
    // Wait for response
    if (receive_message(client_sock, &msg) > 0) {
        cJSON *json = cJSON_Parse(msg.json_data);
        if (json) {
            cJSON *code_item = cJSON_GetObjectItem(json, "code");
            cJSON *message_item = cJSON_GetObjectItem(json, "message");
            
            if (code_item && message_item) {
                int code = (int)code_item->valuedouble;
                if (code == 112) {
                    printf("\n[SUCCESS] %s\n", message_item->valuestring);
                } else {
                    printf("\n[ERROR] %s (Code: %d)\n", message_item->valuestring, code);
                }
            }
            cJSON_Delete(json);
        }
    }
}

void do_reject_friend_request() {
    if (strlen(session_token) == 0) {
        printf("\n[ERROR] Please login first!\n");
        return;
    }
    
    int request_id;
    Message msg;
    
    printf("Request ID: ");
    fflush(stdout);
    if (scanf("%d", &request_id) != 1) return;
    while (getchar() != '\n');
    
    // Build JSON
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "session_token", session_token);
    cJSON_AddNumberToObject(req, "request_id", request_id);
    cJSON_AddBoolToObject(req, "accept", 0);
    char *json_data = cJSON_PrintUnformatted(req);
    
    // Send request
    send_message(client_sock, CMD_FRIEND_RESPONSE, json_data);
    
    // Cleanup request
    free(json_data);
    cJSON_Delete(req);
    
    // Wait for response
    if (receive_message(client_sock, &msg) > 0) {
        cJSON *json = cJSON_Parse(msg.json_data);
        if (json) {
            cJSON *code_item = cJSON_GetObjectItem(json, "code");
            cJSON *message_item = cJSON_GetObjectItem(json, "message");
            
            if (code_item && message_item) {
                int code = (int)code_item->valuedouble;
                if (code == 113) {
                    printf("\n[SUCCESS] %s\n", message_item->valuestring);
                } else {
                    printf("\n[ERROR] %s (Code: %d)\n", message_item->valuestring, code);
                }
            }
            cJSON_Delete(json);
        }
    }
}

void do_remove_friend() {
    if (strlen(session_token) == 0) {
        printf("\n[ERROR] Please login first!\n");
        return;
    }
    
    char friend_username[MAX_USERNAME];
    Message msg;
    
    printf("Friend username: ");
    fflush(stdout);
    if (scanf("%49s", friend_username) != 1) return;
    while (getchar() != '\n');
    
    // Build JSON
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "session_token", session_token);
    cJSON_AddStringToObject(req, "friend_username", friend_username);
    char *json_data = cJSON_PrintUnformatted(req);
    
    // Send request
    send_message(client_sock, CMD_FRIEND_REMOVE, json_data);
    
    // Cleanup request
    free(json_data);
    cJSON_Delete(req);
    
    // Wait for response
    if (receive_message(client_sock, &msg) > 0) {
        cJSON *json = cJSON_Parse(msg.json_data);
        if (json) {
            cJSON *code_item = cJSON_GetObjectItem(json, "code");
            cJSON *message_item = cJSON_GetObjectItem(json, "message");
            
            if (code_item && message_item) {
                int code = (int)code_item->valuedouble;
                if (code == 113) {
                    printf("\n[SUCCESS] %s\n", message_item->valuestring);
                } else {
                    printf("\n[ERROR] %s (Code: %d)\n", message_item->valuestring, code);
                }
            }
            cJSON_Delete(json);
        }
    }
}

int main() {
    struct sockaddr_in server_addr;
    
    printf("=== TCP Socket Client ===\n");
    
    // Create socket
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
    // Connect to server
    printf("Connecting to server %s:%d...\n", SERVER_IP, SERVER_PORT);
    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_sock);
        return 1;
    }
    
    printf("Connected successfully!\n");
    
    // Start receive thread
    pthread_t thread;
    if (pthread_create(&thread, NULL, receive_thread, NULL) != 0) {
        perror("Thread creation failed");
        close(client_sock);
        return 1;
    }
    pthread_detach(thread);
    
    // Main loop
    int choice;
    while (running) {
        print_menu();
        printf("> ");
        fflush(stdout);
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');  // Clear invalid input
            printf("Invalid input!\n");
            continue;
        }
        while (getchar() != '\n');  // Clear input buffer
        
        switch (choice) {
            case 1:
                do_register();
                break;
            case 2:
                do_login();
                break;
            case 3:
                do_send_friend_request();
                break;
            case 4:
                do_accept_friend_request();
                break;
            case 5:
                do_reject_friend_request();
                break;
            case 6:
                do_remove_friend();
                break;
            case 0:
                running = 0;
                printf("Goodbye!\n");
                break;
            default:
                printf("Invalid choice!\n");
        }
    }
    
    // Cleanup
    close(client_sock);
    
    return 0;
}
