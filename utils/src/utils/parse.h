#ifndef PARSE_H_
#define PARSE_H_
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    char *command;
    void (*function)(char **);
    char *description;
} COMMAND;

typedef struct {
    char *command;
    char **params;
} RESPONSE;


bool is_valid_command(const char *command, void *structure);
RESPONSE* parse_command(char* input, void *structure);
#endif PARSE_H_