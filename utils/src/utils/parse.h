#ifndef PARSE_H_
#define PARSE_H_
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <utils/utils.h>
typedef struct {
    char *command;
    char **params;
    char *direccion_fisica;
} RESPONSE;

bool is_valid_command(const char *command);
RESPONSE* parse_command(char* input);
void trim_newline(char *);

#endif