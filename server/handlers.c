#include "server.h"
#include "../common/cJSON.h"
#include <stdio.h>

// Handle REGISTER command
void handle_register(ServerContext* ctx, int client_sock, const char* json_data) {
    cJSON *request = cJSON_Parse(json_data);
    cJSON *response_json = NULL;
    char *response = NULL;
    
    if (request == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Invalid JSON");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_REGISTER, response);
        free(response);
        cJSON_Delete(response_json);
        return;
    }
    
    cJSON *username_item = cJSON_GetObjectItem(request, "username");
    cJSON *password_item = cJSON_GetObjectItem(request, "password");
    cJSON *email_item = cJSON_GetObjectItem(request, "email");
    
    // Validate required fields
    if (!cJSON_IsString(username_item) || !cJSON_IsString(password_item) || !cJSON_IsString(email_item)) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Missing required fields");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_REGISTER, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    const char *username = username_item->valuestring;
    const char *password = password_item->valuestring;
    const char *email = email_item->valuestring;
    
    // Create user
    int user_id = db_create_user(username, password, email);
    
    response_json = cJSON_CreateObject();
    
    if (user_id > 0) {
        // Success
        cJSON_AddNumberToObject(response_json, "code", 201);
        cJSON_AddStringToObject(response_json, "message", "Account created");
        printf("[REGISTER] User '%s' created successfully (ID: %d)\n", username, user_id);
    } else if (user_id == -2) {
        // Username exists
        cJSON_AddNumberToObject(response_json, "code", 409);
        cJSON_AddStringToObject(response_json, "message", "Username already exists");
        printf("[REGISTER] Failed - Username '%s' already exists\n", username);
    } else if (user_id == -3) {
        // Invalid username
        cJSON_AddNumberToObject(response_json, "code", 422);
        cJSON_AddStringToObject(response_json, "message", "Username contains invalid characters");
        printf("[REGISTER] Failed - Invalid username '%s'\n", username);
    } else {
        // Unknown error
        cJSON_AddNumberToObject(response_json, "code", 500);
        cJSON_AddStringToObject(response_json, "message", "Unknown error occurred");
        printf("[REGISTER] Failed - Unknown error\n");
    }
    
    response = cJSON_PrintUnformatted(response_json);
    send_message(client_sock, CMD_RES_REGISTER, response);
    
    free(response);
    cJSON_Delete(response_json);
    cJSON_Delete(request);
}

// Handle LOGIN command
void handle_login(ServerContext* ctx, int client_sock, const char* json_data) {
    cJSON *request = cJSON_Parse(json_data);
    cJSON *response_json = NULL;
    char *response = NULL;
    
    if (request == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Invalid JSON");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_LOGIN, response);
        free(response);
        cJSON_Delete(response_json);
        return;
    }
    
    cJSON *username_item = cJSON_GetObjectItem(request, "username");
    cJSON *password_item = cJSON_GetObjectItem(request, "password");
    
    if (!cJSON_IsString(username_item) || !cJSON_IsString(password_item)) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Missing required fields");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_LOGIN, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    const char *username = username_item->valuestring;
    const char *password = password_item->valuestring;
    
    User* user = db_find_user_by_username(username);
    
    response_json = cJSON_CreateObject();
    
    if (user == NULL) {
        // User not found
        cJSON_AddNumberToObject(response_json, "code", 404);
        cJSON_AddStringToObject(response_json, "message", "User not found");
        printf("[LOGIN] Failed - User '%s' not found\n", username);
    } else if (!db_verify_password(username, password)) {
        // Wrong password
        cJSON_AddNumberToObject(response_json, "code", 401);
        cJSON_AddStringToObject(response_json, "message", "Wrong password");
        printf("[LOGIN] Failed - Wrong password for user '%s'\n", username);
    } else {
        // Login successful
        char* token = session_create(ctx->sm, user->user_id, client_sock);
        
        cJSON_AddNumberToObject(response_json, "code", 200);
        cJSON_AddStringToObject(response_json, "session_token", token);
        cJSON_AddNumberToObject(response_json, "user_id", user->user_id);
        printf("[LOGIN] User '%s' logged in successfully (ID: %d)\n", username, user->user_id);
    }
    
    response = cJSON_PrintUnformatted(response_json);
    send_message(client_sock, CMD_RES_LOGIN, response);
    
    free(response);
    cJSON_Delete(response_json);
    cJSON_Delete(request);
}

// Handle FRIEND_INVITE command
void handle_friend_invite(ServerContext* ctx, int client_sock, const char* json_data) {
    cJSON *request = cJSON_Parse(json_data);
    cJSON *response_json = NULL;
    cJSON *notification_json = NULL;
    char *response = NULL;
    char *notification = NULL;
    
    if (request == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Invalid JSON");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_INVITE, response);
        free(response);
        cJSON_Delete(response_json);
        return;
    }
    
    cJSON *token_item = cJSON_GetObjectItem(request, "session_token");
    cJSON *target_item = cJSON_GetObjectItem(request, "target_user");
    
    if (!cJSON_IsString(token_item) || !cJSON_IsString(target_item)) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Missing required fields");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_INVITE, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    const char *session_token = token_item->valuestring;
    const char *target_username = target_item->valuestring;
    
    // Validate session
    if (!session_validate(ctx->sm, session_token)) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 401);
        cJSON_AddStringToObject(response_json, "message", "Invalid session token");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_INVITE, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    Session* session = session_find_by_token(ctx->sm, session_token);
    User* target_user = db_find_user_by_username(target_username);
    
    if (target_user == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 404);
        cJSON_AddStringToObject(response_json, "message", "User not found");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_INVITE, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    // Create friend request
    int request_id = db_create_friend_request(session->user_id, target_user->user_id);
    
    if (request_id > 0) {
        User* from_user = db_find_user_by_id(session->user_id);
        
        // Send response to requester
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 111);
        cJSON_AddStringToObject(response_json, "message", "Friend request sent");
        cJSON_AddNumberToObject(response_json, "request_id", request_id);
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_INVITE, response);
        free(response);
        cJSON_Delete(response_json);
        
        // Send notification to target user
        Session* target_session = session_find_by_user_id(ctx->sm, target_user->user_id);
        if (target_session != NULL) {
            notification_json = cJSON_CreateObject();
            cJSON_AddNumberToObject(notification_json, "code", 300);
            cJSON_AddStringToObject(notification_json, "from_user", from_user->username);
            cJSON_AddNumberToObject(notification_json, "from_user_id", from_user->user_id);
            cJSON_AddNumberToObject(notification_json, "request_id", request_id);
            cJSON_AddStringToObject(notification_json, "message", "You have a new friend request");
            notification = cJSON_PrintUnformatted(notification_json);
            send_message(target_session->client_socket, CMD_FRIEND_INVITE_NOTIFICATION, notification);
            free(notification);
            cJSON_Delete(notification_json);
        }
        
        printf("[FRIEND_INVITE] User '%s' sent request to '%s' (request_id: %d)\n", 
               from_user->username, target_username, request_id);
    } else {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 500);
        cJSON_AddStringToObject(response_json, "message", "Unknown error occurred");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_INVITE, response);
        free(response);
        cJSON_Delete(response_json);
    }
    
    cJSON_Delete(request);
}

// Handle FRIEND_RESPONSE command
void handle_friend_response(ServerContext* ctx, int client_sock, const char* json_data) {
    cJSON *request = cJSON_Parse(json_data);
    cJSON *response_json = NULL;
    char *response = NULL;
    
    if (request == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Invalid JSON");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_RESPONSE, response);
        free(response);
        cJSON_Delete(response_json);
        return;
    }
    
    cJSON *session_token_item = cJSON_GetObjectItem(request, "session_token");
    cJSON *request_id_item = cJSON_GetObjectItem(request, "request_id");
    cJSON *accept_item = cJSON_GetObjectItem(request, "accept");
    
    // Validate required fields
    if (!cJSON_IsString(session_token_item) || !cJSON_IsNumber(request_id_item) || !cJSON_IsBool(accept_item)) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Missing required fields");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_RESPONSE, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    const char *session_token = session_token_item->valuestring;
    int request_id = (int)request_id_item->valuedouble;
    int accept = cJSON_IsTrue(accept_item);
    
    // Validate session
    if (!session_validate(ctx->sm, session_token)) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 401);
        cJSON_AddStringToObject(response_json, "message", "Invalid session token");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_RESPONSE, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    FriendRequest* req = db_find_friend_request(request_id);
    if (req == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 404);
        cJSON_AddStringToObject(response_json, "message", "Friend request not found");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_RESPONSE, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    if (accept) {
        // Accept request
        db_accept_friend_request(request_id);
        
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 112);
        cJSON_AddStringToObject(response_json, "message", "Friend request accepted");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_RESPONSE, response);
        free(response);
        cJSON_Delete(response_json);
        
        // Send notification to original requester
        Session* requester_session = session_find_by_user_id(ctx->sm, req->from_user_id);
        if (requester_session != NULL) {
            Session* current_session = session_find_by_token(ctx->sm, session_token);
            User* accepting_user = db_find_user_by_id(current_session->user_id);
            
            cJSON *notification_json = cJSON_CreateObject();
            cJSON_AddNumberToObject(notification_json, "code", 301);
            cJSON_AddNumberToObject(notification_json, "from_user_id", accepting_user->user_id);
            cJSON_AddStringToObject(notification_json, "from_user", accepting_user->username);
            cJSON_AddStringToObject(notification_json, "message", "Your friend request was accepted");
            char *notification = cJSON_PrintUnformatted(notification_json);
            send_message(requester_session->client_socket, CMD_FRIEND_ACCEPTED_NOTIFICATION, notification);
            free(notification);
            cJSON_Delete(notification_json);
        }
        
        printf("[FRIEND_RESPONSE] Request %d accepted\n", request_id);
    } else {
        // Reject request
        db_reject_friend_request(request_id);
        
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 113);
        cJSON_AddStringToObject(response_json, "message", "Friend request declined");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_RESPONSE, response);
        free(response);
        cJSON_Delete(response_json);
        
        printf("[FRIEND_RESPONSE] Request %d declined\n", request_id);
    }
    
    cJSON_Delete(request);
}

// Handle FRIEND_REMOVE command
void handle_friend_remove(ServerContext* ctx, int client_sock, const char* json_data) {
    cJSON *request = cJSON_Parse(json_data);
    cJSON *response_json = NULL;
    char *response = NULL;
    
    if (request == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Invalid JSON");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_REMOVE, response);
        free(response);
        cJSON_Delete(response_json);
        return;
    }
    
    cJSON *session_token_item = cJSON_GetObjectItem(request, "session_token");
    cJSON *friend_username_item = cJSON_GetObjectItem(request, "friend_username");
    
    // Validate required fields
    if (!cJSON_IsString(session_token_item) || !cJSON_IsString(friend_username_item)) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 400);
        cJSON_AddStringToObject(response_json, "message", "Missing required fields");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_REMOVE, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    const char *session_token = session_token_item->valuestring;
    const char *friend_username = friend_username_item->valuestring;
    
    // Validate session
    if (!session_validate(ctx->sm, session_token)) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 401);
        cJSON_AddStringToObject(response_json, "message", "Invalid session token");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_REMOVE, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    Session* session = session_find_by_token(ctx->sm, session_token);
    User* friend_user = db_find_user_by_username(friend_username);
    
    if (friend_user == NULL) {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 404);
        cJSON_AddStringToObject(response_json, "message", "User not found");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_REMOVE, response);
        free(response);
        cJSON_Delete(response_json);
        cJSON_Delete(request);
        return;
    }
    
    // Remove friendship
    int result = db_remove_friendship(session->user_id, friend_user->user_id);
    
    if (result == 0) {
        User* current_user = db_find_user_by_id(session->user_id);
        
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 113);
        cJSON_AddStringToObject(response_json, "message", "Friend removed");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_REMOVE, response);
        free(response);
        cJSON_Delete(response_json);
        
        // Send notification to removed friend
        Session* friend_session = session_find_by_user_id(ctx->sm, friend_user->user_id);
        if (friend_session != NULL) {
            cJSON *notification_json = cJSON_CreateObject();
            cJSON_AddNumberToObject(notification_json, "code", 302);
            cJSON_AddStringToObject(notification_json, "from_user", current_user->username);
            cJSON_AddNumberToObject(notification_json, "from_user_id", current_user->user_id);
            cJSON_AddStringToObject(notification_json, "message", "You are no longer friends");
            char *notification = cJSON_PrintUnformatted(notification_json);
            send_message(friend_session->client_socket, CMD_FRIEND_REMOVED_NOTIFICATION, notification);
            free(notification);
            cJSON_Delete(notification_json);
        }
        
        printf("[FRIEND_REMOVE] User '%s' removed friend '%s'\n", 
               current_user->username, friend_username);
    } else {
        response_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_json, "code", 404);
        cJSON_AddStringToObject(response_json, "message", "Not friends");
        response = cJSON_PrintUnformatted(response_json);
        send_message(client_sock, CMD_RES_FRIEND_REMOVE, response);
        free(response);
        cJSON_Delete(response_json);
    }
    
    cJSON_Delete(request);
}

// Handle incoming client messages
void handle_client_message(ServerContext* ctx, int client_sock, Message* msg) {
    printf("[MESSAGE] Received command: %s\n", msg->command);
    
    if (strcmp(msg->command, CMD_REGISTER) == 0) {
        handle_register(ctx, client_sock, msg->json_data);
    } else if (strcmp(msg->command, CMD_LOGIN) == 0) {
        handle_login(ctx, client_sock, msg->json_data);
    } else if (strcmp(msg->command, CMD_FRIEND_INVITE) == 0) {
        handle_friend_invite(ctx, client_sock, msg->json_data);
    } else if (strcmp(msg->command, CMD_FRIEND_RESPONSE) == 0) {
        handle_friend_response(ctx, client_sock, msg->json_data);
    } else if (strcmp(msg->command, CMD_FRIEND_REMOVE) == 0) {
        handle_friend_remove(ctx, client_sock, msg->json_data);
    } else {
        printf("[ERROR] Unknown command: %s\n", msg->command);
    }
}
