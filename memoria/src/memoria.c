#include <memoria.h>

int main(int argc, char* argv[]) {
    char* path_config = "../memoria/memoria.config";
    char* puerto_escucha;

    // CREAMOS LOG Y CONFIG

    t_log* logger_memoria = iniciar_logger("memoria.log", "memoria-log", LOG_LEVEL_INFO);
    log_info(logger_memoria, "Logger Creado.");

    t_config* config = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");

    abrir_servidor(logger_memoria, puerto_escucha);

    
    terminar_programa(logger_memoria, config);

    return 0;
}
