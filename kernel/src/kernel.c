#include <kernel.h>

int main(int argc, char* argv[]) {
    int conexion_memoria, conexion_cpu;

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

    abrir_servidor(logger_kernel, puerto_escucha);

    //TODO HACER envio de paquetes y/o mensajes a los distintos servidores

    conexion_cpu = crear_conexion(ip_cpu, puerto_cpu_dispatch);
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);

    terminar_programa(logger_kernel, config);
    liberar_conexion(conexion_cpu);
    liberar_conexion(conexion_memoria);

    return 0;
}
