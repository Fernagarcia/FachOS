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
    enviar_mensaje("Mensaje para KERNEL", conexion_memoria);
    printf("Inserte valores en el paquete a enviar\n");
    paquete(conexion_memoria);
    log_info(logger, "Mensajes enviados exitosamente");

    //Descomentar cuando tengamos hilos en memoria
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_mensaje("Mensaje para MEMORIA", conexion_memoria);
    printf("Inserte valores en el paquete a enviar\n");
    paquete(conexion_memoria);
    log_info(logger, "Mensajes enviados exitosamente");
    
    liberar_conexion(conexion_memoria);
    liberar_conexion(conexion_kernel);           
    terminar_programa(logger, config);
    return 0;
}
