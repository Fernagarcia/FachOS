#include <cpu.h>

int main(int argc, char* argv[]) { 
    int i;

    t_log* logger = iniciar_logger("../cpu/cpu.log", "cpu-log", LOG_LEVEL_INFO);
    log_info(logger, "Logger creado exitosamente.");

    char* config_path = "../cpu/cpu.config";

    t_config* config = iniciar_config(config_path);

    pthread_t hilo_id[2];

    // Get info from cpu.config
    char* ip_memoria = config_get_string_value(config,"IP_MEMORIA");
    char* puerto_memoria = config_get_string_value(config,"PUERTO_MEMORIA");
    char* puerto_dispatch = config_get_string_value(config,"PUERTO_ESCUCHA_DISPATCH");
    char* puerto_interrupt = config_get_string_value(config,"PUERTO_ESCUCHA_INTERRUPT");
    //char* cant_ent_tlb = config_get_string_value(config,"CANTIDAD_ENTRADAS_TLB");
    //char* algoritmo_tlb = config_get_string_value(config,"ALGORITMO_TLB");
    log_info(logger, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);

    // Abrir servidores
    int server_dispatch = iniciar_servidor(logger, puerto_dispatch);
    log_info(logger, "Servidor dispatch abierto");
    int server_interrupt = iniciar_servidor(logger, puerto_interrupt);
    log_info(logger, "Servidor interrupt abierto");
  
    int cliente_fd_dispatch = esperar_cliente(server_dispatch, logger);
    int cliente_fd_interrupt = esperar_cliente(server_interrupt, logger);

    int conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_mensaje("CPU IS IN DA HOUSE", conexion_memoria);

    ArgsGestionarServidor args_dispatch = {logger, cliente_fd_dispatch};
    ArgsGestionarServidor args_interrupt = {logger, cliente_fd_interrupt};
    
    pthread_create(&hilo_id[0], NULL, gestionar_llegada, &args_dispatch);
    pthread_create(&hilo_id[1], NULL, gestionar_llegada, &args_interrupt);

    for(i = 0; i<2; i++){
        pthread_join(hilo_id[i], NULL);
    }
    
    liberar_conexion(conexion_memoria);
    terminar_programa(logger, config);
    return 0;
}
