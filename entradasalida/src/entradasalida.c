#include <stdlib.h>
#include <stdio.h>
#include <entradasalida.h>

int main(int argc, char* argv[]) {
    int conexion_kernel, conexion_memoria;
    
    char* ip_kernel, *ip_memoria;
    char* puerto_kernel, *puerto_memoria;

    char* path_config = "../entradasalida/entradasalida.config";

    // CREAMOS LOG Y CONFIG

    t_log* logger_entradasalida = iniciar_logger("entradasalida.log", "entradasalida_log", LOG_LEVEL_INFO);
    log_info(logger_entradasalida, "Logger Creado. Esperando mensaje para enviar...");
    
    t_config* config = iniciar_config(path_config);
    ip_kernel = config_get_string_value(config, "IP_KERNEL");
	puerto_kernel = config_get_string_value(config, "PUERTO_KERNEL");
    ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");

    log_info(logger_entradasalida, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE KERNEL", ip_kernel, puerto_kernel);
    log_info(logger_entradasalida, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);
    
    conexion_kernel = crear_conexion(ip_kernel, puerto_kernel);
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);

    char* mensaje_para_kernel = "Espero que te llegue kernel";
    enviar_mensaje(mensaje_para_kernel, conexion_kernel);
    printf("Inserte valores en el paquete a enviar\n");
    paquete(conexion_kernel);
    log_info(logger_entradasalida, "Mensajes enviados exitosamente");

    // -- Descomentar cuando tengamos hilos en memoria
    
    char* mensaje_para_memoria = "Espero que te llegue memoria";
    enviar_mensaje(mensaje_para_memoria, conexion_memoria);
    printf("Inserte valores en el paquete a enviar\n");
    paquete(conexion_memoria);
    log_info(logger_entradasalida, "Mensajes enviados exitosamente");
    
    liberar_conexion(conexion_memoria);
    liberar_conexion(conexion_kernel);           
    terminar_programa(logger_entradasalida, config);
    return 0;
}