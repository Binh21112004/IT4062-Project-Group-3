#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/protocol.h"
#include <time.h>

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
        printf("4. Send Friend Request\n");
        printf("5. Accept Friend Request\n");
        printf("6. Reject Friend Request\n");
        printf("7. Unfriend\n");
        printf("8. Create event\n");
        printf("9. Get my events\n");
        printf("10. Edit event\n");
        printf("11. Delete event\n");
        printf("12. Get friends list\n");
        printf("13. Send event invitation\n");
        printf("14. Accept event invitation\n");
        printf("15. Join event\n");
        printf("16. Accept join event\n");
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

void do_send_friend_request() {
    if (strlen(session_id) == 0) {
        printf("\n[ERROR] You are not logged in!\n");
        return;
    }
    
    char friend_username[MAX_USERNAME];
    
    printf("Friend's username: ");
    fflush(stdout);
    if (fgets(friend_username, sizeof(friend_username), stdin) == NULL) return;
    friend_username[strcspn(friend_username, "\n")] = 0;
    
    // Gửi request
    const char* fields[] = {session_id, friend_username};
    send_request(client_sock, CMD_SEND_FRIEND_REQUEST, fields, 2);
    
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

void do_accept_friend_request() {
    if (strlen(session_id) == 0) {
        printf("\n[ERROR] You are not logged in!\n");
        return;
    }
    
    char requester_username[MAX_USERNAME];
    
    printf("Requester's username: ");
    fflush(stdout);
    if (fgets(requester_username, sizeof(requester_username), stdin) == NULL) return;
    requester_username[strcspn(requester_username, "\n")] = 0;
    // Send request
    const char* fields[] = {session_id, requester_username};
    send_request(client_sock, CMD_ACCEPT_FRIEND_REQUEST, fields, 2);
    
    // Receive response
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

void do_reject_friend_request() {
    if (strlen(session_id) == 0) {
        printf("\n[ERROR] You are not logged in!\n");
        return;
    }
    
    char requester_username[MAX_USERNAME];
    
    printf("Requester's username: ");
    fflush(stdout);
    if (fgets(requester_username, sizeof(requester_username), stdin) == NULL) return;
    requester_username[strcspn(requester_username, "\n")] = 0;
    
    // Send request
    const char* fields[] = {session_id, requester_username};
    send_request(client_sock, CMD_REJECT_FRIEND_REQUEST, fields, 2);
    
    // Receive response
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

void do_unfriend() {
    if (strlen(session_id) == 0) {
        printf("\n[ERROR] You are not logged in!\n");
        return;
    }
    
    char friend_username[MAX_USERNAME];
    
    printf("Friend's username: ");
    fflush(stdout);
    if (fgets(friend_username, sizeof(friend_username), stdin) == NULL) return;
    friend_username[strcspn(friend_username, "\n")] = 0;
    
    // Send request
    const char* fields[] = {session_id, friend_username};
    send_request(client_sock, CMD_UNFRIEND, fields, 2);
    
    // Receive response
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

// check chuỗi rỗng hoặc toàn khoảng trắng
static int is_blank(const char* s) {
    if (!s) return 1;
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

// validate cơ bản YYYY-MM-DD HH:MM:SS
int is_valid_datetime(const char* s) {
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


// Check start_time > thời gian hiện tại (local time) 
// trả về 1 = hợp lệ , 0 = không hợp lệ, -1 = parse lỗi
int check_time_after_now(const char* start_time) {
    if (!start_time) return -1;

    struct tm tm_start = {0};
    if (!strptime(start_time, "%Y-%m-%d %H:%M:%S", &tm_start)) {
        return -1;
    }

    time_t event_time = mktime(&tm_start);
    time_t now = time(NULL);

    return (difftime(event_time, now) > 0) ? 1 : 0;
}

void do_create_event() {
    if (strlen(session_id) == 0) {
        printf("[ERROR] You must login first.\n");
        return;
    }

    char name[128], date[64], location[128], type[32], desc[256];

    printf("Event name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = 0;

    printf("Event date (YYYY-MM-DD HH:MM:SS): ");
    fgets(date, sizeof(date), stdin);
    date[strcspn(date, "\n")] = 0;
    if(check_time_after_now(date) == 0){
        printf("[ERROR] Event date/time must be in the future.\n");
        return;
    }

    printf("Location: ");
    fgets(location, sizeof(location), stdin);
    location[strcspn(location, "\n")] = 0;

    printf("Event type (public/private): ");
    fgets(type, sizeof(type), stdin);
    type[strcspn(type, "\n")] = 0;

    printf("Description: ");
    fgets(desc, sizeof(desc), stdin);
    desc[strcspn(desc, "\n")] = 0;

    // Client validate để user thấy lỗi ngay 
    if (is_blank(name)) {
        printf("[ERROR] Event name is required.\n");
        return;
    }
    if (is_blank(date)) {
        printf("[ERROR] Event date is required (at least date/time).\n");
        return;
    }
    if (!is_valid_datetime(date)) {
        printf("[ERROR] Invalid date/time. Use: YYYY-MM-DD HH:MM:SS (e.g., 2025-12-25 18:00:00)\n");
        return;
    }
    if (is_blank(location)) {
        printf("[ERROR] Location is required.\n");
        return;
    }
    if (strcmp(type, "public") != 0 && strcmp(type, "private") != 0) {
        printf("[ERROR] Event type must be 'public' or 'private'.\n");
        return;
    }

    const char* fields[] = { session_id, name, date, location, type, desc };

    int sent = send_request(client_sock, CMD_CREATE_EVENT, fields, 6);
    if (sent < 0) {
        printf("[ERROR] Failed to send request.\n");
        return;
    }

    int code;
    char message[MAX_BUFFER];
    char* extra = receive_response(client_sock, &code, message, MAX_BUFFER);

    // Xử lý các trường hợp
    printf("[SERVER] (%d) %s\n", code, message);

    if (code == RESPONSE_OK) {
        if (extra && strlen(extra) > 0) {
            printf("New event id: %s\n", extra);
        }
    } else if (code == RESPONSE_BAD_REQUEST) {
        // server báo user nhập sai
        if (extra && strlen(extra) > 0) {
            printf("Details: %s\n", extra);
        }
        printf("-> Please check your input and try again.\n");
    } else if (code == RESPONSE_UNAUTHORIZED) {
        printf("-> Session invalid/expired. Please login again.\n");
        // Option: clear session locally
        // session_id[0] = '\0';
    } else {
        if (extra && strlen(extra) > 0) {
            printf("Details: %s\n", extra);
        }
        printf("-> Server error. Please try again later.\n");
    }

    free(extra);
}

void do_get_events() {
    if (strlen(session_id) == 0) {
        printf("[ERROR] You must login first.\n");
        return;
    }

    const char* fields[] = { session_id };
    send_request(client_sock, CMD_GET_EVENTS, fields, 1);

    int code;
    char message[MAX_BUFFER];
    char* extra = receive_response(client_sock, &code, message, MAX_BUFFER);

    printf("[SERVER] (%d) %s\n", code, message);

    if (code == RESPONSE_OK && extra && strlen(extra) > 0) {
        printf("=== Your events ===\n");
        // mỗi dòng 1 event, fields cách nhau bởi ';'
        char* line = strtok(extra, "\n");
        int i = 1;
        while (line) {
            printf("%d) %s\n", i++, line);
            line = strtok(NULL, "\n");
        }
    } else if (code == RESPONSE_OK) {
        printf("You have no events.\n");
    }

    free(extra);
}

// extra_in sẽ bị sửa (vì strtok), nên truyền vào bản copy
// out_fields[i] sẽ trỏ vào vùng nhớ của extra_in
int parse_event_extra(char* extra_in, char* out_fields[7]) {
    int i = 0;
    char* tok = strtok(extra_in, "|");
    while (tok && i < 7) {
        out_fields[i++] = tok;
        tok = strtok(NULL, "|");
    }
    return (i == 7) ? 1 : 0; 
}
// sửa thông tin sự kiện 
void do_edit_event() {
    if (strlen(session_id) == 0) {
        printf("[ERROR] You must login first.\n");
        return;
    }

    do_get_events();

    int event_id;
    printf("Event ID to edit: ");
    scanf("%d", &event_id);
    getchar();
    
    char event_id_str[16];
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);

    const char* fields[] = { session_id, event_id_str };
    send_request(client_sock, CMD_GET_EVENT_DETAIL, fields, 2);

    int code;
    char message[MAX_BUFFER];
    char* extra = receive_response(client_sock, &code, message, MAX_BUFFER);

    printf("[SERVER] (%d) %s\n", code, message);
    
    if (code != RESPONSE_OK || !extra || strlen(extra) == 0) {
        printf("[ERROR] Cannot get event detail.\n");
        free(extra);
    return;
    }

    //copy extra ra buffer tạm để parse
    char extra_copy[MAX_BUFFER];
    strncpy(extra_copy, extra, sizeof(extra_copy) - 1);
    extra_copy[sizeof(extra_copy) - 1] = '\0';
    free(extra);

    // Parse ra 7 trường
    char* f[7] = {0};
    if (!parse_event_extra(extra_copy, f)) {
        printf("[ERROR] Invalid event detail format from server.\n");
        return;
    }

    char* cur_event_id = f[0];
    char* cur_title = f[1];
    char* cur_desc = f[2];
    char* cur_loc = f[3];
    char* cur_time = f[4];
    char* cur_type = f[5];
    char* cur_status = f[6];

    // Hiển thị hiện tại
    printf("\n Current Event Detail\n");
    printf("ID :%s\n", cur_event_id);
    printf("Title :%s\n",cur_title);
    printf("Desc: %s\n", cur_desc);
    printf("Loc:%s\n", cur_loc);
    printf("Time:%s\n",cur_time);
    printf("Type:%s\n", cur_type);
    printf("Status: %s\n", cur_status);

    // Nhập giá trị mới: rỗng thì  giữ nguyên
    char new_title[128], new_desc[256], new_loc[128], new_time[64], new_type[32];

    printf("\n Nếu không nhập gì thì giữ nguyên \n");

    printf("New title: ");
    fgets(new_title, sizeof(new_title), stdin);
    new_title[strcspn(new_title, "\n")] = 0;

    printf("New description: ");
    fgets(new_desc, sizeof(new_desc), stdin);
    new_desc[strcspn(new_desc, "\n")] = 0;

    printf("New location: ");
    fgets(new_loc, sizeof(new_loc), stdin);
    new_loc[strcspn(new_loc, "\n")] = 0;

    printf("New time (YYYY-MM-DD HH:MM:SS): ");
    fgets(new_time, sizeof(new_time), stdin);
    new_time[strcspn(new_time, "\n")] = 0;

    printf("New type (public/private): ");
    fgets(new_type, sizeof(new_type), stdin);
    new_type[strcspn(new_type, "\n")] = 0;

    // Nếu rỗng thì giữ nguyên
    if(strlen(new_title) == 0) strncpy(new_title, cur_title, sizeof(new_title)-1);
    if(strlen(new_desc)== 0) strncpy(new_desc, cur_desc, sizeof(new_desc)-1);
    if(strlen(new_loc) == 0) strncpy(new_loc,cur_loc,sizeof(new_loc)-1);
    if(strlen(new_time)== 0) strncpy(new_time,cur_time,sizeof(new_time)-1);
    if(strlen(new_type) == 0) strncpy(new_type, cur_type, sizeof(new_type)-1);

    new_title[sizeof(new_title)-1] = '\0';
    new_desc[sizeof(new_desc)-1] = '\0';
    new_loc[sizeof(new_loc)-1] = '\0';
    new_time[sizeof(new_time)-1] = '\0';
    new_type[sizeof(new_type)-1]= '\0';

    if (strcmp(new_type, "public") != 0 && strcmp(new_type, "private") != 0) {
        printf("[ERROR] Invalid type. Must be public/private \n");
        return;
    }
    if(is_valid_datetime(new_time)==0) {
        printf("[ERROR] Invalid date/time . format true : YYYY-MM-DD HH:MM:SS \n");
        return;
    }

    // Gửi request update
    // EDIT_EVENT|session_id|event_id|title|description|location|event_time|event_type
    const char* fields_update[] = {session_id,cur_event_id, new_title,new_desc,new_loc,new_time,new_type };

    send_request(client_sock, CMD_UPDATE_EVENT , fields_update, 7);

    // nhận response update
    int code2;
    char msg2[MAX_BUFFER];
    char* extra2 = receive_response(client_sock, &code2, msg2, MAX_BUFFER);

    printf("[SERVER] (%d) %s\n", code2, msg2);
    if (code2 == RESPONSE_OK) {
        printf("[INFO] Event updated successfully.\n");
    } else {
        printf("[ERROR] Update failed.\n");
    }
    free(extra2);
}

void do_delete_event() {
    if (strlen(session_id) == 0) {
        printf("[ERROR] You must login first.\n");
        return;
    }

    do_get_events();

    int event_id;
    printf("Event ID to delete: ");
    scanf("%d", &event_id);
    getchar();

    char event_id_str[16];
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);

    const char* fields[] = { session_id, event_id_str };
    send_request(client_sock, CMD_DELETE_EVENT, fields, 2);

    int code;
    char message[MAX_BUFFER];
    char* extra = receive_response(client_sock, &code, message, MAX_BUFFER);

    printf("[SERVER] (%d) %s\n", code, message);

    if (code == RESPONSE_OK) {
        printf("[INFO] Deleted event %d.\n", event_id);
    } else {
        printf("[ERROR] Delete failed.\n");
    }

    free(extra);
}
void do_get_friends() {
    if (strlen(session_id) == 0) {
        printf("[ERROR] You must login first.\n");
        return;
    }

    const char* fields[] = { session_id };
    send_request(client_sock, CMD_GET_FRIENDS, fields, 1);

    int code;
    char message[MAX_BUFFER];
    char* extra = receive_response(client_sock, &code, message, MAX_BUFFER);

    printf("[SERVER] (%d) %s\n", code, message);

    if (code == RESPONSE_OK && extra && strlen(extra) > 0) {
        printf("=== Your friends ===\n");
        // mỗi dòng 1 friend, fields cách nhau bởi ';'
        char* line = strtok(extra, "\n");
        int i = 1;
        while (line) {
            printf("%d) %s\n", i++, line);
            line = strtok(NULL, "\n");
        }
    } else if (code == RESPONSE_OK) {
        printf("You have no friends.\n");
    }

    free(extra);
}
void do_send_invitation_event(){
    if (strlen(session_id) == 0) {
        printf("[ERROR] You must login first.\n");
        return;
    }
    do_get_events();
    int event_id;
    printf("Event ID to invite: ");
    scanf("%d", &event_id);
    getchar();

    do_get_friends();
    char friend_username[MAX_USERNAME];
    printf("Enter friend's username: ");
    fflush(stdout);
    if (fgets(friend_username, sizeof(friend_username), stdin) == NULL) return;
    friend_username[strcspn(friend_username, "\n")] = 0;
    
    // Validate input
    if (strlen(friend_username) == 0) {
        printf("[ERROR] Username cannot be empty.\n");
        return;
    }

    char event_id_str[16];
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);
    const char* fields[] = { session_id, friend_username, event_id_str };
    send_request(client_sock, CMD_SEND_INVITATION_EVENT, fields, 3);
    int code;
    char message[MAX_BUFFER];
    char* extra = receive_response(client_sock, &code, message, MAX_BUFFER);
    printf("[SERVER] (%d) %s\n", code, message);

    if (code == RESPONSE_OK) {
    printf("[SUCCESS] %s\n", message);
    } else if (code == RESPONSE_UNAUTHORIZED) {
        printf("[ERROR] %s\n", message);
    } else if (code == RESPONSE_NOT_FOUND) {
        printf("[ERROR] %s\n", message);
    } else if (code == RESPONSE_BAD_REQUEST) {
        printf("[ERROR] %s\n", message);
    } else if (code == RESPONSE_CONFLICT) {
        printf("[ERROR] %s\n", message);
    } else {
        printf("[ERROR] %s\n", message);
    }
    
    if (extra != NULL) {
        free(extra);
    }
}

// Accept invitation request
void do_accept_invitation_request() {
    if (strlen(session_id) == 0) {
        printf("[ERROR] You must login first.\n");
        return;
    }
    
    char requester_username[MAX_USERNAME];
    int event_id;
    printf("Event ID to invite: ");
    scanf("%d", &event_id);
    getchar();

    if (event_id <= 0) {
        printf("[ERROR] Invalid event ID.\n");
        return;
    }
    char event_id_str[16];
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);

    printf("Enter requester's username: ");
    fflush(stdout);
    if (fgets(requester_username, sizeof(requester_username), stdin) == NULL) return;
    requester_username[strcspn(requester_username, "\n")] = 0;
    
    // Validate input
    if (strlen(requester_username) == 0) {
        printf("[ERROR] Username cannot be empty.\n");
        return;
    }
    
    // Send request: ACCEPT_INVITATION_REQUEST|session_id|requester_username
    const char* fields[] = {session_id, requester_username, event_id_str};
    send_request(client_sock, CMD_ACCEPT_INVITATION_REQUEST, fields, 3);
    
    // Receive response
    int code;
    char message[MAX_BUFFER];
    char* extra_data = receive_response(client_sock, &code, message, MAX_BUFFER);
    
    printf("[SERVER] (%d) %s\n", code, message);
    
    if (code == RESPONSE_OK) {
        printf("[SUCCESS] Accepted invitation from '%s'\n", requester_username);
    } else if (code == RESPONSE_UNAUTHORIZED) {
        printf("[ERROR] Session invalid/expired. Please login again.\n");
    } else if (code == RESPONSE_NOT_FOUND) {
        printf("[ERROR] No invitation from '%s' found.\n", requester_username);
    } else if (code == RESPONSE_SERVER_ERROR) {
        printf("[ERROR] Internal server error occurred.\n");
    } else {
        printf("[ERROR] Failed to accept invitation request.\n");
        if (extra_data && strlen(extra_data) > 0) {
            printf("Details: %s\n", extra_data);
        }
    }
    
    if (extra_data != NULL) {
        free(extra_data);
    }
}

void do_join_event(){
    if (strlen(session_id) == 0) {
        printf("[ERROR] You must login first.\n");
        return;
    }
    int event_id;
    printf("Event ID to join: ");
    scanf("%d", &event_id);
    getchar();
    
    char event_id_str[16];
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);
    const char* fields[] = { session_id, event_id_str };
    send_request(client_sock, CMD_JOIN_EVENT, fields, 2);

    int code;
    char message[MAX_BUFFER];   
    char* extra = receive_response(client_sock, &code, message, MAX_BUFFER);
    printf("[SERVER] (%d) %s\n", code, message);
    if (code == RESPONSE_OK) {
        printf("[SUCCESS] %s\n", message);
    }  else if (code == RESPONSE_UNAUTHORIZED) {
        printf("[ERROR] %s\n", message);
    } else if (code == RESPONSE_NOT_FOUND) {
        printf("[ERROR] %s\n", message);
    } else if (code == RESPONSE_CONFLICT) {
        printf("[ERROR] %s\n", message);
    } else {
        printf("[ERROR] %s\n", message);
    }
    if (extra != NULL) {
        free(extra);
    }
}
void accept_request_join_event() {
    if (strlen(session_id) == 0) {
        printf("[ERROR] You must login first.\n");
        return;
    }
    int event_id;
    printf("Event ID to accept: ");
    scanf("%d", &event_id);
    getchar();

    char join_username[MAX_USERNAME];
    printf("Enter username accept: ");
    fflush(stdout);
    if (fgets(join_username, sizeof(join_username), stdin) == NULL) return;
    join_username[strcspn(join_username, "\n")] = 0;

    char event_id_str[16];
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);
    const char* fields[] = { session_id, event_id_str, join_username };
    send_request(client_sock, CMD_ACCEPT_JOIN_REQUEST, fields, 3);

    int code;
    char message[MAX_BUFFER];   
    char* extra = receive_response(client_sock, &code, message, MAX_BUFFER);
    printf("[SERVER] (%d) %s\n", code, message);
    if (code == RESPONSE_OK) {
        printf("[SUCCESS] %s\n", message);
    }  else if (code == RESPONSE_UNAUTHORIZED) {
        printf("[ERROR] %s\n", message);
    } else if (code == RESPONSE_NOT_FOUND) {
        printf("[ERROR] %s\n", message);
    } else if (code == RESPONSE_CONFLICT) {
        printf("[ERROR] %s\n", message);
    } else {
        printf("[ERROR] %s\n", message);
    }
    if (extra != NULL) {
        free(extra);
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
            case 4:
                do_send_friend_request();
                break;
            case 5:
                do_accept_friend_request();
                break;
            case 6:
                do_reject_friend_request();
                break;
            case 7:
                do_unfriend();
                break;
            case 8:
                do_create_event();
                break;
            case 9:
                do_get_events();
                break;
            case 10:
                do_edit_event();
                break;
            case 11:
                do_delete_event();
                break;
            case 12:
                do_get_friends();
                break;
            case 13:
                do_send_invitation_event();
                break;
            case 14:
                do_accept_invitation_request();
                break;
            case 15:
                do_join_event();
                break;  
            case 16:
                accept_request_join_event();
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
