#include "session.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>


static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

// Generate random token
static void generate_token(char* token) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int len = 32;
    
    for (int i = 0; i < len; i++) {
        int index = rand() % (sizeof(charset) - 1);
        token[i] = charset[index];
    }
    token[len] = '\0';
}

// Initialize session manager
void session_init(SessionManager* sm) {
    sm->session_count = 0;
    memset(sm->sessions, 0, sizeof(sm->sessions));
    srand(time(NULL));
}

// Create new session
char* session_create(SessionManager* sm, int user_id, int client_socket) {
    pthread_mutex_lock(&session_mutex);
    
    if (sm->session_count >= MAX_SESSIONS) {
        // Cleanup expired sessions 
        time_t now = time(NULL);
        for (int i = 0; i < sm->session_count; i++) {
            if (sm->sessions[i].is_active) {
                if (now - sm->sessions[i].last_activity > SESSION_TIMEOUT) {
                    sm->sessions[i].is_active = 0;
                }
            }
        }
    }
    

    int slot_index = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!sm->sessions[i].is_active) {
            slot_index = i;
            break;
        }
    }
    
    if (slot_index == -1) {
        pthread_mutex_unlock(&session_mutex);
        return NULL; 
    }
    
   
    if (slot_index >= sm->session_count) {
        sm->session_count = slot_index + 1;
    }
    
    // Create new session
    Session* new_session = &sm->sessions[slot_index];
    generate_token(new_session->token);
    new_session->user_id = user_id;
    new_session->client_socket = client_socket;
    new_session->created_at = time(NULL);
    new_session->last_activity = time(NULL);
    new_session->is_active = 1;
    
    pthread_mutex_unlock(&session_mutex);
    return new_session->token;
}

// Find session by token
Session* session_find_by_token(SessionManager* sm, const char* token) {
    pthread_mutex_lock(&session_mutex);
    Session* result = NULL;
    
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].is_active && strcmp(sm->sessions[i].token, token) == 0) {
            result = &sm->sessions[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&session_mutex);
    return result;
}

// Find session by user ID
Session* session_find_by_user_id(SessionManager* sm, int user_id) {
    pthread_mutex_lock(&session_mutex);
    Session* result = NULL;
    
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].is_active && sm->sessions[i].user_id == user_id) {
            result = &sm->sessions[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&session_mutex);
    return result;
}

// Find session by socket
Session* session_find_by_socket(SessionManager* sm, int client_socket) {
    pthread_mutex_lock(&session_mutex);
    Session* result = NULL;
    
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].is_active && sm->sessions[i].client_socket == client_socket) {
            result = &sm->sessions[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&session_mutex);
    return result;
}

// Destroy session
void session_destroy(SessionManager* sm, const char* token) {
    pthread_mutex_lock(&session_mutex);
    
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].is_active && strcmp(sm->sessions[i].token, token) == 0) {
            sm->sessions[i].is_active = 0;
            break;
        }
    }
    
    pthread_mutex_unlock(&session_mutex);
}

// Cleanup expired sessions
void session_cleanup_expired(SessionManager* sm) {
    pthread_mutex_lock(&session_mutex);
    time_t now = time(NULL);
    
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].is_active) {
            if (now - sm->sessions[i].last_activity > SESSION_TIMEOUT) {
                sm->sessions[i].is_active = 0;
            }
        }
    }
    
    pthread_mutex_unlock(&session_mutex);
}

// Validate session and update last activity
int session_validate(SessionManager* sm, const char* token) {
    pthread_mutex_lock(&session_mutex);
    Session* session = NULL;
    
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].is_active && strcmp(sm->sessions[i].token, token) == 0) {
            session = &sm->sessions[i];
            break;
        }
    }
    
    if (session == NULL) {
        pthread_mutex_unlock(&session_mutex);
        return 0;
    }
    
    time_t now = time(NULL);
    if (now - session->last_activity > SESSION_TIMEOUT) {
        session->is_active = 0;
        pthread_mutex_unlock(&session_mutex);
        return 0;
    }
    
    session->last_activity = now;
    pthread_mutex_unlock(&session_mutex);
    return 1;
}

// Check if user is already logged in (from different socket)
// Inspired by previous assignment's is_user_logged_in function
int session_is_user_logged_in(SessionManager* sm, int user_id, int exclude_socket) {
    pthread_mutex_lock(&session_mutex);
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sm->sessions[i].is_active &&
            sm->sessions[i].user_id == user_id &&
            sm->sessions[i].client_socket != exclude_socket) {
            pthread_mutex_unlock(&session_mutex);
            return 1;  // User already logged in on another client
        }
    }
    
    pthread_mutex_unlock(&session_mutex);
    return 0;  // User not logged in anywhere else
}
