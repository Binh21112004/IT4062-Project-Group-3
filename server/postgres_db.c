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
    
    return 0;
}


/**
 * Gửi lời mời tham gia sự kiện
 * @return invitation_id nếu OK
 *         -1  DB error
 *         -2  Event không tồn tại 
 *         -4  Không có quyền mời (không phải creator)
 *         -5  Đã có invitation pending
 *         -6  Người nhận đã tham gia event
 */
int db_send_event_invitation(int event_id, int sender_id, int receiver_id) {
    if (!conn) return -1;

    char eid[20], sid[20], rid[20];
    snprintf(eid, sizeof(eid), "%d", event_id);
    snprintf(sid, sizeof(sid), "%d", sender_id);
    snprintf(rid, sizeof(rid), "%d", receiver_id);

    PGresult* res;

    //Check event tồn tại + quyền 
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
    //Check receiver đã tham gia event chưa 
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

    //Check invitation pending đã tồn tại chưa
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

    //Insert invitation
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
    return invitation_id;
}


/**
 * Chấp nhận lời mời tham gia sự kiện (đúng theo event_id)
 * @param receiver_id: id người nhận lời mời
 * @param sender_username: tên người gửi lời mời
 * @param event_id: id sự kiện cần đồng ý tham gia
 *
 * @return:
 *   0 = thành công
 *  -1 = lỗi DB
 *  -2 = không tìm thấy lời mời pending phù hợp
 */
int db_accept_event_invitation(int receiver_id, const char* sender_username, int event_id) {
    if (!conn || !sender_username || sender_username[0] == '\0') return -1;
    if (receiver_id <= 0 || event_id <= 0) return -1;

    char receiver_id_str[20], event_id_str[20];
    snprintf(receiver_id_str, sizeof(receiver_id_str), "%d", receiver_id);
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);

    // BEGIN transaction
    PGresult* res = PQexec(conn, "BEGIN");
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return -1;
    }
    PQclear(res);

    // Find pending invitation by (sender_username + receiver_id + event_id), lock row
    const char* params1[3] = { sender_username, receiver_id_str, event_id_str };

    res = PQexecParams(
        conn,
        "SELECT ei.invitation_id "
        "FROM event_invitations ei "
        "JOIN users u ON ei.sender_id = u.user_id "
        "WHERE u.username = $1 "
        "  AND ei.receiver_id = $2 "
        "  AND ei.event_id = $3 "
        "  AND ei.status = 'pending' "
        "ORDER BY ei.created_at DESC "
        "LIMIT 1 "
        "FOR UPDATE",
        3, NULL, params1, NULL, NULL, 0
    );

    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_accept_event_invitation query error: %s\n", PQerrorMessage(conn));
        if (res) PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -2;
    }

    int invitation_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    // Update invitation -> accepted (ràng buộc thêm receiver_id + event_id cho chắc)
    char invitation_id_str[20];
    snprintf(invitation_id_str, sizeof(invitation_id_str), "%d", invitation_id);
    const char* params2[3] = { invitation_id_str, receiver_id_str, event_id_str };

    res = PQexecParams(
        conn,
        "UPDATE event_invitations "
        "SET status = 'accepted', responded_at = CURRENT_TIMESTAMP "
        "WHERE invitation_id = $1 "
        "  AND receiver_id = $2 "
        "  AND event_id = $3 "
        "  AND status = 'pending'",
        3, NULL, params2, NULL, NULL, 0
    );

    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "db_accept_event_invitation update error: %s\n", PQerrorMessage(conn));
        if (res) PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }

    // (Khuyến nghị) đảm bảo update thực sự đổi 1 dòng
    if (PQcmdTuples(res) && atoi(PQcmdTuples(res)) == 0) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -2;
    }
    PQclear(res);

    // Join event theo event_id truyền vào
    if (db_join_event(receiver_id, event_id) < 0) {
        PQexec(conn, "ROLLBACK");
        return -1;
    }

    res = PQexec(conn, "COMMIT");
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    PQclear(res);

    return 0;
}


/**
 * Chức năng: Tạo yêu cầu tham gia sự kiện private
 * - User gửi request để tham gia sự kiện private
 * - Status mặc định là 'pending'
 * - Cần creator accept mới được tham gia
 *
 * @return:
 *  >0 = join_request_id (tạo request thành công)
 *   0 = đã join rồi
 *  -1 = lỗi database
 *  -2 = event không tồn tại / không active
 *  -3 = event không phải private (public -> không cần request)
 *  -4 = đã có request pending
 */
int db_create_join_request(int user_id, int event_id) {
    if (!conn) return -1;
    if (user_id <= 0 || event_id <= 0) return -1;

    char user_id_str[20], event_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);

    //Check event exists + active + type
    const char* p_event[1] = { event_id_str };
    PGresult* res = PQexecParams(
        conn,
        "SELECT event_type FROM events "
        "WHERE event_id = $1 AND status = 'active'",
        1, NULL, p_event, NULL, NULL, 0
    );

    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_create_join_request check event error: %s\n", PQerrorMessage(conn));
        if (res) PQclear(res);
        return -1;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        return -2; // event not found or not active
    }

    const char* event_type = PQgetvalue(res, 0, 0);
    PQclear(res);

    //Chỉ cho phép tạo request nếu event là private
    if (!event_type || strcmp(event_type, "private") != 0) {
        return -3; // nếu không phải private 
    }

    //  Check already joined
    const char* p_join[2] = { event_id_str, user_id_str };
    res = PQexecParams(
        conn,
        "SELECT 1 FROM event_participants "
        "WHERE event_id = $1 AND user_id = $2",
        2, NULL, p_join, NULL, NULL, 0
    );

    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_create_join_request check participant error: %s\n", PQerrorMessage(conn));
        if (res) PQclear(res);
        return -1;
    }

    if (PQntuples(res) > 0) {
        PQclear(res);
        return 0; // already joined
    }
    PQclear(res);

    // Check already has pending request
    const char* p_req[2] = { user_id_str, event_id_str };
    res = PQexecParams(
        conn,
        "SELECT 1 FROM event_join_requests "
        "WHERE user_id = $1 AND event_id = $2 AND status = 'pending' "
        "LIMIT 1",
        2, NULL, p_req, NULL, NULL, 0
    );

    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_create_join_request check pending request error: %s\n", PQerrorMessage(conn));
        if (res) PQclear(res);
        return -1;
    }

    if (PQntuples(res) > 0) {
        PQclear(res);
        return -4; // already has pending request
    }
    PQclear(res);

    // Insert join request (status default 'pending' at DB level or set explicitly)
    const char* paramValues[2] = { user_id_str, event_id_str };
    res = PQexecParams(
        conn,
        "INSERT INTO event_join_requests (user_id, event_id, status, created_at) "
        "VALUES ($1, $2, 'pending', CURRENT_TIMESTAMP) "
        "RETURNING join_request_id",
        2, NULL, paramValues, NULL, NULL, 0
    );

    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "db_create_join_request insert error: %s\n", PQerrorMessage(conn));
        if (res) PQclear(res);
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
    if (!conn || !join_username || join_username[0] == '\0') return -1;

    char creator_id_str[20], event_id_str[20];
    snprintf(creator_id_str, sizeof(creator_id_str), "%d", creator_id);
    snprintf(event_id_str, sizeof(event_id_str), "%d", event_id);

    // BEGIN transaction
    PGresult* res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }
    PQclear(res);

    // Check event exists + belongs to creator + active
    const char* params_event[2] = { event_id_str, creator_id_str };
    res = PQexecParams(
        conn,
        "SELECT 1 FROM events "
        "WHERE event_id = $1 AND creator_id = $2 AND status = 'active'",
        2, NULL, params_event, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -3; // event not found
    }
    PQclear(res);

    // Get join_user_id by username (must be active)
    const char* params_user[1] = { join_username };
    res = PQexecParams(
        conn,
        "SELECT user_id FROM users WHERE username = $1 AND status = 'active'",
        1, NULL, params_user, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -4; // username not exist / not active
    }

    int join_user_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    // Find pending join request (lock to avoid double-approve)
    char join_user_id_str[20];
    snprintf(join_user_id_str, sizeof(join_user_id_str), "%d", join_user_id);

    const char* params_req[2] = { event_id_str, join_user_id_str };
    res = PQexecParams(
        conn,
        "SELECT join_request_id "
        "FROM event_join_requests "
        "WHERE event_id = $1 AND user_id = $2 AND status = 'pending' "
        "FOR UPDATE",
        2, NULL, params_req, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -2; // no pending request
    }

    int join_request_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    //Update join request -> accepted
    char join_request_id_str[20];
    snprintf(join_request_id_str, sizeof(join_request_id_str), "%d", join_request_id);

    const char* params_upd[1] = { join_request_id_str };
    res = PQexecParams(
        conn,
        "UPDATE event_join_requests "
        "SET status = 'accepted', responded_at = CURRENT_TIMESTAMP "
        "WHERE join_request_id = $1 AND status = 'pending'",
        1, NULL, params_upd, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    PQclear(res);

    //Insert participant (role phải đúng CHECK: 'creator' hoặc 'participant')
    res = PQexecParams(
        conn,
        "INSERT INTO event_participants (event_id, user_id, role) "
        "VALUES ($1, $2, 'participant') "
        "ON CONFLICT (event_id, user_id) DO NOTHING",
        2, NULL, params_req, NULL, NULL, 0
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    PQclear(res);

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


// Free results array
void db_free_results(char*** results, int count) {
    if (!results || !*results) return;
    
    for (int i = 0; i < count; i++) {
        free((*results)[i]);
    }
    free(*results);
    *results = NULL;
}
