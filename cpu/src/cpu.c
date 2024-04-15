#include <cpu.h>

int main(int argc, char* argv[]) {   
    int conexion_memoria, opcion;
    char* config_path = "../cpu/cpu.config";
    
    t_log* logger = iniciar_logger("../cpu/cpu.log", "cpu-log", LOG_LEVEL_INFO);
    log_info(logger, "Logger para CPU creado exitosamente.");

    t_config* config = iniciar_config(config_path);
    // Get info from cpu.config
    char* ip_memoria = config_get_string_value(config,"IP_MEMORIA");
    char* puerto_memoria = config_get_string_value(config,"PUERTO_MEMORIA");
    char* puerto_dispatch = config_get_string_value(config,"PUERTO_ESCUCHA_DISPATCH");
    char* puerto_interrupt = config_get_string_value(config,"PUERTO_ESCUCHA_INTERRUPT");
    //char* cant_ent_tlb = config_get_string_value(config,"CANTIDAD_ENTRADAS_TLB");
    //char* algoritmo_tlb = config_get_string_value(config,"ALGORITMO_TLB");
    
    while (1) {
        printf("Menú:\n");
        printf("1. Abrir servidor DISPATCH\n");
        printf("2. Abrir servidor INTERRUPT\n");
        printf("3. Establecer conexion con Memoria\n");
        printf("4. Salir\n");
        printf("Seleccione una opción: ");
        scanf("%d", &opcion);

        switch (opcion) {
            case 1:
                printf("INICIANDO SERVIDOR DISPATCH...\n");
                abrir_servidor(logger, puerto_dispatch);
                break;
            case 2:
                printf("INICIANDO SERVIDOR INTERRUPT...\n");
                abrir_servidor(logger, puerto_interrupt);
                break;
            case 3:
                printf("Estableciendo conexion con MEMORIA....\n");
                conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
                enviar_mensaje("Hola MEMORIA", conexion_memoria);
                printf("Inserte valores en el paquete a enviar\n");
                paquete(conexion_memoria);
                liberar_conexion(conexion_memoria);
                break;
            case 4:
                printf("Saliendo del programa...\n");
                terminar_programa(logger, config);
                return 0;
            default:
                printf("Opción no válida. Por favor, seleccione una opción válida.\n");
        }
    }
}

