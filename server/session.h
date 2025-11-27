#ifndef SESSION_H
#define SESSION_H

#include <time.h>
#include "file_db.h"

#define MAX_SESSIONS 1000
#define SESSION_TIMEOUT 3600 
#define MAX_TOKEN 64

typedef struct {
    char token[MAX_TOKEN];
    int user_id;
    int client_socket;
    time_t created_at;
    time_t last_activity;
    int is_active;
} Session;

typedef struct {
    Session sessions[MAX_SESSIONS];
    int session_count;
} SessionManager;

// Session functions
void session_init(SessionManager* sm);
char* session_create(SessionManager* sm, int user_id, int client_socket);
Session* session_find_by_token(SessionManager* sm, const char* token);
Session* session_find_by_user_id(SessionManager* sm, int user_id);
Session* session_find_by_socket(SessionManager* sm, int client_socket);
void session_destroy(SessionManager* sm, const char* token);
void session_cleanup_expired(SessionManager* sm);
int session_validate(SessionManager* sm, const char* token);
int session_is_user_logged_in(SessionManager* sm, int user_id, int exclude_socket);

#endif 
