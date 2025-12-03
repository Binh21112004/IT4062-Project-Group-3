#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/protocol.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888

int client_sock;
char session_id[MAX_SESSION_ID] = {0};
int running = 1;

void print_menu() {
    printf("\n=== MENU ===\n");
    printf("1. Register\n");
    printf("2. Login\n");
    
    if (strlen(session_id) > 0) {
        printf("3. Logout\n");
    }
    
    printf("0. Exit\n");
    printf("============\n");
}

void do_register() {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char email[MAX_EMAIL];
    
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
    
    // Gửi request
    const char* fields[] = {username, password, email};
    send_request(client_sock, CMD_REGISTER, fields, 3);
    
    // Nhận response
    int code;
    char message[MAX_BUFFER];
    char* extra_data = receive_response(client_sock, &code, message, MAX_BUFFER);
    
    if (code == RESPONSE_OK) {
        printf("\n[SUCCESS] %s\n", message);
    } else {
        printf("\n[ERROR] %s (Code: %d)\n", message, code);
    }
    
    if (extra_data != NULL) {
        free(extra_data);
    }
}

void do_login() {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    
    printf("Username: ");
    fflush(stdout);
    if (fgets(username, sizeof(username), stdin) == NULL) return;
    username[strcspn(username, "\n")] = 0;
    
    printf("Password: ");
    fflush(stdout);
    if (fgets(password, sizeof(password), stdin) == NULL) return;
    password[strcspn(password, "\n")] = 0;
    
    // Gửi request
    const char* fields[] = {username, password};
    send_request(client_sock, CMD_LOGIN, fields, 2);
    
    // Nhận response
    int code;
    char message[MAX_BUFFER];
    char* extra_data = receive_response(client_sock, &code, message, MAX_BUFFER);
    
    if (code == RESPONSE_OK) {
        // Lưu session_id từ extra_data
        if (extra_data != NULL) {
            strncpy(session_id, extra_data, MAX_SESSION_ID - 1);
            session_id[MAX_SESSION_ID - 1] = '\0';
            printf("\n[SUCCESS] %s\n", message);
            printf("Session ID: %s\n", session_id);
        }
    } else {
        printf("\n[ERROR] %s (Code: %d)\n", message, code);
    }
    
    if (extra_data != NULL) {
        free(extra_data);
    }
}

void do_logout() {
    if (strlen(session_id) == 0) {
        printf("\n[ERROR] You are not logged in!\n");
        return;
    }
    
    // Gửi request
    const char* fields[] = {session_id};
    send_request(client_sock, CMD_LOGOUT, fields, 1);
    
    // Nhận response
    int code;
    char message[MAX_BUFFER];
    char* extra_data = receive_response(client_sock, &code, message, MAX_BUFFER);
    
    if (code == RESPONSE_OK) {
        // Xóa session_id
        memset(session_id, 0, sizeof(session_id));
        printf("\n[SUCCESS] %s\n", message);
    } else {
        printf("\n[ERROR] %s (Code: %d)\n", message, code);
    }
    
    if (extra_data != NULL) {
        free(extra_data);
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
