#include <memoria.h>

int main(int argc, char* argv[]) {
    int i, server_memoria;

    char* path_config = "../memoria/memoria.config";
    char* puerto_escucha;

    // CREAMOS LOG Y CONFIG

    t_log* logger_memoria = iniciar_logger("memoria.log", "memoria-log", LOG_LEVEL_INFO);
    log_info(logger_memoria, "Logger Creado.");

    t_config* config = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");

    pthread_t hilo[2];
    server_memoria = iniciar_servidor(logger_memoria, puerto_escucha);
    
    ArgsGestionarServidor args = {logger_memoria, server_memoria};

    for(i = 0; i<3; i++){
        pthread_create(&hilo[i], NULL, gestionar_llegada, &args);
    }

    for(i = 0; i<3; i++){
        pthread_join(hilo[i], NULL);
    }    
    /*while(1){
    
    }*/
    return 0;
}
