#include "protocol.h"
#include <arpa/inet.h>

// Send message with format: COMMAND_TYPE | JSON_DATA
int send_message(int sock, const char* command, const char* json_data) {
    char buffer[MAX_BUFFER];
    int total_len;
    
    // Format: COMMAND_TYPE | JSON_DATA\r\n
    snprintf(buffer, MAX_BUFFER, "%s|%s\r\n", command, json_data);
    total_len = strlen(buffer);
    
    // Send message with \r\n delimiter
    int sent = 0;
    while (sent < total_len) {
        int n = send(sock, buffer + sent, total_len - sent, 0);
        if (n <= 0) {
            if (n < 0) {
                perror("[ERROR] send failed");
            }
            return -1;
        }
        sent += n;
    }
    
    return total_len;
}

// Receive message and parse into command and json_data
int receive_message(int sock, Message* msg) {
    char buffer[MAX_BUFFER];
    char recv_buff[256];
    int bytes;
    int total_len = 0;
    
    buffer[0] = '\0';
    
    // Receive until \r\n delimiter is found
    while (strstr(buffer, "\r\n") == NULL) {
        // Check buffer overflow
        if (total_len >= MAX_BUFFER - 1) {
            fprintf(stderr, "[ERROR] Buffer overflow\n");
            return -1;
        }
        
        bytes = recv(sock, recv_buff, sizeof(recv_buff) - 1, 0);
        
        if (bytes <= 0) {
            if (bytes < 0) {
                perror("[ERROR] recv failed");
            }
            return bytes;
        }
        
        recv_buff[bytes] = '\0';
        
        int space_left = MAX_BUFFER - total_len - 1;
        if (space_left > 0) {
            strncat(buffer, recv_buff, space_left);
            total_len = strlen(buffer);
        }
    }
    
    // Remove \r\n delimiter
    char* delimiter = strstr(buffer, "\r\n");
    if (delimiter != NULL) {
        *delimiter = '\0';
    }
    
    // Parse: COMMAND_TYPE | JSON_DATA
    delimiter = strchr(buffer, '|');
    if (delimiter == NULL) {
        fprintf(stderr, "[ERROR] Invalid message format (no | delimiter)\n");
        return -1;
    }
    
    // Extract command
    int cmd_len = delimiter - buffer;
    strncpy(msg->command, buffer, cmd_len);
    msg->command[cmd_len] = '\0';
    
    // Extract JSON data
    strcpy(msg->json_data, delimiter + 1);
    
    return strlen(buffer);
}
