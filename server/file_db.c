#include "file_db.h"
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

static User* current_user = NULL;
static FriendRequest* current_request = NULL;

// Initialize database (create data directory and files)
void db_init() {
    mkdir("data", 0755);
    
    // Create files if they don't exist
    FILE* f = fopen(USERS_FILE, "a");
    if (f) fclose(f);
    
    f = fopen(FRIEND_REQUESTS_FILE, "a");
    if (f) fclose(f);
    
    f = fopen(FRIENDSHIPS_FILE, "a");
    if (f) fclose(f);
}

// Cleanup
void db_cleanup() {
    if (current_user) {
        free(current_user);
        current_user = NULL;
    }
    if (current_request) {
        free(current_request);
        current_request = NULL;
    }
}

// Validate username (no special characters except underscore)
int db_validate_username(const char* username) {
    if (username == NULL || strlen(username) == 0) {
        return 0;
    }
    
    for (int i = 0; username[i] != '\0'; i++) {
        if (!isalnum(username[i]) && username[i] != '_') {
            return 0;
        }
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

// Get next friend request ID
static int get_next_request_id() {
    FILE* f = fopen(FRIEND_REQUESTS_FILE, "r");
    if (!f) return 1;
    
    int max_id = 0;
    int id, from_id, to_id, status;
    long created_at;
    
    while (fscanf(f, "%d|%d|%d|%d|%ld\n", &id, &from_id, &to_id, &status, &created_at) == 5) {
        if (id > max_id) max_id = id;
    }
    
    fclose(f);
    return max_id + 1;
}

// Create friend request
int db_create_friend_request(int from_user_id, int to_user_id) {
    // Check if already friends
    if (db_are_friends(from_user_id, to_user_id)) {
        return -2; // Already friends
    }
    
    // Check if request already exists
    FILE* f = fopen(FRIEND_REQUESTS_FILE, "r");
    if (f) {
        int id, from_id, to_id, status;
        long created_at;
        
        while (fscanf(f, "%d|%d|%d|%d|%ld\n", &id, &from_id, &to_id, &status, &created_at) == 5) {
            if (from_id == from_user_id && to_id == to_user_id && status == 0) {
                fclose(f);
                return -3; // Request already pending
            }
        }
        fclose(f);
    }
    
    int request_id = get_next_request_id();
    
    f = fopen(FRIEND_REQUESTS_FILE, "a");
    if (!f) return -1;
    
    fprintf(f, "%d|%d|%d|%d|%ld\n", request_id, from_user_id, to_user_id, 0, time(NULL));
    fclose(f);
    
    return request_id;
}

// Find friend request
FriendRequest* db_find_friend_request(int request_id) {
    FILE* f = fopen(FRIEND_REQUESTS_FILE, "r");
    if (!f) return NULL;
    
    if (current_request) {
        free(current_request);
    }
    current_request = (FriendRequest*)malloc(sizeof(FriendRequest));
    
    long created_at;
    while (fscanf(f, "%d|%d|%d|%d|%ld\n", 
                  &current_request->request_id,
                  &current_request->from_user_id,
                  &current_request->to_user_id,
                  &current_request->status,
                  &created_at) == 5) {
        current_request->created_at = (time_t)created_at;
        if (current_request->request_id == request_id) {
            fclose(f);
            return current_request;
        }
    }
    
    fclose(f);
    free(current_request);
    current_request = NULL;
    return NULL;
}

// Update friend request status
static int update_request_status(int request_id, int new_status) {
    FILE* f = fopen(FRIEND_REQUESTS_FILE, "r");
    if (!f) return -1;
    
    FILE* temp = fopen("data/friend_requests.tmp", "w");
    if (!temp) {
        fclose(f);
        return -1;
    }
    
    int id, from_id, to_id, status;
    long created_at;
    
    while (fscanf(f, "%d|%d|%d|%d|%ld\n", &id, &from_id, &to_id, &status, &created_at) == 5) {
        if (id == request_id) {
            fprintf(temp, "%d|%d|%d|%d|%ld\n", id, from_id, to_id, new_status, created_at);
        } else {
            fprintf(temp, "%d|%d|%d|%d|%ld\n", id, from_id, to_id, status, created_at);
        }
    }
    
    fclose(f);
    fclose(temp);
    
    remove(FRIEND_REQUESTS_FILE);
    rename("data/friend_requests.tmp", FRIEND_REQUESTS_FILE);
    
    return 0;
}

// Accept friend request
int db_accept_friend_request(int request_id) {
    FriendRequest* req = db_find_friend_request(request_id);
    if (req == NULL || req->status != 0) {
        return -1;
    }
    
    update_request_status(request_id, 1);
    return db_create_friendship(req->from_user_id, req->to_user_id);
}

// Reject friend request
int db_reject_friend_request(int request_id) {
    FriendRequest* req = db_find_friend_request(request_id);
    if (req == NULL || req->status != 0) {
        return -1;
    }
    
    return update_request_status(request_id, 2);
}

// Create friendship
int db_create_friendship(int user1_id, int user2_id) {
    // Check if already friends
    if (db_are_friends(user1_id, user2_id)) {
        return -2;
    }
    
    int min_id = user1_id < user2_id ? user1_id : user2_id;
    int max_id = user1_id < user2_id ? user2_id : user1_id;
    
    FILE* f = fopen(FRIENDSHIPS_FILE, "a");
    if (!f) return -1;
    
    fprintf(f, "%d|%d|%ld\n", min_id, max_id, time(NULL));
    fclose(f);
    
    return 0;
}

// Remove friendship
int db_remove_friendship(int user1_id, int user2_id) {
    int min_id = user1_id < user2_id ? user1_id : user2_id;
    int max_id = user1_id < user2_id ? user2_id : user1_id;
    
    FILE* f = fopen(FRIENDSHIPS_FILE, "r");
    if (!f) return -1;
    
    FILE* temp = fopen("data/friendships.tmp", "w");
    if (!temp) {
        fclose(f);
        return -1;
    }
    
    int id1, id2;
    long created_at;
    int found = 0;
    
    while (fscanf(f, "%d|%d|%ld\n", &id1, &id2, &created_at) == 3) {
        if (id1 == min_id && id2 == max_id) {
            found = 1;
            continue; // Skip this line
        }
        fprintf(temp, "%d|%d|%ld\n", id1, id2, created_at);
    }
    
    fclose(f);
    fclose(temp);
    
    remove(FRIENDSHIPS_FILE);
    rename("data/friendships.tmp", FRIENDSHIPS_FILE);
    
    return found ? 0 : -1;
}

// Check if two users are friends
int db_are_friends(int user1_id, int user2_id) {
    int min_id = user1_id < user2_id ? user1_id : user2_id;
    int max_id = user1_id < user2_id ? user2_id : user1_id;
    
    FILE* f = fopen(FRIENDSHIPS_FILE, "r");
    if (!f) return 0;
    
    int id1, id2;
    long created_at;
    
    while (fscanf(f, "%d|%d|%ld\n", &id1, &id2, &created_at) == 3) {
        if (id1 == min_id && id2 == max_id) {
            fclose(f);
            return 1;
        }
    }
    
    fclose(f);
    return 0;
}
