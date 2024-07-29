#include <utils/parse.h>
// Array de comandos válidos
const char *valid_commands[19] = {"SET", "SUM", "SUB", "JNZ", "RESIZE", "EXIT", "IO_GEN_SLEEP", "WAIT", "SIGNAL", "MOV_IN", "MOV_OUT", "COPY_STRING", "IO_STDIN_READ", "IO_STDOUT_WRITE", "IO_FS_CREATE", "IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE", "IO_FS_READ"};

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

    char** array_instruction = string_split(input, " ");

    if(!is_valid_command(array_instruction[0])) {
        return NULL;
    }

    response->command = strdup(array_instruction[0]);
    response->params = string_array_new();

    for (int i = 1; i < string_array_size(array_instruction); i++) {
        string_trim_right(&array_instruction[i]);
        string_array_push(&response->params, strdup(array_instruction[i]));
    }
    string_array_destroy(array_instruction);
    
    return response;
}