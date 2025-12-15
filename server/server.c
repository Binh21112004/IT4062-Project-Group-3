#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
    // Check if already logged in
    Session* existing_session = session_find_by_socket(ctx->sm, client_sock);
    if (existing_session != NULL && existing_session->is_active) {
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Already logged in. Please logout first", NULL);
        printf("[REGISTER] Failed - Client socket %d is already logged in\n", client_sock);
        return;
    }
    
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
    // Check if already logged in
    Session* existing_session = session_find_by_socket(ctx->sm, client_sock);
    if (existing_session != NULL && existing_session->is_active) {
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Already logged in. Please logout first", NULL);
        printf("[LOGIN] Failed - Client socket %d is already logged in\n", client_sock);
        return;
    }
    
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

static int is_blank_s(const char* s) {
    if (!s) return 1;
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int is_valid_datetime_s(const char* s) {
    if (!s) return 0;
    int Y,M,D,h,m,sec;
    char extra;
    if (sscanf(s, "%4d-%2d-%2d %2d:%2d:%2d%c", &Y,&M,&D,&h,&m,&sec,&extra) != 6)
        return 0;
    if (Y < 1970 || Y > 2100) return 0;
    if (M < 1 || M > 12) return 0;
    if (D < 1 || D > 31) return 0;
    if (h < 0 || h > 23) return 0;
    if (m < 0 || m > 59) return 0;
    if (sec < 0 || sec > 59) return 0;
    return 1;
}

// CREATE_EVENT|session_id|event_name|event_date|event_location|event_type|event_description
void handle_create_event(ServerContext* ctx, int client_sock, char** fields, int field_count) {
    // 1) Validate request format : 400
    if (field_count != 6 || !fields) {
        send_response(client_sock, RESPONSE_BAD_REQUEST,
                      "Invalid request. Usage: CREATE_EVENT|session_id|name|datetime|location|type|desc",
                      NULL);
        printf("[CREATE_EVENT] Failed - Bad request (field_count=%d)\n", field_count);
        return;
    }

    const char* session_token     = fields[0];
    const char* event_name        = fields[1];
    const char* event_date        = fields[2];
    const char* event_location    = fields[3];
    const char* event_type        = fields[4];
    const char* event_description = fields[5];

    // 2) Validate session : 401
    Session* session = session_find_by_token(ctx->sm, session_token);
    if (session == NULL || !session->is_active) {
        send_response(client_sock, RESPONSE_UNAUTHORIZED,"Invalid or expired session. Please login again.",NULL);
        printf("[CREATE_EVENT] Failed - Invalid session\n");
        return;
    }

    int user_id = session->user_id;

    // 3) Validate fields : 400 
    if (is_blank_s(event_name)) {
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Event name is required.", NULL);
        printf("[CREATE_EVENT] Failed - Missing event_name\n");
        return;
    }

    // ít nhất phải nhập ngày tháng :event_date bắt buộc
    if (is_blank_s(event_date)) {
        send_response(client_sock, RESPONSE_BAD_REQUEST,"Event date/time is required. Use format: YYYY-MM-DD HH:MM:SS",NULL);
        printf("[CREATE_EVENT] Failed - Missing event_date\n");
        return;
    }

    if (!is_valid_datetime_s(event_date)) {
        send_response(client_sock, RESPONSE_BAD_REQUEST,"Invalid date/time. Use format: YYYY-MM-DD HH:MM:SS (vd: 2025-12-25 18:00:00)",NULL);
        printf("[CREATE_EVENT] Failed - Invalid datetime: '%s'\n", event_date);
        return;
    }

    if (is_blank_s(event_location)) {
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Location is required.", NULL);
        printf("[CREATE_EVENT] Failed - Missing location\n");
        return;
    }

    if (strcmp(event_type, "public") != 0 && strcmp(event_type, "private") != 0) {
        send_response(client_sock, RESPONSE_BAD_REQUEST,"Invalid event type. Must be 'public' or 'private'.",NULL);
        printf("[CREATE_EVENT] Failed - Invalid event_type: '%s'\n", event_type);
        return;
    }

    // 4) DB create -> nếu lỗi DB thật thì 500
    int event_id = db_create_event(user_id, event_name, event_description, event_location, event_date, event_type);
    if (event_id < 0) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Database error while creating event.", NULL);
        printf("[CREATE_EVENT] Failed - DB error\n");
        return;
    }

    // 5) Success : 200 +event_id
    char extra[64];
    snprintf(extra, sizeof(extra), "%d", event_id);

    send_response(client_sock, RESPONSE_OK, "Event created successfully", extra);
    printf("[CREATE_EVENT] Success - user %d created event %d\n", user_id, event_id);
}


// GET_EVENTS|session_id
void handle_get_events(ServerContext* ctx, int client_sock, char** fields, int field_count) {
    if (field_count != 1) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }

    const char* session_token = fields[0];

    // 1. Kiểm tra session
    Session* session = session_find_by_token(ctx->sm, session_token);
    if (session == NULL || !session->is_active) {
        send_response(client_sock, RESPONSE_UNAUTHORIZED, "Invalid session ID", NULL);
        printf("[GET_EVENTS] Failed - Invalid session ID\n");
        return;
    }

    int user_id = session->user_id;

    // 2. Lấy danh sách events từ DB
    char** results = NULL;
    int count = 0;

    if (db_get_user_events(user_id, &results, &count) < 0) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        printf("[GET_EVENTS] Failed - DB error\n");
        return;
    }

    if (count == 0) {
        send_response(client_sock, RESPONSE_OK, "Event list retrieved successfully", ""); // extra rỗng
        printf("[GET_EVENTS] Success - user %d has no events\n", user_id);
        return;
    }

    // 3. Gộp thành 1 chuỗi <event_list>
    // mỗi event 1 dòng, mỗi dòng: event_id;title;location;time;type;status
    size_t buf_size = 0;
    for (int i = 0; i < count; i++) {
        buf_size += strlen(results[i]) + 1; // + '\n'
    }

    char* buffer = (char*)malloc(buf_size + 1);
    if (!buffer) {
        db_free_results(&results, count);
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }

    buffer[0] = '\0';
    for (int i = 0; i < count; i++) {
        strcat(buffer, results[i]);
        if (i < count - 1) strcat(buffer, "\n");
    }

    db_free_results(&results, count);

    send_response(client_sock, RESPONSE_OK, "Event list retrieved successfully", buffer);
    printf("[GET_EVENTS] Success - user %d has %d events\n", user_id, count);

    free(buffer);
}
// GET_EVENT_DETAIL|session_id|event_id
// Hàm này trả về chi tiết sự kiện theo event_id và user_id .( Dùng để gửi tới client khi client sửa sự kiện)
void handle_get_event_detail(ServerContext* ctx, int client_sock, char** fields, int field_count){
    if (field_count != 2) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }

    const char* session_token = fields[0];
    const char* event_id_str  = fields[1];
    // Kiểm tra session
    Session* session = session_find_by_token(ctx->sm, session_token);
    if (session == NULL || !session->is_active) {
        send_response(client_sock, RESPONSE_UNAUTHORIZED, "Invalid session ID", NULL);
        printf("[GET_EVENTS] Failed - Invalid session ID\n");
        return;
    }
    int user_id = session->user_id;
    // int event_id = atoi(fields[1]);
    char* end = NULL;
    int event_id = strtol(event_id_str, &end, 10);
    if (end == event_id_str || *end != '\0' || event_id <= 0) {
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Invalid event_id (must be a positive integer)", NULL);
        return;
    }

     // 3) query DB
    // extra trả về: event_id|title|description|location|event_time|event_type|status|creator_id
    char* extra = NULL;
    int rc = db_get_event_detail_by_creator(user_id, event_id, &extra);
    if (rc < 0) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }
    if (rc == 0) {
        send_response(client_sock, RESPONSE_NOT_FOUND, "Event not found", NULL);
        return;
    }

    send_response(client_sock, RESPONSE_OK, "Event detail retrieved successfully", extra);
    free(extra);
}
// EDIT_EVENT|session_id|event_id|title|description|location|event_time|event_type
void handle_update_event(ServerContext* ctx, int client_sock, char** fields, int field_count) {
    if (field_count != 7) {
        send_response(client_sock, RESPONSE_BAD_REQUEST,"Format: UPDATE_EVENT|session_id|event_id|title|description|location|event_time|event_type",NULL);
        return;
    }

    const char* session_token = fields[0]; 
    const char* event_id_str = fields[1];
    const char* title = fields[2];
    const char* description = fields[3];
    const char* location = fields[4];
    const char* event_time = fields[5];
    const char* event_type  = fields[6];

    //Check session
    Session* session = session_find_by_token(ctx->sm, session_token);
    if (!session || !session->is_active) {
        send_response(client_sock, RESPONSE_UNAUTHORIZED, "Invalid session ID", NULL);
        return;
    }
    int user_id = session->user_id;

    //Parse event_id
    char* end = NULL;
    long eid = strtol(event_id_str, &end, 10);
    if (end == event_id_str || *end != '\0' || eid <= 0) {
        send_response(client_sock, RESPONSE_BAD_REQUEST,"Invalid event_id (must be a positive integer)", NULL);
        return;
    }
    int event_id = (int)eid;

    // Validate field
    if (!title || strlen(title) == 0) {
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Title is required", NULL);
        return;
    }
    if (!event_time || strlen(event_time) == 0) {
        send_response(client_sock, RESPONSE_BAD_REQUEST,"Event time is required (YYYY-MM-DD HH:MM:SS)", NULL);
        return;
    }
    if (!location || strlen(location) == 0) {
        send_response(client_sock, RESPONSE_BAD_REQUEST, "Location is required", NULL);
        return;
    }
    if (strcmp(event_type, "public") != 0 && strcmp(event_type, "private") != 0) {
        send_response(client_sock, RESPONSE_BAD_REQUEST,"Event type must be 'public' or 'private'", NULL);
        return;
    }
    //gọi qua db để update 
    int rc = db_update_event(user_id, event_id, title, description, location, event_time, event_type);

    if (rc < 0) {
        send_response(client_sock, RESPONSE_UNPROCESSABLE,"Invalid data. check data update", NULL);
        return;
    }
    if (rc == 0) {
        // Không thấy event theo creator_id + event_id
        send_response(client_sock, RESPONSE_BAD_REQUEST,"Event not found or not editable", NULL);
        return;
    }
    send_response(client_sock, RESPONSE_OK, "Event updated successfully", NULL);
}

// DELETE_EVENT|session_id|event_id
void handle_delete_event(ServerContext* ctx, int client_sock, char** fields, int field_count) {
    if (field_count != 2) {
        send_response(client_sock, RESPONSE_BAD_REQUEST,"Format: DELETE_EVENT|session_id|event_id", NULL);
        return;
    }

    const char* session_token = fields[0];
    const char* event_id_str  = fields[1];

    // Check session
    Session* session = session_find_by_token(ctx->sm, session_token);
    if (!session || !session->is_active) {
        send_response(client_sock, RESPONSE_UNAUTHORIZED, "Invalid session ID", NULL);
        return;
    }
    int user_id = session->user_id;

    //Parse event_id
    char* end = NULL;
    long eid = strtol(event_id_str, &end, 10);
    if (end == event_id_str || *end != '\0' || eid <= 0) {
        send_response(client_sock, RESPONSE_BAD_REQUEST,"Invalid event_id (must be a positive integer)", NULL);
        return;
    }
    int event_id = (int)eid;

    //DB delete
    int rc = db_delete_event(user_id, event_id);
    if (rc < 0) {
        send_response(client_sock, RESPONSE_SERVER_ERROR, "Internal server error", NULL);
        return;
    }
    if (rc == 0) {
        send_response(client_sock, RESPONSE_NOT_FOUND, "Event not found", NULL);
        return;
    }

    send_response(client_sock, RESPONSE_OK, "Event deleted successfully", NULL);
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
    } else if (strcmp(command, CMD_CREATE_EVENT) == 0) {
        handle_create_event(ctx, client_sock, fields, field_count);
    } else if (strcmp(command, CMD_GET_EVENTS) == 0) {
        handle_get_events(ctx, client_sock, fields, field_count);
    } else if (strcmp(command, CMD_GET_EVENT_DETAIL) == 0) {
        handle_get_event_detail(ctx, client_sock, fields, field_count);
    } else if (strcmp(command, CMD_UPDATE_EVENT) == 0) {
        handle_update_event(ctx, client_sock, fields, field_count);
    } else if (strcmp(command, CMD_DELETE_EVENT) == 0) {
        handle_delete_event(ctx, client_sock, fields, field_count);
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
