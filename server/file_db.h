#ifndef FILE_DB_H
#define FILE_DB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_USERNAME 64
#define MAX_PASSWORD 128
#define MAX_EMAIL 128

#define USERS_FILE "data/users.txt"
#define FRIEND_REQUESTS_FILE "data/friend_requests.txt"
#define FRIENDSHIPS_FILE "data/friendships.txt"

// User structure
typedef struct {
    int user_id;
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char email[MAX_EMAIL];
    int is_active;
} User;

// Friend request structure
typedef struct {
    int request_id;
    int from_user_id;
    int to_user_id;
    int status; // 0 = pending, 1 = accepted, 2 = rejected
    time_t created_at;
} FriendRequest;

// Friendship structure
typedef struct {
    int user1_id;
    int user2_id;
    time_t created_at;
} Friendship;

// File database functions
void db_init();
int db_create_user(const char* username, const char* password, const char* email);
User* db_find_user_by_username(const char* username);
User* db_find_user_by_id(int user_id);
int db_verify_password(const char* username, const char* password);
int db_create_friend_request(int from_user_id, int to_user_id);
FriendRequest* db_find_friend_request(int request_id);
int db_accept_friend_request(int request_id);
int db_reject_friend_request(int request_id);
int db_create_friendship(int user1_id, int user2_id);
int db_remove_friendship(int user1_id, int user2_id);
int db_are_friends(int user1_id, int user2_id);
int db_validate_username(const char* username);
void db_cleanup();

#endif // FILE_DB_H
