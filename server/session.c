#include "session.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// Mutex for thread-safe session operations
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
        session_cleanup_expired(sm);
    }
    
    // Check if user already has a session
    Session* existing = session_find_by_user_id(sm, user_id);
    if (existing != NULL) {
        // Update existing session
        existing->client_socket = client_socket;
        existing->last_activity = time(NULL);
        existing->is_active = 1;
        pthread_mutex_unlock(&session_mutex);
        return existing->token;
    }
    
    // Create new session
    Session* new_session = &sm->sessions[sm->session_count];
    generate_token(new_session->token);
    new_session->user_id = user_id;
    new_session->client_socket = client_socket;
    new_session->created_at = time(NULL);
    new_session->last_activity = time(NULL);
    new_session->is_active = 1;
    
    sm->session_count++;
    
    pthread_mutex_unlock(&session_mutex);
    return new_session->token;
}

// Find session by token
Session* session_find_by_token(SessionManager* sm, const char* token) {
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].is_active && strcmp(sm->sessions[i].token, token) == 0) {
            return &sm->sessions[i];
        }
    }
    return NULL;
}

// Find session by user ID
Session* session_find_by_user_id(SessionManager* sm, int user_id) {
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].is_active && sm->sessions[i].user_id == user_id) {
            return &sm->sessions[i];
        }
    }
    return NULL;
}

// Find session by socket
Session* session_find_by_socket(SessionManager* sm, int client_socket) {
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].is_active && sm->sessions[i].client_socket == client_socket) {
            return &sm->sessions[i];
        }
    }
    return NULL;
}

// Destroy session
void session_destroy(SessionManager* sm, const char* token) {
    Session* session = session_find_by_token(sm, token);
    if (session != NULL) {
        session->is_active = 0;
    }
}

// Cleanup expired sessions
void session_cleanup_expired(SessionManager* sm) {
    time_t now = time(NULL);
    
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].is_active) {
            if (now - sm->sessions[i].last_activity > SESSION_TIMEOUT) {
                sm->sessions[i].is_active = 0;
            }
        }
    }
}

// Validate session and update last activity
int session_validate(SessionManager* sm, const char* token) {
    Session* session = session_find_by_token(sm, token);
    if (session == NULL) {
        return 0;
    }
    
    time_t now = time(NULL);
    if (now - session->last_activity > SESSION_TIMEOUT) {
        session->is_active = 0;
        return 0;
    }
    
    session->last_activity = now;
    return 1;
}
