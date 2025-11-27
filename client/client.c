#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

void print_menu() {
    printf("\n=== MENU ===\n");
    printf("1. Register\n");
    printf("2. Login\n");
    
   
    if (strlen(session_token) > 0) {
        printf("3. Logout\n");
    }
    
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
    if (fgets(username, sizeof(username), stdin) == NULL) return;
    username[strcspn(username, "\n")] = 0;  
    
    printf("Password: ");
    fflush(stdout);
    if (fgets(password, sizeof(password), stdin) == NULL) return;
    password[strcspn(password, "\n")] = 0;  
    
    printf("Email: ");
    fflush(stdout);
    if (fgets(email, sizeof(email), stdin) == NULL) return;
    email[strcspn(email, "\n")] = 0;  
    
    // Build JSON
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "username", username);
    cJSON_AddStringToObject(req, "password", password);
    cJSON_AddStringToObject(req, "email", email);
    char *json_data = cJSON_PrintUnformatted(req);
    
    
    send_message(client_sock, CMD_REGISTER, json_data);
    
 
    free(json_data);
    cJSON_Delete(req);
    
  
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
    if (fgets(username, sizeof(username), stdin) == NULL) return;
    username[strcspn(username, "\n")] = 0; 
    
    printf("Password: ");
    fflush(stdout);
    if (fgets(password, sizeof(password), stdin) == NULL) return;
    password[strcspn(password, "\n")] = 0;  
    
    
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "username", username);
    cJSON_AddStringToObject(req, "password", password);
    char *json_data = cJSON_PrintUnformatted(req);
    
 
    send_message(client_sock, CMD_LOGIN, json_data);
    
    
    free(json_data);
    cJSON_Delete(req);
    
    
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

void do_logout() {
    if (strlen(session_token) == 0) {
        printf("\n[ERROR] You are not logged in!\n");
        return;
    }
    
    Message msg;
    

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "session_token", session_token);
    char *json_data = cJSON_PrintUnformatted(req);
    

    send_message(client_sock, CMD_LOGOUT, json_data);
    
   
    free(json_data);
    cJSON_Delete(req);
    
  
    if (receive_message(client_sock, &msg) > 0) {
        cJSON *json = cJSON_Parse(msg.json_data);
        if (json) {
            cJSON *code_item = cJSON_GetObjectItem(json, "code");
            cJSON *message_item = cJSON_GetObjectItem(json, "message");
            
            if (code_item) {
                int code = (int)code_item->valuedouble;
                if (code == 200) {
                    
                    memset(session_token, 0, sizeof(session_token));
                    user_id = 0;
                    
                    if (message_item) {
                        printf("\n[SUCCESS] %s\n", message_item->valuestring);
                    } else {
                        printf("\n[SUCCESS] Logged out successfully!\n");
                    }
                } else {
                    if (message_item) {
                        printf("\n[ERROR] %s (Code: %d)\n", message_item->valuestring, code);
                    }
                }
            }
            cJSON_Delete(json);
        }
    }
}

int main() {
    struct sockaddr_in server_addr;
    
    printf("=== TCP Socket Client ===\n");
    
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
   
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
  
    printf("Connecting to server %s:%d...\n", SERVER_IP, SERVER_PORT);
    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_sock);
        return 1;
    }
    
    printf("Connected successfully!\n");
    
    int choice;
    while (running) {
        print_menu();
        printf("> ");
        fflush(stdout);
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');  
            printf("Invalid input!\n");
            continue;
        }
        while (getchar() != '\n');  
        
        switch (choice) {
            case 1:
                do_register();
                break;
            case 2:
                do_login();
                break;
            case 3:
                do_logout();
                break;
            case 0:
                running = 0;
                printf("Goodbye!\n");
                break;
            default:
                printf("Invalid choice!\n");
        }
    }
    
   
    close(client_sock);
    
    return 0;
}
