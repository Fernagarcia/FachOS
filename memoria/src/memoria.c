#include <memoria.h>

void enviar_instrucciones_a_cpu(char* path, t_log* logger, int cliente_fd){
    char instruccion[50];

    FILE* f = fopen(path, "rb"); // Se recibe la ruta del archivo y se abre en memoria

    while(!feof(f)){
        char* linea_instruccion = fgets(instruccion, strlen(instruccion) + 1, f);
        enviar_mensaje(linea_instruccion, cliente_fd);
    }
    log_info(logger, "Se termino de leer el archivo de instrucciones");
}



int main(int argc, char* argv[]) {
    int i, server_memoria;

    char* path_config = "../memoria/memoria.config";
    char* puerto_escucha;

    // CREAMOS LOG Y CONFIG

    t_log* logger_memoria = iniciar_logger("memoria.log", "memoria-log", LOG_LEVEL_INFO);
    log_info(logger_memoria, "Logger Creado.");

    t_config* config = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");

    pthread_t hilo[3];
    server_memoria = iniciar_servidor(logger_memoria, puerto_escucha);
    log_info(logger_memoria, "Servidor a la espera de clientes");

    int cliente_fd_cpu = esperar_cliente(server_memoria, logger_memoria);
    int cliente_fd_kernel = esperar_cliente(server_memoria, logger_memoria);
    //int cliente_fd_tres = esperar_cliente(server_memoria, logger_memoria);
    
    ArgsGestionarServidor args_sv1 = {logger_memoria, cliente_fd_cpu};
    ArgsGestionarServidor args_sv2 = {logger_memoria, cliente_fd_kernel};
    //ArgsGestionarServidor args_sv3 = {logger_memoria, cliente_fd_tres};

    pthread_create(&hilo[0], NULL, gestionar_llegada, &args_sv1);
    pthread_create(&hilo[1], NULL, gestionar_llegada, &args_sv2);
    //pthread_create(&hilo[2], NULL, gestionar_llegada, &args_sv3);
    

    for(i = 0; i<3; i++){
        pthread_join(hilo[i], NULL);
    }    
    /*while(1){
    
    }*/
    return 0;
}
