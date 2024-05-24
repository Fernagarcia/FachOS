#include <utils/parse.h>
// Array de comandos válidos
const char *valid_commands[7] = {"SET", "SUM", "SUB", "JNZ", "RESIZE", "EXIT", "IO_GEN_SLEEP"};

bool is_valid_command(const char *command) {
    int num_commands = sizeof(valid_commands) / sizeof(valid_commands[0]);
    for (int i = 0; i < num_commands; i++) {
        if (strcmp(valid_commands[i], command) == 0) {
            return true;
        }
    }
    return false;
}

RESPONSE* parse_command(char* input) {
    RESPONSE *response = malloc(sizeof(RESPONSE));
    if (response == NULL) {
        return NULL;
    }
    char command_name[100];
    char input_copy[100];

    strcpy(input_copy, input);

    // Tokenizar string por espacios
    char *token = strtok(input_copy, " ");
    if (token == NULL) {
        free(response);
        return NULL;
    }

    // Compruebo que el comando exista en el array valid_commands.
    if (!is_valid_command(token)) {
        printf("Comando inválido: %s\n", token);
        free(response);
        return NULL;
    }
    printf("Comando válido: %s\n", token);
    strcpy(command_name, token);

    // Agarro los parámetros
    int params_max = 3;
    char *params[params_max];
    int index = 0;
    while ((token = strtok(NULL, " ")) != NULL && index < params_max) {
        params[index++] = token;
    }
    params[index] = NULL; // Marcar el final del array de parámetros.

    // Asignar valores a la estructura RESPONSE
    response->command = strdup(command_name);
    if (response->command == NULL) {
        free(response);
        return NULL;
    }
    response->params = malloc(sizeof(char*) * (index + 1));
    if (response->params == NULL) {
        free(response->command);
        free(response);
        return NULL;
    }
    for (int i = 0; i < index; i++) {
        response->params[i] = strdup(params[i]);
        if (response->params[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(response->params[j]);
            }
            free(response->params);
            free(response->command);
            free(response);
            return NULL;
        }
    }
    response->params[index] = NULL;

    return response;
}