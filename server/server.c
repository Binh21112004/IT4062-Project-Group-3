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
