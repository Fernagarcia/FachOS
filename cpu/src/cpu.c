#include <cpu.h>

int main(int argc, char* argv[]) {   
    int conexion_memoria;
    char* config_path = "../cpu/cpu.config";
    
    t_log* logger_cpu = iniciar_logger("../cpu/cpu.log", "cpu-log", LOG_LEVEL_INFO);
    log_info(logger_cpu, "logger para CPU creado exitosamente.");

    t_config* config = iniciar_config(config_path);
    // Get info from cpu.config
    char* ip_memoria = config_get_string_value(config,"IP_MEMORIA");
    char* puerto_memoria = config_get_string_value(config,"PUERTO_MEMORIA");
    char* puerto_dispatch = config_get_string_value(config,"PUERTO_ESCUCHA_DISPATCH");
    char* puerto_interrupt = config_get_string_value(config,"PUERTO_ESCUCHA_INTERRUPT");
    //char* cant_ent_tlb = config_get_string_value(config,"CANTIDAD_ENTRADAS_TLB");
    //char* algoritmo_tlb = config_get_string_value(config,"ALGORITMO_TLB");

    // Abrir servidores
    int server_dispatch = iniciar_servidor(logger_cpu, puerto_dispatch);
    int server_interrupt = iniciar_servidor(logger_cpu, puerto_interrupt);

    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_mensaje("Hola MEMORIA", conexion_memoria);
    printf("Inserte valores en el paquete a enviar\n");
    paquete(conexion_memoria);
    
    
        // Conexion con memoria (Aca posiblemente hilos (?))
        //gestionar_llegada(logger_cpu, server_dispatch);
    gestionar_llegada(logger_cpu, conexion_memoria);
    //recibir_mensaje(conexion_memoria, logger_cpu);
    //char mensaje[23];
    //recv(conexion_memoria, &mensaje, strlen(mensaje) + 1, MSG_WAITALL);

    liberar_conexion(conexion_memoria);
    printf("Saliendo del programa...\n");
    terminar_programa(logger_cpu, config);
    return 0;
}


