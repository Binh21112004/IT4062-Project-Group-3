#include "postgres_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>

static PGconn* conn = NULL;

// Initialize database connection
int db_init(const char* conninfo) {
    conn = PQconnectdb(conninfo);
    
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        conn = NULL;
        return -1;
    }
    
    printf("Connected to PostgreSQL database successfully\n");
    return 0;
}

// Cleanup database connection
void db_cleanup() {
    if (conn) {
        PQfinish(conn);
        conn = NULL;
    }
}

// Get database connection
PGconn* db_get_connection() {
    return conn;
}

// Validate username (no special characters except underscore)
int db_validate_username(const char* username) {
    if (username == NULL || strlen(username) == 0) {
        return 0;
    }
    
    // Check for special characters (except underscore)
    for (int i = 0; username[i] != '\0'; i++) {
        char c = username[i];
        if (c == '!' || c == '@' || c == '#' || c == '$' || c == '%' || 
            c == '^' || c == '&' || c == '*' || c == '(' || c == ')' || c == '|') {
            return 0;
        }
    }
    
    return 1;
}

// Validate email format (basic check for @ and .)
int db_validate_email(const char* email) {
    if (email == NULL || strlen(email) == 0) {
        return 0;
    }
    
    const char* at = strchr(email, '@');
    if (at == NULL || at == email) {
        return 0;  // No @ or @ at the beginning
    }
    
    const char* dot = strchr(at, '.');
    if (dot == NULL || dot == at + 1 || dot[1] == '\0') {
        return 0;  // No . after @ or . right after @ or . at the end
    }
    
    return 1;
}

// Generate random session token
char* db_generate_session_token() {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char* token = malloc(65); // 64 chars + null terminator
    if (!token) return NULL;
    
    srand(time(NULL) ^ (unsigned int)((uintptr_t)token));
    
    for (int i = 0; i < 64; i++) {
        token[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    token[64] = '\0';
    
    return token;
}

// Create new user
int db_create_user(const char* username, const char* password, const char* email) {
    if (!conn) return -1;
    
    // Validate username
    if (!db_validate_username(username)) {
        return -3; // Invalid username
    }
    
    // Validate email
    if (!db_validate_email(email)) {
        return -4; // Invalid email format
    }
    
    const char* paramValues[3] = {username, password, email};
    
    PGresult* res = PQexecParams(conn,
        "INSERT INTO users (username, password, email) VALUES ($1, $2, $3) RETURNING user_id",
        3, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        // Check if it's a unique violation (username already exists)
        const char* sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
        PQclear(res);
        
        if (sqlstate && strcmp(sqlstate, "23505") == 0) {
            return -2; // Username exists
        }
        return -1; // Other error
    }
    
    int user_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    // Log activity
    db_log_activity(user_id, "USER_REGISTERED", username);
    
    return user_id;
}

// Find user by username
int db_find_user_by_username(const char* username, int* user_id, char* email, int email_size, int* is_active) {
    if (!conn) return -1;
    
    const char* paramValues[1] = {username};
    
    PGresult* res = PQexecParams(conn,
        "SELECT user_id, email, status FROM users WHERE username = $1",
        1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }
    
    if (PQntuples(res) == 0) {
        PQclear(res);
        return 0; // User not found
    }
    
    *user_id = atoi(PQgetvalue(res, 0, 0));
    strncpy(email, PQgetvalue(res, 0, 1), email_size - 1);
    email[email_size - 1] = '\0';
    *is_active = strcmp(PQgetvalue(res, 0, 2), "active") == 0 ? 1 : 0;
    
    PQclear(res);
    return 1; // User found
}

// Find user by ID
int db_find_user_by_id(int user_id, char* username, int username_size, char* email, int email_size, int* is_active) {
    if (!conn) return -1;
    
    char user_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    const char* paramValues[1] = {user_id_str};
    
    PGresult* res = PQexecParams(conn,
        "SELECT username, email, status FROM users WHERE user_id = $1",
        1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }
    
    if (PQntuples(res) == 0) {
        PQclear(res);
        return 0; // User not found
    }
    
    strncpy(username, PQgetvalue(res, 0, 0), username_size - 1);
    username[username_size - 1] = '\0';
    strncpy(email, PQgetvalue(res, 0, 1), email_size - 1);
    email[email_size - 1] = '\0';
    *is_active = strcmp(PQgetvalue(res, 0, 2), "active") == 0 ? 1 : 0;
    
    PQclear(res);
    return 1; // User found
}

// Verify password
int db_verify_password(const char* username, const char* password) {
    if (!conn) return 0;
    
    const char* paramValues[2] = {username, password};
    
    PGresult* res = PQexecParams(conn,
        "SELECT user_id FROM users WHERE username = $1 AND password = $2 AND status = 'active'",
        2, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 0;
    }
    
    int found = PQntuples(res) > 0;
    PQclear(res);
    
    return found;
}

// Update user active status
int db_update_user_status(int user_id, int is_active) {
    if (!conn) return -1;
    
    char user_id_str[20];
    const char* status_str;
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    status_str = is_active ? "active" : "inactive";
    
    const char* paramValues[2] = {user_id_str, status_str};
    
    PGresult* res = PQexecParams(conn,
        "UPDATE users SET status = $2 WHERE user_id = $1",
        2, NULL, paramValues, NULL, NULL, 0);
    
    int success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    return success ? 0 : -1;
}

// Create session
int db_create_session(int user_id, char* session_token, int token_size) {
    if (!conn) return -1;
    
    char* token = db_generate_session_token();
    if (!token) return -1;
    
    char user_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    
    const char* paramValues[2] = {user_id_str, token};
    
    PGresult* res = PQexecParams(conn,
        "INSERT INTO sessions (user_id, session_token) VALUES ($1, $2) RETURNING session_id",
        2, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        free(token);
        PQclear(res);
        return -1;
    }
    
    strncpy(session_token, token, token_size - 1);
    session_token[token_size - 1] = '\0';
    
    free(token);
    PQclear(res);
    
    // Log activity
    db_log_activity(user_id, "USER_LOGIN", "Session created");
    
    return 0;
}

// Validate session
int db_validate_session(const char* session_token, int* user_id) {
    if (!conn) return 0;
    
    const char* paramValues[1] = {session_token};
    
    PGresult* res = PQexecParams(conn,
        "SELECT user_id FROM sessions WHERE session_token = $1 AND is_active = true",
        1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return 0;
    }
    
    if (PQntuples(res) == 0) {
        PQclear(res);
        return 0; // Session not found
    }
    
    *user_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    return 1; // Valid session
}

// Delete session
int db_delete_session(const char* session_token) {
    if (!conn) return -1;
    
    const char* paramValues[1] = {session_token};
    
    PGresult* res = PQexecParams(conn,
        "UPDATE sessions SET is_active = false WHERE session_token = $1",
        1, NULL, paramValues, NULL, NULL, 0);
    
    int success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    return success ? 0 : -1;
}

// Delete all user sessions
int db_delete_user_sessions(int user_id) {
    if (!conn) return -1;
    
    char user_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    
    const char* paramValues[1] = {user_id_str};
    
    PGresult* res = PQexecParams(conn,
        "UPDATE sessions SET is_active = false WHERE user_id = $1",
        1, NULL, paramValues, NULL, NULL, 0);
    
    int success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    // Log activity
    db_log_activity(user_id, "USER_LOGOUT", "All sessions terminated");
    
    return success ? 0 : -1;
}

// Send friend request
int db_send_friend_request(int sender_id, int receiver_id) {
    if (!conn) return -1;
    
    // Check if already friends
    if (db_check_friendship(sender_id, receiver_id)) {
        return -2; // Already friends
    }
    
    char sender_id_str[20], receiver_id_str[20];
    snprintf(sender_id_str, sizeof(sender_id_str), "%d", sender_id);
    snprintf(receiver_id_str, sizeof(receiver_id_str), "%d", receiver_id);
    
    const char* paramValues[2] = {sender_id_str, receiver_id_str};
    
    PGresult* res = PQexecParams(conn,
        "INSERT INTO friend_requests (sender_id, receiver_id) VALUES ($1, $2) RETURNING request_id",
        2, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char* sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
        PQclear(res);
        
        if (sqlstate && strcmp(sqlstate, "23505") == 0) {
            return -3; // Request already exists
        }
        return -1;
    }
    
    int request_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "Sent friend request to user %d", receiver_id);
    db_log_activity(sender_id, "FRIEND_REQUEST_SENT", details);
    
    return request_id;
}

// Accept friend request
int db_accept_friend_request(int request_id) {
    if (!conn) return -1;
    
    char request_id_str[20];
    snprintf(request_id_str, sizeof(request_id_str), "%d", request_id);
    
    // Begin transaction
    PGresult* res = PQexec(conn, "BEGIN");
    PQclear(res);
    
    // Get sender and receiver IDs
    const char* paramValues1[1] = {request_id_str};
    res = PQexecParams(conn,
        "SELECT sender_id, receiver_id FROM friend_requests WHERE request_id = $1 AND status = 'pending'",
        1, NULL, paramValues1, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    
    int sender_id = atoi(PQgetvalue(res, 0, 0));
    int receiver_id = atoi(PQgetvalue(res, 0, 1));
    PQclear(res);
    
    // Update request status
    res = PQexecParams(conn,
        "UPDATE friend_requests SET status = 'accepted' WHERE request_id = $1",
        1, NULL, paramValues1, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    PQclear(res);
    
    // Create friendship
    char sender_id_str[20], receiver_id_str[20];
    snprintf(sender_id_str, sizeof(sender_id_str), "%d", sender_id);
    snprintf(receiver_id_str, sizeof(receiver_id_str), "%d", receiver_id);
    
    const char* paramValues2[2] = {sender_id_str, receiver_id_str};
    res = PQexecParams(conn,
        "INSERT INTO friendships (user_id, friend_id) VALUES ($1, $2)",
        2, NULL, paramValues2, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    PQclear(res);
    
    // Commit transaction
    res = PQexec(conn, "COMMIT");
    PQclear(res);
    
    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "Accepted friend request from user %d", sender_id);
    db_log_activity(receiver_id, "FRIEND_REQUEST_ACCEPTED", details);
    
    return 0;
}

// Reject friend request
int db_reject_friend_request(int request_id) {
    if (!conn) return -1;
    
    char request_id_str[20];
    snprintf(request_id_str, sizeof(request_id_str), "%d", request_id);
    
    const char* paramValues[1] = {request_id_str};
    
    PGresult* res = PQexecParams(conn,
        "UPDATE friend_requests SET status = 'rejected' WHERE request_id = $1 AND status = 'pending'",
        1, NULL, paramValues, NULL, NULL, 0);
    
    int success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    return success ? 0 : -1;
}

// Cancel friend request
int db_cancel_friend_request(int request_id) {
    if (!conn) return -1;
    
    char request_id_str[20];
    snprintf(request_id_str, sizeof(request_id_str), "%d", request_id);
    
    const char* paramValues[1] = {request_id_str};
    
    PGresult* res = PQexecParams(conn,
        "DELETE FROM friend_requests WHERE request_id = $1 AND status = 'pending'",
        1, NULL, paramValues, NULL, NULL, 0);
    
    int success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    return success ? 0 : -1;
}

// Remove friend
int db_remove_friend(int user_id, int friend_id) {
    if (!conn) return -1;
    
    char user_id_str[20], friend_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    snprintf(friend_id_str, sizeof(friend_id_str), "%d", friend_id);
    
    const char* paramValues[2] = {user_id_str, friend_id_str};
    
    PGresult* res = PQexecParams(conn,
        "DELETE FROM friendships WHERE (user_id = $1 AND friend_id = $2) OR (user_id = $2 AND friend_id = $1)",
        2, NULL, paramValues, NULL, NULL, 0);
    
    int success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "Removed friend user %d", friend_id);
    db_log_activity(user_id, "FRIEND_REMOVED", details);
    
    return success ? 0 : -1;
}

// Get friend requests
int db_get_friend_requests(int user_id, char*** results, int* count) {
    if (!conn) return -1;
    
    char user_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    
    const char* paramValues[1] = {user_id_str};
    
    PGresult* res = PQexecParams(conn,
        "SELECT fr.request_id, u.username, fr.sent_at "
        "FROM friend_requests fr "
        "JOIN users u ON fr.sender_id = u.user_id "
        "WHERE fr.receiver_id = $1 AND fr.status = 'pending' "
        "ORDER BY fr.sent_at DESC",
        1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }
    
    *count = PQntuples(res);
    *results = malloc(*count * sizeof(char*));
    
    for (int i = 0; i < *count; i++) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%s|%s|%s",
                 PQgetvalue(res, i, 0), // request_id
                 PQgetvalue(res, i, 1), // username
                 PQgetvalue(res, i, 2)); // sent_at
        
        (*results)[i] = strdup(buffer);
    }
    
    PQclear(res);
    return 0;
}

/**
 * Chức năng : Lấy danh sách bạn bè của user
 * - Query từ bảng friendships và JOIN với users
 * - Trả về thông tin: user_id, username, email
 * - Sắp xếp theo username
 * @param results - Mảng chứa kết quả (cần free bằng db_free_results() sau khi dùng)
 * @param count - Số lượng bạn bè
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int db_get_friends_list(int user_id, char*** results, int* count) {
    if (!conn || !results || !count) return -1;

    char uid[20];
    snprintf(uid, sizeof(uid), "%d", user_id);

    const char* params[1] = { uid };

    PGresult* res = PQexecParams(
        conn,
        "SELECT "
        "CASE "
        "  WHEN f.user1_id = $1 THEN u2.user_id "
        "  ELSE u1.user_id "
        "END AS friend_id, "
        "CASE "
        "  WHEN f.user1_id = $1 THEN u2.username "
        "  ELSE u1.username "
        "END AS username, "
        "CASE "
        "  WHEN f.user1_id = $1 THEN u2.email "
        "  ELSE u1.email "
        "END AS email "
        "FROM friendships f "
        "JOIN users u1 ON u1.user_id = f.user1_id "
        "JOIN users u2 ON u2.user_id = f.user2_id "
        "WHERE f.user1_id = $1 OR f.user2_id = $1 "
        "ORDER BY username",
        1, NULL, params, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_get_friends_list error: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    *count = PQntuples(res);
    *results = NULL;

    if (*count == 0) {
        PQclear(res);
        return 0;
    }

    *results = (char**)malloc(*count * sizeof(char*));
    if (!*results) {
        PQclear(res);
        return -1;
    }

    for (int i = 0; i < *count; i++) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%s|%s|%s",
                 PQgetvalue(res, i, 0), // friend_id
                 PQgetvalue(res, i, 1), // username
                 PQgetvalue(res, i, 2)  // email
        );
        (*results)[i] = strdup(buffer);
    }

    PQclear(res);
    return 0;
}


// Check if two users are friends
int db_check_friendship(int user_id1, int user_id2) {
    if (!conn) return 0;
    
    char user_id1_str[20], user_id2_str[20];
    snprintf(user_id1_str, sizeof(user_id1_str), "%d", user_id1);
    snprintf(user_id2_str, sizeof(user_id2_str), "%d", user_id2);
    
    const char* paramValues[2] = {user_id1_str, user_id2_str};
    
    PGresult* res = PQexecParams(conn,
        "SELECT 1 FROM friendships WHERE (user_id = $1 AND friend_id = $2) OR (user_id = $2 AND friend_id = $1)",
        2, NULL, paramValues, NULL, NULL, 0);
    
    int found = PQntuples(res) > 0;
    PQclear(res);
    
    return found;
}

/**
 * Tạo mới sự kiện
 * @param creator_id   ID user tạo sự kiện
 * @param event_name   Tên sự kiện  (map vào events.title)
 * @param description  Mô tả        (events.description)
 * @param location     Địa điểm     (events.location)
 * @param event_time   Thời gian    (events.event_time, format: YYYY-MM-DD HH:MM:SS)
 * @param event_type   'public' / 'private' (events.event_type)
 * @return event_id nếu OK, -1 nếu lỗi
 */
int db_create_event(int creator_id,const char* event_name,const char* description,
                    const char* location,const char* event_time,const char* event_type)
{
    if (!conn) return -1;

    // ép kiểu creator_id sang string
    char creator_id_str[20];
    snprintf(creator_id_str, sizeof(creator_id_str), "%d", creator_id);

    const char* params[6] = {
        creator_id_str,   // $1
        event_name,       // $2
        description,      // $3
        location,         // $4
        event_time,       // $5
        event_type        // $6
    };

    PGresult* res = PQexecParams(conn,
        "INSERT INTO events (creator_id, title, description, location, event_time, event_type) "
        "VALUES ($1, $2, $3, $4, $5, $6) "
        "RETURNING event_id",
        6, NULL, params, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_create_event error: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int event_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    // Trigger SQL đã tự thêm creator vào event_participants rồi
    //  không cần gọi db_join_event nữa

    // Ghi log 
    char details[256];
    snprintf(details, sizeof(details), "Created event: %s", event_name);
    db_log_activity(creator_id, "EVENT_CREATED", details);

    return event_id;
}


/**
 * Chức năng : Sửa thông tin sự kiện
 * - Update thông tin sự kiện trong database
 * @return 0 nếu thành công, -1 nếu lỗi
 */
// return: 1 = updated, 0 = not found, -1 = db error
int db_update_event(int creator_id, int event_id,const char* title,const char* description,
                    const char* location,const char* event_time,const char* event_type){
    if (!conn) return -1;

    char uid[20], eid[20];
    snprintf(uid, sizeof(uid), "%d", creator_id);
    snprintf(eid, sizeof(eid), "%d", event_id);

    const char* params[7] = {title,description,location,event_time,event_type,uid,eid};
    PGresult* res = PQexecParams(
        conn,
        "UPDATE events "
        "SET title=$1, description=$2, location=$3, event_time=$4, event_type=$5 "
        "WHERE creator_id=$6 AND event_id=$7",
        7, NULL, params, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "db_update_event error: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    // số dòng update
    int affected = atoi(PQcmdTuples(res));
    PQclear(res);

    return (affected > 0) ? 1 : 0;
}


/**
 * Chức năng : Xóa sự kiện
 * - Xóa sự kiện khỏi database
 * - Cascade delete sẽ tự động xóa các bản ghi liên quan (participants, invitations, requests)
 * @return 0 nếu thành công, -1 nếu lỗi
 */
// return: 1 = deleted, 0 = not found, -1 = db error
int db_delete_event(int user_id, int event_id) {
    if (!conn) return -1;

    char uid[20], eid[20];
    snprintf(uid, sizeof(uid), "%d", user_id);
    snprintf(eid, sizeof(eid), "%d", event_id);

    const char* params[2] = { uid, eid };

    PGresult* res = PQexecParams(
        conn,
        "DELETE FROM events WHERE creator_id = $1 AND event_id = $2",
        2, NULL, params, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "db_delete_event error: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int affected = atoi(PQcmdTuples(res)); // số dòng bị xóa
    PQclear(res);

    return (affected > 0) ? 1 : 0;
}


// Get event details
int db_get_event_details(int event_id, char*** results) {
    if (!conn) return -1;
    
    char event_id_str[20];
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);
    
    const char* paramValues[1] = {event_id_str};
    
    PGresult* res = PQexecParams(conn,
        "SELECT e.event_id, e.event_name, e.description, e.location, e.start_time, e.end_time, "
        "e.max_participants, e.current_participants, u.username as creator "
        "FROM events e "
        "JOIN users u ON e.creator_id = u.user_id "
        "WHERE e.event_id = $1",
        1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return -1;
    }
    
    *results = malloc(9 * sizeof(char*));
    for (int i = 0; i < 9; i++) {
        (*results)[i] = strdup(PQgetvalue(res, 0, i));
    }
    
    PQclear(res);
    return 0;
}

// Get user's events
/**
 * Lấy danh sách sự kiện mà user sở hữu hoặc tham gia
 * @param user_id  ID người dùng
 * @param results  Mảng string kết quả (mỗi phần tử là 1 event, cần free bằng db_free_results)
 * @param count    Số event
 */
int db_get_user_events(int user_id, char*** results, int* count) {
    if (!conn) return -1;

    char user_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);

    const char* params[1] = { user_id_str };

    PGresult* res = PQexecParams(
        conn,
        "SELECT DISTINCT e.event_id, e.title, e.location, e.event_time, e.event_type, e.status "
        "FROM events e "
        "LEFT JOIN event_participants ep ON e.event_id = ep.event_id "
        "WHERE e.creator_id = $1 OR ep.user_id = $1 "
        "ORDER BY e.event_time",
        1, NULL, params, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_get_user_events error: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    *count = PQntuples(res);
    *results = NULL;

    if (*count == 0) {
        PQclear(res);
        return 0;
    }

    *results = (char**)malloc(*count * sizeof(char*));
    if (!*results) {
        PQclear(res);
        return -1;
    }

    for (int i = 0; i < *count; i++) {
        char buffer[512];

        // format 1 dòng: event_id;title;location;time;type;status
        snprintf(buffer, sizeof(buffer), "%s;%s;%s;%s;%s;%s",
                 PQgetvalue(res, i, 0), // event_id
                 PQgetvalue(res, i, 1), // title
                 PQgetvalue(res, i, 2), // location
                 PQgetvalue(res, i, 3), // event_time
                 PQgetvalue(res, i, 4), // event_type
                 PQgetvalue(res, i, 5)  // status
        );

        (*results)[i] = strdup(buffer);
    }

    PQclear(res);
    return 0;
}


/**
 * Chức năng : Lấy danh sách tất cả sự kiện
 * - Query tất cả events từ database
 * - Join với users để lấy tên creator
 * - Sắp xếp theo thời gian bắt đầu
 * @param results - Mảng chứa kết quả (cần free bằng db_free_results() sau khi dùng)
 * @param count - Số lượng sự kiện tìm được
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int db_get_all_events(char*** results, int* count) {
    if (!conn) return -1;
    
    PGresult* res = PQexec(conn,
        "SELECT e.event_id, e.title, e.location, e.event_time, "
        "       e.event_type, e.status, u.username "
        "FROM events e "
        "JOIN users u ON e.creator_id = u.user_id "
        "ORDER BY e.event_time"
    );
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[DB] Get all events failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    
    *count = PQntuples(res);
    if (*count == 0) {
        *results = NULL;
        PQclear(res);
        return 0;
    }
    
    *results = malloc(*count * sizeof(char*));
    if (!*results) {
        PQclear(res);
        return -1;
    }
    
    for (int i = 0; i < *count; i++) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%s|%s|%s|%s|%s|%s|%s",
                 PQgetvalue(res, i, 0), // event_id
                 PQgetvalue(res, i, 1), // title
                 PQgetvalue(res, i, 2), // location
                 PQgetvalue(res, i, 3), // event_time
                 PQgetvalue(res, i, 4), // event_type
                 PQgetvalue(res, i, 5), // status
                 PQgetvalue(res, i, 6)  // creator username
        );
        (*results)[i] = strdup(buffer);
    }
    
    PQclear(res);
    return 0;
}
// return: 1 = found, 0 = not found, -1 = db error
int db_get_event_detail_by_creator(int user_id, int event_id, char** out_extra) {
    if (!conn || !out_extra) return -1;
    *out_extra = NULL;

    char uid[20], eid[20];
    snprintf(uid, sizeof(uid), "%d", user_id);
    snprintf(eid, sizeof(eid), "%d", event_id);

    //tham số truyền cho PQexecParams:$1 = uid, $2 = eid
    const char* params[2] = { uid, eid };

    PGresult* res = PQexecParams(conn,
        "SELECT event_id, title, COALESCE(description,''), COALESCE(location,''), "
        "       event_time::text, event_type, status "
        "FROM events "
        "WHERE creator_id = $1 AND event_id = $2",
        2,NULL,params,NULL, NULL,0);

    //  PGRES_TUPLES_OK : thành công và trả về rows
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_get_event_detail_by_creator error: %s\n", PQerrorMessage(conn));
        PQclear(res); // giải phóng PGresult để tránh memory leak
        return -1;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        return 0;
    }

    //Lấy giá trị từng cột ở row 0
    const char* c0 = PQgetvalue(res, 0, 0); //event_id
    const char* c1 = PQgetvalue(res, 0, 1); //title
    const char* c2 = PQgetvalue(res, 0, 2); //description
    const char* c3 = PQgetvalue(res, 0, 3); //location
    const char* c4 = PQgetvalue(res, 0, 4); //event_time:text
    const char* c5 = PQgetvalue(res, 0, 5); //event_type
    const char* c6 = PQgetvalue(res, 0, 6); //status

    int n = snprintf(NULL, 0, "%s|%s|%s|%s|%s|%s|%s", c0,c1,c2,c3,c4,c5,c6);

    char* extra = (char*)malloc((size_t)n + 1);
    if (!extra) {
        PQclear(res);
        return -1;
    }

    //ghi dữ liệu vào extra theo format event_id|title|description|location|event_time|event_type|status
    snprintf(extra, (size_t)n + 1, "%s|%s|%s|%s|%s|%s|%s", c0,c1,c2,c3,c4,c5,c6);

    PQclear(res);
    *out_extra = extra;
    return 1;
}

// Search events
int db_search_events(const char* keyword, char*** results, int* count) {
    if (!conn) return -1;
    
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%%%s%%", keyword);
    
    const char* paramValues[1] = {pattern};
    
    PGresult* res = PQexecParams(conn,
        "SELECT e.event_id, e.event_name, e.location, e.start_time, e.current_participants, e.max_participants "
        "FROM events e "
        "WHERE e.event_name ILIKE $1 OR e.description ILIKE $1 OR e.location ILIKE $1 "
        "ORDER BY e.start_time",
        1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }
    
    *count = PQntuples(res);
    *results = malloc(*count * sizeof(char*));
    
    for (int i = 0; i < *count; i++) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%s|%s|%s|%s|%s/%s",
                 PQgetvalue(res, i, 0), // event_id
                 PQgetvalue(res, i, 1), // event_name
                 PQgetvalue(res, i, 2), // location
                 PQgetvalue(res, i, 3), // start_time
                 PQgetvalue(res, i, 4), // current_participants
                 PQgetvalue(res, i, 5)); // max_participants
        
        (*results)[i] = strdup(buffer);
    }
    
    PQclear(res);
    return 0;
}

// Join event
int db_join_event(int user_id, int event_id) {
    if (!conn) return -1;
    
    char user_id_str[20], event_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);
    
    const char* paramValues[2] = {user_id_str, event_id_str};
    
    PGresult* res = PQexecParams(conn,
        "INSERT INTO event_participants (user_id, event_id) VALUES ($1, $2) RETURNING participant_id",
        2, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char* sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
        PQclear(res);
        
        if (sqlstate && strcmp(sqlstate, "23505") == 0) {
            return -2; // Already joined
        }
        return -1;
    }
    
    PQclear(res);
    
    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "Joined event %d", event_id);
    db_log_activity(user_id, "EVENT_JOINED", details);
    
    return 0;
}

// Leave event
int db_leave_event(int user_id, int event_id) {
    if (!conn) return -1;
    
    char user_id_str[20], event_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);
    
    const char* paramValues[2] = {user_id_str, event_id_str};
    
    PGresult* res = PQexecParams(conn,
        "UPDATE event_participants SET status = 'left' WHERE user_id = $1 AND event_id = $2",
        2, NULL, paramValues, NULL, NULL, 0);
    
    int success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    // Log activity
    char details[256];
    snprintf(details, sizeof(details), "Left event %d", event_id);
    db_log_activity(user_id, "EVENT_LEFT", details);
    
    return success ? 0 : -1;
}

// Get event participants
int db_get_event_participants(int event_id, char*** results, int* count) {
    if (!conn) return -1;
    
    char event_id_str[20];
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);
    
    const char* paramValues[1] = {event_id_str};
    
    PGresult* res = PQexecParams(conn,
        "SELECT u.user_id, u.username, ep.joined_at "
        "FROM event_participants ep "
        "JOIN users u ON ep.user_id = u.user_id "
        "WHERE ep.event_id = $1 AND ep.status = 'joined' "
        "ORDER BY ep.joined_at",
        1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }
    
    *count = PQntuples(res);
    *results = malloc(*count * sizeof(char*));
    
    for (int i = 0; i < *count; i++) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%s|%s|%s",
                 PQgetvalue(res, i, 0), // user_id
                 PQgetvalue(res, i, 1), // username
                 PQgetvalue(res, i, 2)); // joined_at
        
        (*results)[i] = strdup(buffer);
    }
    
    PQclear(res);
    return 0;
}

// Check if user is participant
int db_check_participant(int user_id, int event_id) {
    if (!conn) return 0;
    
    char user_id_str[20], event_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);
    
    const char* paramValues[2] = {user_id_str, event_id_str};
    
    PGresult* res = PQexecParams(conn,
        "SELECT 1 FROM event_participants WHERE user_id = $1 AND event_id = $2 AND status = 'joined'",
        2, NULL, paramValues, NULL, NULL, 0);
    
    int found = PQntuples(res) > 0;
    PQclear(res);
    
    return found;
}

/**
 * Gửi lời mời tham gia sự kiện
 * @return invitation_id nếu OK
 *         -1  DB error
 *         -2  Event không tồn tại / không active
 *         -3  User nhận không tồn tại / inactive
 *         -4  Không có quyền mời (không phải creator)
 *         -5  Đã có invitation pending
 *         -6  Người nhận đã tham gia event
 *         -7  Không thể tự mời chính mình
 */
int db_send_event_invitation(int event_id, int sender_id, int receiver_id) {
    if (!conn) return -1;
    if (sender_id == receiver_id) return -7;

    char eid[20], sid[20], rid[20];
    snprintf(eid, sizeof(eid), "%d", event_id);
    snprintf(sid, sizeof(sid), "%d", sender_id);
    snprintf(rid, sizeof(rid), "%d", receiver_id);

    PGresult* res;

    /* 1. Check event tồn tại + active + quyền */
    const char* p_event[1] = { eid };
    res = PQexecParams(
        conn,
        "SELECT creator_id, status FROM events WHERE event_id = $1",
        1, NULL, p_event, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return -2;
    }

    int creator_id = atoi(PQgetvalue(res, 0, 0));
    const char* ev_status = PQgetvalue(res, 0, 1);
    PQclear(res);

    if (strcmp(ev_status, "active") != 0) return -2;
    if (creator_id != sender_id) return -4;

    /* 2. Check receiver tồn tại */
    const char* p_user[1] = { rid };
    res = PQexecParams(
        conn,
        "SELECT 1 FROM users WHERE user_id = $1 AND status = 'active'",
        1, NULL, p_user, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return -3;
    }
    PQclear(res);

    /* 3. Check receiver đã tham gia event chưa */
    const char* p_joined[2] = { eid, rid };
    res = PQexecParams(
        conn,
        "SELECT 1 FROM event_participants WHERE event_id=$1 AND user_id=$2",
        2, NULL, p_joined, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }
    if (PQntuples(res) > 0) {
        PQclear(res);
        return -6;
    }
    PQclear(res);

    /* 4. Check invitation pending đã tồn tại */
    const char* p_pending[3] = { eid, sid, rid };
    res = PQexecParams(
        conn,
        "SELECT invitation_id FROM event_invitations "
        "WHERE event_id=$1 AND sender_id=$2 AND receiver_id=$3 AND status='pending'",
        3, NULL, p_pending, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }
    if (PQntuples(res) > 0) {
        PQclear(res);
        return -5;
    }
    PQclear(res);

    /* 5. Insert invitation */
    res = PQexecParams(
        conn,
        "INSERT INTO event_invitations (event_id, sender_id, receiver_id, status) "
        "VALUES ($1,$2,$3,'pending') RETURNING invitation_id",
        3, NULL, p_pending, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    int invitation_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    /* 6. Log */
    char detail[256];
    snprintf(detail, sizeof(detail),
             "Sent event invitation to user %d for event %d",
             receiver_id, event_id);
    db_log_activity(sender_id, "EVENT_INVITATION_SENT", detail);

    return invitation_id;
}


/**
 * Chấp nhận lời mời tham gia sự kiện
 * @param invitee_id        ID người nhận lời mời
 * @param inviter_username Tên người gửi lời mời
 * @return 0 nếu thành công
 *        -1 lỗi DB
 *        -2 không tìm thấy lời mời pending
 */
// return: 0 success, -2 not found, -1 db error
int db_accept_event_invitation(int receiver_id, const char* sender_username) {
    if (!conn || !sender_username || sender_username[0] == '\0') return -1;

    char receiver_id_str[20];
    snprintf(receiver_id_str, sizeof(receiver_id_str), "%d", receiver_id);

    // BEGIN transaction
    PGresult* res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }
    PQclear(res);

    // 1) Find pending invitation by (receiver_id + sender_username)
    const char* params1[2] = { sender_username, receiver_id_str };

    res = PQexecParams(
        conn,
        "SELECT ei.invitation_id, ei.event_id "
        "FROM event_invitations ei "
        "JOIN users u ON ei.sender_id = u.user_id "
        "WHERE u.username = $1 "
        "  AND ei.receiver_id = $2 "
        "  AND ei.status = 'pending' "
        "ORDER BY ei.created_at DESC "
        "LIMIT 1",
        2, NULL, params1, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_accept_event_invitation query error: %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -2; // not found pending invitation
    }

    int invitation_id = atoi(PQgetvalue(res, 0, 0));
    int event_id      = atoi(PQgetvalue(res, 0, 1));
    PQclear(res);

    // 2) Update invitation -> accepted
    char invitation_id_str[20];
    snprintf(invitation_id_str, sizeof(invitation_id_str), "%d", invitation_id);
    const char* params2[1] = { invitation_id_str };

    res = PQexecParams(
        conn,
        "UPDATE event_invitations "
        "SET status = 'accepted', responded_at = CURRENT_TIMESTAMP "
        "WHERE invitation_id = $1 AND status = 'pending'",
        1, NULL, params2, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "db_accept_event_invitation update error: %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    PQclear(res);

    // 3) Add receiver to participants (db_join_event should handle duplicate safely if possible)
    if (db_join_event(receiver_id, event_id) < 0) {
        PQexec(conn, "ROLLBACK");
        return -1;
    }

    // COMMIT
    res = PQexec(conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    PQclear(res);

    return 0;
}



// Reject event invitation
int db_reject_event_invitation(int invitation_id) {
    if (!conn) return -1;
    
    char invitation_id_str[20];
    snprintf(invitation_id_str, sizeof(invitation_id_str), "%d", invitation_id);
    
    const char* paramValues[1] = {invitation_id_str};
    
    PGresult* res = PQexecParams(conn,
        "UPDATE event_invitations SET status = 'rejected' WHERE invitation_id = $1 AND status = 'pending'",
        1, NULL, paramValues, NULL, NULL, 0);
    
    int success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    return success ? 0 : -1;
}

// Get user invitations
int db_get_user_invitations(int user_id, char*** results, int* count) {
    if (!conn) return -1;
    
    char user_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    
    const char* paramValues[1] = {user_id_str};
    
    PGresult* res = PQexecParams(conn,
        "SELECT ei.invitation_id, e.event_name, u.username as inviter, ei.sent_at "
        "FROM event_invitations ei "
        "JOIN events e ON ei.event_id = e.event_id "
        "JOIN users u ON ei.inviter_id = u.user_id "
        "WHERE ei.invitee_id = $1 AND ei.status = 'pending' "
        "ORDER BY ei.sent_at DESC",
        1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }
    
    *count = PQntuples(res);
    *results = malloc(*count * sizeof(char*));
    
    for (int i = 0; i < *count; i++) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%s|%s|%s|%s",
                 PQgetvalue(res, i, 0), // invitation_id
                 PQgetvalue(res, i, 1), // event_name
                 PQgetvalue(res, i, 2), // inviter username
                 PQgetvalue(res, i, 3)); // sent_at
        
        (*results)[i] = strdup(buffer);
    }
    
    PQclear(res);
    return 0;
}

/**
 * Chức năng: Tạo yêu cầu tham gia sự kiện public
 * - User gửi request để tham gia sự kiện
 * - Status mặc định là 'pending' (theo DB default)
 * - Cần admin/creator approve mới được tham gia
 *
 * @return:
 *  >0  = request_id (tạo request thành công)
 *   0  = đã join rồi (đã có trong event_participants)
 *  -1  = lỗi database
 *  -2  = event không tồn tại / không active
 *  -3  = event private (không được join trực tiếp)
 */
int db_create_join_request(int user_id, int event_id) {
    if (!conn) return -1;
    if (user_id <= 0 || event_id <= 0) return -1;

    char user_id_str[20], event_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);  
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);

    /* =========================
       1) Check event exists + active + type
       ========================= */
    const char* p_event[1] = { event_id_str };
    PGresult* res = PQexecParams(
        conn,
        "SELECT event_type FROM events "
        "WHERE event_id = $1 AND status = 'active'",
        1, NULL, p_event, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_create_join_request check event error: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        return -2; // event not found or not active
    }

    const char* event_type = PQgetvalue(res, 0, 0);
    PQclear(res);

    if (event_type && strcmp(event_type, "private") == 0) {
        return -3; // private => cannot join directly
    }

    /* =========================
       2) Check already joined?
       ========================= */
    const char* p_join[2] = { event_id_str, user_id_str };
    res = PQexecParams(
        conn,
        "SELECT 1 FROM event_participants "
        "WHERE event_id = $1 AND user_id = $2",
        2, NULL, p_join, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_create_join_request check participant error: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) > 0) {
        PQclear(res);
        return 0; // already joined
    }
    PQclear(res);

    /* =========================
       3) Insert join request -> RETURNING request_id
       ========================= */
    const char* paramValues[2] = { user_id_str, event_id_str };
    res = PQexecParams(
        conn,
        "INSERT INTO event_join_requests (user_id, event_id) "
        "VALUES ($1, $2) RETURNING join_request_id",
        2, NULL, paramValues, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_create_join_request insert error: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int request_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    return request_id;
}


/**
 * Chức năng: Creator chấp nhận yêu cầu tham gia sự kiện (join request)
 * Input:
 *  - creator_id   : user_id của người tạo sự kiện (người approve)
 *  - event_id     : event_id của sự kiện
 *  - join_username: username của người gửi request tham gia
 *
 * Return:
 *  0  = thành công
 * -1  = lỗi DB / transaction
 * -2  = không có request pending tương ứng
 * -3  = event không tồn tại hoặc không thuộc creator_id
 * -4  = join_username không tồn tại / không active
 */
int db_approve_join_request_by_creator(int creator_id, int event_id, const char* join_username)
{
    PGconn *conn = db_get_connection();
    PGresult *res = NULL;

    int join_user_id = -1;
    int join_request_id = -1;

    if (!conn || PQstatus(conn) != CONNECTION_OK) {
        return -1;
    }

    /* BEGIN TRANSACTION */
    res = PQexec(conn, "BEGIN");
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) goto db_error;
    PQclear(res);

    /* 1. Check event exists & belongs to creator */
    {
        const char *params[2];
        char ev_buf[16], cr_buf[16];
        snprintf(ev_buf, sizeof(ev_buf), "%d", event_id);
        snprintf(cr_buf, sizeof(cr_buf), "%d", creator_id);
        params[0] = ev_buf;
        params[1] = cr_buf;

        res = PQexecParams(
            conn,
            "SELECT 1 FROM events "
            "WHERE event_id = $1 AND creator_id = $2 AND status = 'active'",
            2, NULL, params, NULL, NULL, 0
        );

        if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) goto db_error;
        if (PQntuples(res) == 0) {
            PQclear(res);
            PQexec(conn, "ROLLBACK");
            return -3;
        }
        PQclear(res);
    }

    /* 2. Get join user_id by username */
    {
        const char *params[1] = { join_username };

        res = PQexecParams(
            conn,
            "SELECT user_id FROM users "
            "WHERE username = $1 AND status = 'active'",
            1, NULL, params, NULL, NULL, 0
        );

        if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) goto db_error;
        if (PQntuples(res) == 0) {
            PQclear(res);
            PQexec(conn, "ROLLBACK");
            return -4;
        }

        join_user_id = atoi(PQgetvalue(res, 0, 0));
        PQclear(res);
    }

    /* 3. Lock pending join request */
    {
        const char *params[2];
        char ev_buf[16], uid_buf[16];
        snprintf(ev_buf, sizeof(ev_buf), "%d", event_id);
        snprintf(uid_buf, sizeof(uid_buf), "%d", join_user_id);
        params[0] = ev_buf;
        params[1] = uid_buf;

        res = PQexecParams(
            conn,
            "SELECT join_request_id "
            "FROM event_join_requests "
            "WHERE event_id = $1 AND user_id = $2 AND status = 'pending' "
            "FOR UPDATE",
            2, NULL, params, NULL, NULL, 0
        );

        if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) goto db_error;
        if (PQntuples(res) == 0) {
            PQclear(res);
            PQexec(conn, "ROLLBACK");
            return -2;
        }

        join_request_id = atoi(PQgetvalue(res, 0, 0));
        PQclear(res);
    }

    /* 4. Update join request -> accepted */
    {
        const char *params[1];
        char rid_buf[16];
        snprintf(rid_buf, sizeof(rid_buf), "%d", join_request_id);
        params[0] = rid_buf;

        res = PQexecParams(
            conn,
            "UPDATE event_join_requests "
            "SET status = 'accepted', responded_at = CURRENT_TIMESTAMP "
            "WHERE join_request_id = $1",
            1, NULL, params, NULL, NULL, 0
        );

        if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) goto db_error;
        PQclear(res);
    }

    /* 5. Insert participant (FIX: role = 'participant') */
    {
        const char *params[2];
        char ev_buf[16], uid_buf[16];
        snprintf(ev_buf, sizeof(ev_buf), "%d", event_id);
        snprintf(uid_buf, sizeof(uid_buf), "%d", join_user_id);
        params[0] = ev_buf;
        params[1] = uid_buf;

        res = PQexecParams(
            conn,
            "INSERT INTO event_participants (event_id, user_id, role) "
            "VALUES ($1, $2, 'participant') "
            "ON CONFLICT (event_id, user_id) DO NOTHING",
            2, NULL, params, NULL, NULL, 0
        );

        if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) goto db_error;
        PQclear(res);
    }

    /* COMMIT */
    res = PQexec(conn, "COMMIT");
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) goto db_error;
    PQclear(res);

    return 0;

/* ================= ERROR HANDLING ================= */

db_error:
    if (res) PQclear(res);
    PQexec(conn, "ROLLBACK");
    return -1;
}




// Reject join request
int db_reject_join_request(int request_id) {
    if (!conn) return -1;
    
    char request_id_str[20];
    snprintf(request_id_str, sizeof(request_id_str), "%d", request_id);
    
    const char* paramValues[1] = {request_id_str};
    
    PGresult* res = PQexecParams(conn,
        "UPDATE event_join_requests SET status = 'rejected' WHERE join_request_id = $1 AND status = 'pending'",
        1, NULL, paramValues, NULL, NULL, 0);
    
    int success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    return success ? 0 : -1;
}

// Get event join requests
int db_get_event_join_requests(int event_id, char*** results, int* count) {
    if (!conn) return -1;
    
    char event_id_str[20];
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);
    
    const char* paramValues[1] = {event_id_str};
    
    PGresult* res = PQexecParams(conn,
        "SELECT jr.request_id, u.username, jr.requested_at "
        "FROM event_join_requests jr "
        "JOIN users u ON jr.user_id = u.user_id "
        "WHERE jr.event_id = $1 AND jr.status = 'pending' "
        "ORDER BY jr.requested_at",
        1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }
    
    *count = PQntuples(res);
    *results = malloc(*count * sizeof(char*));
    
    for (int i = 0; i < *count; i++) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%s|%s|%s",
                 PQgetvalue(res, i, 0), // request_id
                 PQgetvalue(res, i, 1), // username
                 PQgetvalue(res, i, 2)); // requested_at
        
        (*results)[i] = strdup(buffer);
    }
    
    PQclear(res);
    return 0;
}

/**
 * Chức năng : Ghi log hoạt động của user
 * - Insert log vào bảng activity_logs
 * - Timestamp được tự động thêm bởi database (DEFAULT CURRENT_TIMESTAMP)
 * - Các activity_type thường dùng:
 *   + USER_REGISTERED, USER_LOGIN, USER_LOGOUT
 *   + FRIEND_REQUEST_SENT, FRIEND_REQUEST_ACCEPTED, FRIEND_REMOVED
 *   + EVENT_CREATED, EVENT_JOINED, EVENT_LEFT
 *   + INVITATION_SENT, INVITATION_ACCEPTED
 * @param user_id - ID của user thực hiện hành động
 * @param activity_type - Loại hoạt động (tối đa 50 ký tự)
 * @param details - Chi tiết về hoạt động
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int db_log_activity(int user_id, const char* activity_type, const char* details) {
    if (!conn) return -1;
    
    char user_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    
    const char* paramValues[3] = {user_id_str, activity_type, details};
    
    // INSERT log vào database
    PGresult* res = PQexecParams(conn,
        "INSERT INTO activity_logs (user_id, activity_type, details) VALUES ($1, $2, $3)",
        3, NULL, paramValues, NULL, NULL, 0);
    
    int success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    return success ? 0 : -1;
}

// Get user activities
int db_get_user_activities(int user_id, int limit, char*** results, int* count) {
    if (!conn) return -1;
    
    char user_id_str[20], limit_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    snprintf(limit_str, sizeof(limit_str), "%d", limit);
    
    const char* paramValues[2] = {user_id_str, limit_str};
    
    PGresult* res = PQexecParams(conn,
        "SELECT activity_type, details, activity_time "
        "FROM activity_logs "
        "WHERE user_id = $1 "
        "ORDER BY activity_time DESC "
        "LIMIT $2",
        2, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }
    
    *count = PQntuples(res);
    *results = malloc(*count * sizeof(char*));
    
    for (int i = 0; i < *count; i++) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%s|%s|%s",
                 PQgetvalue(res, i, 0), // activity_type
                 PQgetvalue(res, i, 1), // details
                 PQgetvalue(res, i, 2)); // activity_time
        
        (*results)[i] = strdup(buffer);
    }
    
    PQclear(res);
    return 0;
}

// Free results array
void db_free_results(char*** results, int count) {
    if (!results || !*results) return;
    
    for (int i = 0; i < count; i++) {
        free((*results)[i]);
    }
    free(*results);
    *results = NULL;
}
