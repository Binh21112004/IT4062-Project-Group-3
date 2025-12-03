#ifndef CONFIG_H
#define CONFIG_H

#define MAX_CONFIG_LINE 256
#define MAX_CONFIG_VALUE 128

// Database configuration structure
typedef struct {
    char host[MAX_CONFIG_VALUE];
    char port[MAX_CONFIG_VALUE];
    char dbname[MAX_CONFIG_VALUE];
    char user[MAX_CONFIG_VALUE];
    char password[MAX_CONFIG_VALUE];
} DatabaseConfig;

// Load database configuration from file
int config_load_database(const char* config_file, DatabaseConfig* config);

// Build PostgreSQL connection string from config
char* config_build_conninfo(const DatabaseConfig* config);

#endif // CONFIG_H
