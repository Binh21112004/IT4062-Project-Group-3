#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Trim whitespace from string
static char* trim(char* str) {
    char* end;
    
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    end[1] = '\0';
    return str;
}

// Load database configuration from file
int config_load_database(const char* config_file, DatabaseConfig* config) {
    FILE* file = fopen(config_file, "r");
    if (!file) {
        fprintf(stderr, "Failed to open config file: %s\n", config_file);
        return -1;
    }
    
    // Set defaults
    strcpy(config->host, "localhost");
    strcpy(config->port, "5432");
    strcpy(config->dbname, "event_management");
    strcpy(config->user, "postgres");
    strcpy(config->password, "");
    
    char line[MAX_CONFIG_LINE];
    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        char* trimmed = trim(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') {
            continue;
        }
        
        // Parse key=value
        char* equal = strchr(trimmed, '=');
        if (!equal) continue;
        
        *equal = '\0';
        char* key = trim(trimmed);
        char* value = trim(equal + 1);
        
        // Set config values
        if (strcmp(key, "host") == 0) {
            strncpy(config->host, value, MAX_CONFIG_VALUE - 1);
        } else if (strcmp(key, "port") == 0) {
            strncpy(config->port, value, MAX_CONFIG_VALUE - 1);
        } else if (strcmp(key, "dbname") == 0) {
            strncpy(config->dbname, value, MAX_CONFIG_VALUE - 1);
        } else if (strcmp(key, "user") == 0) {
            strncpy(config->user, value, MAX_CONFIG_VALUE - 1);
        } else if (strcmp(key, "password") == 0) {
            strncpy(config->password, value, MAX_CONFIG_VALUE - 1);
        }
    }
    
    fclose(file);
    
    // Validate required fields
    if (strlen(config->password) == 0) {
        fprintf(stderr, "Warning: Database password is empty in config file\n");
    }
    
    return 0;
}

// Build PostgreSQL connection string from config
char* config_build_conninfo(const DatabaseConfig* config) {
    static char conninfo[512];
    
    snprintf(conninfo, sizeof(conninfo),
             "host=%s port=%s dbname='%s' user=%s password=%s",
             config->host, config->port, config->dbname,
             config->user, config->password);
    
    return conninfo;
}
