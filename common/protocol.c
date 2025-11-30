#include "protocol.h"
#include <arpa/inet.h>

// Send request from client to server: COMMAND|FIELD1|FIELD2|...
int send_request(int sock, const char* command, char fields[][MAX_FIELD], int field_count) {
    char buffer[MAX_BUFFER];
    int pos = 0;
    
    // Add command
    pos += snprintf(buffer + pos, MAX_BUFFER - pos, "%s", command);
    
    // Add fields
    for (int i = 0; i < field_count && pos < MAX_BUFFER - 2; i++) {
        pos += snprintf(buffer + pos, MAX_BUFFER - pos, "|%s", fields[i]);
    }
    
    // Add delimiter
    pos += snprintf(buffer + pos, MAX_BUFFER - pos, "\r\n");
    
    // Send
    int sent = 0;
    while (sent < pos) {
        int n = send(sock, buffer + sent, pos - sent, 0);
        if (n <= 0) {
            if (n < 0) {
                perror("[ERROR] send failed");
            }
            return -1;
        }
        sent += n;
    }
    
    return sent;
}

// Receive request on server side
int receive_request(int sock, Request* req) {
    char buffer[MAX_BUFFER];
    char recv_buff[256];
    int bytes;
    int total_len = 0;
    
    buffer[0] = '\0';
    
    // Receive until \r\n delimiter is found
    while (strstr(buffer, "\r\n") == NULL) {
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
    
    // Parse fields separated by |
    req->field_count = 0;
    char* token = strtok(buffer, "|");
    
    if (token == NULL) {
        fprintf(stderr, "[ERROR] Invalid request format\n");
        return -1;
    }
    
    // First token is command
    strncpy(req->command, token, MAX_COMMAND - 1);
    req->command[MAX_COMMAND - 1] = '\0';
    
    // Remaining tokens are fields
    while ((token = strtok(NULL, "|")) != NULL && req->field_count < MAX_FIELDS) {
        strncpy(req->fields[req->field_count], token, MAX_FIELD - 1);
        req->fields[req->field_count][MAX_FIELD - 1] = '\0';
        req->field_count++;
    }
    
    return total_len;
}

// Send response from server to client: CODE|MESSAGE or CODE|MESSAGE|EXTRA_DATA
int send_response(int sock, int code, const char* message, const char* extra_data) {
    char buffer[MAX_BUFFER];
    int pos = 0;
    
    if (extra_data != NULL && strlen(extra_data) > 0) {
        pos = snprintf(buffer, MAX_BUFFER, "%d|%s|%s\r\n", code, message, extra_data);
    } else {
        pos = snprintf(buffer, MAX_BUFFER, "%d|%s\r\n", code, message);
    }
    
    // Send
    int sent = 0;
    while (sent < pos) {
        int n = send(sock, buffer + sent, pos - sent, 0);
        if (n <= 0) {
            if (n < 0) {
                perror("[ERROR] send failed");
            }
            return -1;
        }
        sent += n;
    }
    
    return sent;
}

// Receive response on client side
int receive_response(int sock, Response* res) {
    char buffer[MAX_BUFFER];
    char recv_buff[256];
    int bytes;
    int total_len = 0;
    
    buffer[0] = '\0';
    
    // Receive until \r\n delimiter is found
    while (strstr(buffer, "\r\n") == NULL) {
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
    
    // Parse: CODE|MESSAGE or CODE|MESSAGE|EXTRA_DATA
    char* code_str = strtok(buffer, "|");
    char* message_str = strtok(NULL, "|");
    char* extra_str = strtok(NULL, "|");
    
    if (code_str == NULL || message_str == NULL) {
        fprintf(stderr, "[ERROR] Invalid response format\n");
        return -1;
    }
    
    res->code = atoi(code_str);
    strncpy(res->message, message_str, MAX_BUFFER - 1);
    res->message[MAX_BUFFER - 1] = '\0';
    
    if (extra_str != NULL) {
        strncpy(res->extra_data, extra_str, MAX_BUFFER - 1);
        res->extra_data[MAX_BUFFER - 1] = '\0';
    } else {
        res->extra_data[0] = '\0';
    }
    
    return total_len;
}
