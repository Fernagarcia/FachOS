#include <memoria.h>

int main(int argc, char* argv[]) {
    int opcion;

    char* path_config = "../memoria/memoria.config";
    char* puerto_escucha;

    t_log* logger_memoria = iniciar_logger("memoria.log", "memoria-log", LOG_LEVEL_INFO);
    log_info(logger_memoria, "Logger Creado.");

    t_config* config = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    
    int server_memoria = iniciar_servidor(logger_memoria, puerto_escucha);

    while(1){
        gestionar_llegada(logger_memoria, server_memoria);
    }
}
