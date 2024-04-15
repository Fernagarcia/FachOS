#include <kernel.h>

int main(int argc, char* argv[]) {
    int conexion_memoria, conexion_cpu, opcion;

    char* ip_cpu, *ip_memoria;
    char* puerto_cpu_dispatch, *puerto_memoria;

    char* path_config = "../kernel/kernel.config";
    char* puerto_escucha;

    // CREAMOS LOG Y CONFIG

    t_log* logger_kernel = iniciar_logger("kernel.log", "kernel-log", LOG_LEVEL_INFO);
    log_info(logger_kernel, "Logger Creado.");

    t_config* config = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    ip_cpu = config_get_string_value(config, "IP_CPU");
    puerto_cpu_dispatch = config_get_string_value(config, "PUERTO_CPU_DISPATCH");
    ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");

    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE CPU", ip_cpu, puerto_cpu_dispatch);
    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);

    //TODO VER IMPLEMENTACION DE HILOS PARA PODER HACER LAS ACCIONES EN SIMULTANEO

    while (1) {
        printf("Menú:\n");
        printf("1. Abrir servidor\n");
        printf("2. Establecer conexion con CPU\n");
        printf("3. Establecer conexion con Memoria\n");
        printf("4. Salir\n");
        printf("Seleccione una opción: ");
        scanf("%d", &opcion);

        switch (opcion) {
            case 1:
                printf("INICIANDO SERVIDOR...\n");
                abrir_servidor(logger_kernel, puerto_escucha);
                break;
            case 2:
                printf("Estableciendo conexion con CPU...\n");
                conexion_cpu = crear_conexion(ip_cpu, puerto_cpu_dispatch);
                enviar_mensaje("Hola CPU :)", conexion_cpu);
                printf("Inserte valores en el paquete a enviar\n");
                paquete(conexion_cpu);
                liberar_conexion(conexion_cpu);
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
                terminar_programa(logger_kernel, config);
                return 0;
            default:
                printf("Opción no válida. Por favor, seleccione una opción válida.\n");
        }
    }
}
