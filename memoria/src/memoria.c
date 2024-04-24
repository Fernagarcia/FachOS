#include <memoria.h>

t_list* lista_instrucciones = list_create();

t_list* abrir_pseudocodigo(char* path_instrucciones) {
    FILE *fp;
    char linea[20];
    char* full_path = strcat(path_instrucciones, "/prueba.txt");

    fp = fopen(full_path, "r");

    if (fp == NULL) {
        printf("Error al abrir el archivo de instrucciones");
        return 1;
    }

    while (fgets(linea, sizeof(linea), fp) != NULL) {
        // Imprime la línea leída
        printf("Línea leída: %s", linea);

        // Agregar instrucciones a lista
        list_add(lista_instrucciones, linea);
    }
}

int main(int argc, char* argv[]) {
    char* path_config = "../memoria/memoria.config";
    char* puerto_escucha;
    char* path_instrucciones;

    t_log* logger_memoria = iniciar_logger("memoria.log", "memoria-log", LOG_LEVEL_INFO);
    log_info(logger_memoria, "Logger Creado.");

    t_config* config = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    path_instrucciones = config_get_string_value(config, "PATH_INSTRUCCIONES");

    t_list* lista_instrucciones = abrir_pseudocodigo(path_instrucciones);

    paqueteInstrucciones(lista_instrucciones);
    
    int server_memoria = iniciar_servidor(logger_memoria, puerto_escucha);

    while(1){
        gestionar_llegada(logger_memoria, server_memoria);
    }
    return 0;
}
