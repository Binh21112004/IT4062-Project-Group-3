#include "file_db.h"
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

static User* current_user = NULL;

// Initialize database (create data directory and files)
void db_init() {
    mkdir("data", 0755);
    
    // Create users file if it doesn't exist
    FILE* f = fopen(USERS_FILE, "a");
    if (f) fclose(f);
}


void db_cleanup() {
    if (current_user) {
        free(current_user);
        current_user = NULL;
    }
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
            c == '^' || c == '&' || c == '*' || c == '(' || c == ')') {
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

// Get next user ID
static int get_next_user_id() {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) return 1;
    
    int max_id = 0;
    int id;
    char username[MAX_USERNAME], password[MAX_PASSWORD], email[MAX_EMAIL];
    int is_active;
    
    while (fscanf(f, "%d|%[^|]|%[^|]|%[^|]|%d\n", &id, username, password, email, &is_active) == 5) {
        if (id > max_id) max_id = id;
    }
    
    fclose(f);
    return max_id + 1;
}

// Create new user
int db_create_user(const char* username, const char* password, const char* email) {
    // Validate username
    if (!db_validate_username(username)) {
        return -3; // Invalid username
    }
    
    // Validate email
    if (!db_validate_email(email)) {
        return -4; // Invalid email format
    }
    
    // Check if username already exists
    if (db_find_user_by_username(username) != NULL) {
        return -2; // Username exists
    }
    
    int user_id = get_next_user_id();
    
    FILE* f = fopen(USERS_FILE, "a");
    if (!f) return -1;
    
    fprintf(f, "%d|%s|%s|%s|%d\n", user_id, username, password, email, 1);
    fclose(f);
    
    return user_id;
}

// Find user by username
User* db_find_user_by_username(const char* username) {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) return NULL;
    
    if (current_user) {
        free(current_user);
    }
    current_user = (User*)malloc(sizeof(User));
    
    while (fscanf(f, "%d|%[^|]|%[^|]|%[^|]|%d\n", 
                  &current_user->user_id, 
                  current_user->username, 
                  current_user->password, 
                  current_user->email, 
                  &current_user->is_active) == 5) {
        if (strcmp(current_user->username, username) == 0) {
            fclose(f);
            return current_user;
        }
    }
    
    fclose(f);
    free(current_user);
    current_user = NULL;
    return NULL;
}

// Find user by ID
User* db_find_user_by_id(int user_id) {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) return NULL;
    
    if (current_user) {
        free(current_user);
    }
    current_user = (User*)malloc(sizeof(User));
    
    while (fscanf(f, "%d|%[^|]|%[^|]|%[^|]|%d\n", 
                  &current_user->user_id, 
                  current_user->username, 
                  current_user->password, 
                  current_user->email, 
                  &current_user->is_active) == 5) {
        if (current_user->user_id == user_id) {
            fclose(f);
            return current_user;
        }
    }
    
    fclose(f);
    free(current_user);
    current_user = NULL;
    return NULL;
}

// Verify password
int db_verify_password(const char* username, const char* password) {
    User* user = db_find_user_by_username(username);
    if (user == NULL) {
        return 0;
    }
    
    return strcmp(user->password, password) == 0;
}

