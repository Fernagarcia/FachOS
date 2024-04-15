#include <memoria.h>

int main(int argc, char* argv[]) {
    int opcion;

    char* path_config = "../memoria/memoria.config";
    char* puerto_escucha;

    t_log* logger_memoria = iniciar_logger("memoria.log", "memoria-log", LOG_LEVEL_INFO);
    log_info(logger_memoria, "Logger Creado.");

    t_config* config = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    
    while(1){
        printf("Menú:\n");
        printf("1. Abrir servidor\n");
        printf("2. Salir\n");
        printf("Seleccione una opción: ");
        scanf("%d", &opcion);

        switch (opcion) {
            case 1:
                printf("INICIANDO SERVIDOR...\n");
                abrir_servidor(logger_memoria, puerto_escucha);
                break;
            case 2:
                printf("Saliendo del programa...\n");
                terminar_programa(logger_memoria, config);
                return 0;
        }
    }
}
