#include <stdlib.h>
#include <stdio.h>
#include <entradasalida.h>


int main(int argc, char* argv[]) {
    int conexion_kernel, conexion_memoria;
    
    char* ip_kernel;
    char* puerto_kernel;

    char* ip_memoria;
    char* puerto_memoria;

    char* path_config = "../entradasalida/entradasalida.config";

    // CREAMOS LOG Y CONFIG

    t_log* logger = iniciar_logger("entradasalida.log", "entradasalida_log", LOG_LEVEL_INFO);
    log_info(logger, "Logger Creado. Esperando mensaje para enviar...");
    
    t_config* config = iniciar_config(path_config);
    ip_kernel = config_get_string_value(config, "IP_KERNEL");
	puerto_kernel = config_get_string_value(config, "PUERTO_KERNEL");
    ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");

    log_info(logger, "INFORMACION SOBRE KERNEL");
    log_info(logger, ip_kernel);
    log_info(logger, puerto_kernel);
    log_info(logger, "INFORMACION SOBRE MEMORIA");
    log_info(logger, ip_memoria);
    log_info(logger, puerto_memoria);
    
    // CREAMOS LA CONEXION
    conexion_kernel = crear_conexion(ip_kernel, puerto_kernel);

    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);

    char* mensaje_para_kernel = "Espero que te llegue kernel";
    char* mensaje_para_memoria = "Espero que te llegue memoria";
    enviar_mensaje(mensaje_para_kernel, conexion_kernel);
    enviar_mensaje(mensaje_para_memoria, conexion_memoria);

    terminar_programa(logger, config);

    //TODO PARA PENSAR: Desarrollar funcion para liberar todas las conexiones (si es necesario). 
    liberar_conexion(conexion_kernel);
    liberar_conexion(conexion_memoria);
    return 0;
}
