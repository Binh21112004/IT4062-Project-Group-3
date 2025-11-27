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

// User structure
typedef struct {
    int user_id;
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char email[MAX_EMAIL];
    int is_active;
} User;

// File database functions
void db_init();
int db_create_user(const char* username, const char* password, const char* email);
User* db_find_user_by_username(const char* username);
User* db_find_user_by_id(int user_id);
int db_verify_password(const char* username, const char* password);
int db_validate_username(const char* username);
void db_cleanup();

#endif // FILE_DB_H
