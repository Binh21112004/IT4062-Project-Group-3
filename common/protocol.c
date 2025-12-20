#include "protocol.h"
#include <arpa/inet.h>

// Gửi chuỗi message qua socket
int send_message(int sock, const char* message) {
    int len = strlen(message);
    int sent = 0;
    
    while (sent < len) {
        int n = send(sock, message + sent, len - sent, 0);
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

// Nhận chuỗi message từ socket (đọc đến khi gặp \r\n)
int receive_message(int sock, char* buffer, int buffer_size) {
    char recv_buff[256];
    int bytes;
    int total_len = 0;
    
    buffer[0] = '\0';
    
    // Đọc cho đến khi gặp \r\n
    while (strstr(buffer, "\r\n") == NULL) {
        if (total_len >= buffer_size - 1) {
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
        
        int space_left = buffer_size - total_len - 1;
        if (space_left > 0) {
            strncat(buffer, recv_buff, space_left);
            total_len = strlen(buffer);
        }
    }
    
    // Loại bỏ \r\n
    char* delimiter = strstr(buffer, "\r\n");
    if (delimiter != NULL) {
        *delimiter = '\0';
    }
    
    return strlen(buffer);
}

// Parse request: COMMAND|FIELD1|FIELD2|... (cấp phát động)
char** parse_request(const char* buffer, char* command, int* field_count) {
    char temp[MAX_BUFFER];
    strncpy(temp, buffer, MAX_BUFFER - 1);
    temp[MAX_BUFFER - 1] = '\0';
    
    *field_count = 0;
    
    // Đếm số lượng fields trước
    char temp2[MAX_BUFFER];
    strcpy(temp2, temp);
    char* token = strtok(temp2, "|");
    if (token == NULL) {
        command[0] = '\0';
        return NULL;
    }
    
    int count = 0;
    while (strtok(NULL, "|") != NULL) {
        count++;
    }
    
    // Cấp phát mảng con trỏ
    char** fields = (char**)malloc(count * sizeof(char*));
    if (fields == NULL) {
        fprintf(stderr, "[ERROR] Memory allocation failed for fields array\n");
        command[0] = '\0';
        return NULL;
    }
    
    // Parse lại để lấy dữ liệu
    token = strtok(temp, "|");
    
    // Token đầu tiên là command
    strncpy(command, token, MAX_COMMAND - 1);
    command[MAX_COMMAND - 1] = '\0';
    
    // Các token còn lại là fields
    int i = 0;
    while ((token = strtok(NULL, "|")) != NULL && i < count) {
        fields[i] = (char*)malloc(strlen(token) + 1);
        if (fields[i] == NULL) {
            fprintf(stderr, "[ERROR] Memory allocation failed for field %d\n", i);
            // Free các fields đã cấp phát
            for (int j = 0; j < i; j++) {
                free(fields[j]);
            }
            free(fields);
            return NULL;
        }
        strcpy(fields[i], token);
        i++;
    }
    
    *field_count = i;
    return fields;
}

// Giải phóng mảng fields
void free_fields(char** fields, int field_count) {
    if (fields == NULL) return;
    
    for (int i = 0; i < field_count; i++) {
        if (fields[i] != NULL) {
            free(fields[i]);
        }
    }
    free(fields);
}

// Gửi request với số lượng fields tùy ý (cấp phát động)
int send_request(int sock, const char* command, const char** fields, int field_count) {
    // Tính tổng kích thước cần thiết
    int total_len = strlen(command) + 3; // command + \r\n + \0
    for (int i = 0; i < field_count; i++) {
        total_len += strlen(fields[i]) + 1; // field + |
    }
    
    // Cấp phát buffer động
    char* buffer = (char*)malloc(total_len);
    if (buffer == NULL) {
        fprintf(stderr, "[ERROR] Memory allocation failed\n");
        return -1;
    }
    
    // Build request: COMMAND|FIELD1|FIELD2|...\r\n
    int pos = 0;
    pos = snprintf(buffer, total_len, "%s", command);
    
    for (int i = 0; i < field_count && pos < total_len - 2; i++) {
        int written = snprintf(buffer + pos, total_len - pos, "|%s", fields[i]);
        if (written < 0) {
            free(buffer);
            return -1;
        }
        pos += written;
    }
    
    // Thêm \r\n
    if (pos + 2 < total_len) {
        buffer[pos++] = '\r';
        buffer[pos++] = '\n';
        buffer[pos] = '\0';
    }
    
    // Gửi
    int sent = send_message(sock, buffer);
    free(buffer);
    
    return sent;
}

// Gửi response (server)
int send_response(int sock, int code, const char* message, const char* extra_data) {
    // Tính kích thước cần thiết
    int total_len = 50 + strlen(message); // code + | + message
    if (extra_data != NULL) {
        total_len += strlen(extra_data) + 1; // + | + extra_data
    }
    total_len += 3; // + \r\n\0
    
    // Cấp phát buffer
    char* buffer = (char*)malloc(total_len);
    if (buffer == NULL) {
        fprintf(stderr, "[ERROR] Memory allocation failed\n");
        return -1;
    }
    
    // Build response
    if (extra_data != NULL && strlen(extra_data) > 0) {
        snprintf(buffer, total_len, "%d|%s|%s\r\n", code, message, extra_data);
    } else {
        snprintf(buffer, total_len, "%d|%s\r\n", code, message);
    }
    
    // Gửi
    int sent = send_message(sock, buffer);
    free(buffer);
    
    return sent;
}

// Nhận response (client) - extra_data được cấp phát động
// Format: CODE|MESSAGE|EXTRA|...\r\n
char* receive_response(int sock, int* code, char* message, int message_size) {
    char buffer[MAX_BUFFER];

    // 1) Nhận message từ socket
    int n = receive_message(sock, buffer, MAX_BUFFER);
    if (n <= 0) {
        if (code) *code = -1;
        if (message && message_size > 0) message[0] = '\0';
        return NULL;
    }
    buffer[MAX_BUFFER - 1] = '\0';
    // 2) Tìm dấu '|' thứ 1 và thứ 2
    char* p1 = strchr(buffer, '|');
    if (!p1) {
        if (code) *code = -1;
        if (message && message_size > 0) message[0] = '\0';
        return NULL;
    }

    char* p2 = strchr(p1 + 1, '|');  // có thể NULL nếu không có extra
    *p1 = '\0';
    if (code) *code = atoi(buffer);

    //Copy message (p1+1 .. p2-1) hoặc (p1+1 .. end) nếu không có p2
    if (!p2) {
        // không có extra
        if (message && message_size > 0) {
            strncpy(message, p1 + 1, message_size - 1);
            message[message_size - 1] = '\0';
        }
        return NULL;
    }

    *p2 = '\0';
    if (message && message_size > 0) {
        strncpy(message, p1 + 1, message_size - 1);
        message[message_size - 1] = '\0';
    }

    // EXTRA là toàn bộ phần còn lại sau p2
    const char* extra_str = p2 + 1;

    // có thể server gửi extra rỗng
    if (!extra_str || extra_str[0] == '\0') {
        return NULL;
    }

    //Cấp phát động extra_data
    char* extra_data = (char*)malloc(strlen(extra_str) + 1);
    if (!extra_data) {
        fprintf(stderr, "[ERROR] Memory allocation failed for extra_data\n");
        return NULL;
    }

    strcpy(extra_data, extra_str);
    return extra_data; 
}
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

// thread-local request: mỗi thread client giữ request riêng
static __thread const char* tls_current_request = NULL;

void protocol_set_current_request_for_log(const char* request_line) {
    tls_current_request = request_line;
}

static void sanitize_for_log(char* s) {
    if (!s) return;
    for (char* p = s; *p; p++) {
        if (*p == '\n' || *p == '\r') *p = ' ';
        if (*p == '$') *p = '_';
    }
}

static void get_client_ip_str(int sock, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return;

    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    out[0] = '\0';

    if (getpeername(sock, (struct sockaddr*)&addr, &len) != 0) {
        snprintf(out, out_sz, "unknown");
        return;
    }

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* a = (struct sockaddr_in*)&addr;
        inet_ntop(AF_INET, &a->sin_addr, out, out_sz);
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6* a6 = (struct sockaddr_in6*)&addr;
        inet_ntop(AF_INET6, &a6->sin6_addr, out, out_sz);
    } else {
        snprintf(out, out_sz, "unknown");
    }
}

static void write_activity_log_line(int client_sock, const char* request, const char* result) {
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%d/%m/%Y %H:%M:%S", &tm_now);

    char ip[64];
    get_client_ip_str(client_sock, ip, sizeof(ip));

    char req_buf[512];
    char res_buf[512];
    snprintf(req_buf, sizeof(req_buf), "%s", request ? request : "");
    snprintf(res_buf, sizeof(res_buf), "%s", result ? result : "");

    sanitize_for_log(req_buf);
    sanitize_for_log(res_buf);

    pthread_mutex_lock(&g_log_mutex);
    FILE* f = fopen(LOG_FILE_NAME, "a");
    if (f) {
        fprintf(f, "[%s]$%s$%s$%s\n", time_buf, ip, req_buf, res_buf);
        fclose(f);
    }
    pthread_mutex_unlock(&g_log_mutex);
}

// Wrapper: gửi response rồi log
int send_response_with_log(int client_sock, int code, const char* message, const char* extra_data) {
    int sent = send_response(client_sock, code, message, extra_data);

    char result[768];
    if (extra_data && extra_data[0] != '\0') {
        snprintf(result, sizeof(result), "%d|%s|%s", code, message ? message : "", extra_data);
    } else {
        snprintf(result, sizeof(result), "%d|%s", code, message ? message : "");
    }

    write_activity_log_line(client_sock, tls_current_request, result);
    return sent;
}
