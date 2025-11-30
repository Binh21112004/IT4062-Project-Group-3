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
    char fields[3][MAX_FIELD];
    Response res;
    
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
    
    // Prepare fields: REGISTER|username|password|email
    strncpy(fields[0], username, MAX_FIELD - 1);
    strncpy(fields[1], password, MAX_FIELD - 1);
    strncpy(fields[2], email, MAX_FIELD - 1);
    
  
    send_request(client_sock, CMD_REGISTER, fields, 3);
    
    
    if (receive_response(client_sock, &res) > 0) {
        if (res.code == RESPONSE_OK) {
            printf("\n[SUCCESS] %s\n", res.message);
        } else {
            printf("\n[ERROR] %s (Code: %d)\n", res.message, res.code);
        }
    }
}

void do_login() {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char fields[2][MAX_FIELD];
    Response res;
    
    printf("Username: ");
    fflush(stdout);
    if (fgets(username, sizeof(username), stdin) == NULL) return;
    username[strcspn(username, "\n")] = 0;
    
    printf("Password: ");
    fflush(stdout);
    if (fgets(password, sizeof(password), stdin) == NULL) return;
    password[strcspn(password, "\n")] = 0;
    
    // Prepare fields: LOGIN|username|password
    strncpy(fields[0], username, MAX_FIELD - 1);
    strncpy(fields[1], password, MAX_FIELD - 1);
    

    send_request(client_sock, CMD_LOGIN, fields, 2);
    

    if (receive_response(client_sock, &res) > 0) {
        if (res.code == RESPONSE_OK) {
            // Save session_id from extra_data
            if (strlen(res.extra_data) > 0) {
                strncpy(session_id, res.extra_data, MAX_SESSION_ID - 1);
                session_id[MAX_SESSION_ID - 1] = '\0';
                printf("\n[SUCCESS] %s\n", res.message);
                printf("Session ID: %s\n", session_id);
            }
        } else {
            printf("\n[ERROR] %s (Code: %d)\n", res.message, res.code);
        }
    }
}

void do_logout() {
    if (strlen(session_id) == 0) {
        printf("\n[ERROR] You are not logged in!\n");
        return;
    }
    
    char fields[1][MAX_FIELD];
    Response res;
    
    // Prepare fields: LOGOUT|session_id
    strncpy(fields[0], session_id, MAX_FIELD - 1);
    

    send_request(client_sock, CMD_LOGOUT, fields, 1);
    
  
    if (receive_response(client_sock, &res) > 0) {
        if (res.code == RESPONSE_OK) {
            // Clear session_id
            memset(session_id, 0, sizeof(session_id));
            printf("\n[SUCCESS] %s\n", res.message);
        } else {
            printf("\n[ERROR] %s (Code: %d)\n", res.message, res.code);
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
