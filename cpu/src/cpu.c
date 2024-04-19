#include <cpu.h>

int main(int argc, char* argv[]) {   
    t_log* logger = iniciar_logger("../cpu/cpu.log", "cpu-log", LOG_LEVEL_INFO);

    char* config_path = "../cpu/cpu.config";

    t_config* config = iniciar_config(config_path);

    // Get info from cpu.config
    //char* ip_memoria = config_get_string_value(config,"IP_MEMORIA");
    //char* puerto_memoria = config_get_string_value(config,"PUERTO_MEMORIA");
    char* puerto_dispatch = config_get_string_value(config,"PUERTO_ESCUCHA_DISPATCH");
    //char* puerto_interrupt = config_get_string_value(config,"PUERTO_ESCUCHA_INTERRUPT");
    //char* cant_ent_tlb = config_get_string_value(config,"CANTIDAD_ENTRADAS_TLB");
    //char* algoritmo_tlb = config_get_string_value(config,"ALGORITMO_TLB");

    // Abrir servidores
    int server_dispatch = iniciar_servidor(logger, puerto_dispatch);
    //int server_interrupt = iniciar_servidor(logger, puerto_interrupt);

    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_mensaje("Hola MEMORIA", conexion_memoria);
    printf("Inserte valores en el paquete a enviar\n");
    paquete(conexion_memoria);

    ArgsGestionarServidor args = {logger, server_dispatch};
    
    while (1) {
        // Conexion con memoria (Aca posiblemente hilos (?))
        gestionar_llegada(&args);
        //gestionar_llegada(logger, server_interrupt);
    }

    terminar_programa(logger, config);
<<<<<<< Updated upstream
=======
    return 0;
>>>>>>> Stashed changes
}
