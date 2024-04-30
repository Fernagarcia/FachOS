#include <stdlib.h>
#include <stdio.h>
#include <entradasalida.h>

int main(int argc, char* argv[]) {
    int conexion_kernel; //conexion_memoria;
    
    char* ip_kernel; //*ip_memoria;
    char* puerto_kernel; //*puerto_memoria;

    char* path_config = "../entradasalida/entradasalida.config";

    // CREAMOS LOG Y CONFIG

    t_log* logger_entradasalida = iniciar_logger("entradasalida.log", "entradasalida_log", LOG_LEVEL_INFO);
    log_info(logger_entradasalida, "Logger Creado. Esperando mensaje para enviar...");
    
    t_config* config = iniciar_config(path_config);
    ip_kernel = config_get_string_value(config, "IP_KERNEL");
	puerto_kernel = config_get_string_value(config, "PUERTO_KERNEL");
    //ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    //puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");

    conexion_kernel = crear_conexion(ip_kernel, puerto_kernel);
    log_info(logger_entradasalida, "%s\n\t\t\t\t\t%s\t%s\t", "Se ha establecido la conexion con Kernel", ip_kernel, puerto_kernel);
    
    /*
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    log_info(logger_entradasalida, "%s\n\t\t\t\t\t%s\t%s\t", "Se ha establecido la conexion con Memoria", ip_memoria, puerto_memoria);
    */

    char* mensaje_para_kernel = "Se ha conectado la interfaz\n";
    enviar_mensaje(mensaje_para_kernel, conexion_kernel);
    log_info(logger_entradasalida, "Mensajes enviados exitosamente");

    ArgsGestionarServidor* args_cliente = {logger_entradasalida, conexion_kernel};

    gestionar_llegada((void*)&args_cliente);
    
    /*
    char* mensaje_para_memoria = "Se ha conectado la interfaz\n";
    enviar_mensaje(mensaje_para_memoria, conexion_memoria);
    log_info(logger_entradasalida, "Mensaje enviado exitosamente");
    */

    // |----------------------- Checkpoint 2 -----------------------| 

    //int unidadTiempo = config_get_int_value(config, "TIEMPO_UNIDAD_TRABAJO");

    

    
    //liberar_conexion(conexion_memoria); TODO: descomentar las lineas de memoria cuando sea necesario
    liberar_conexion(conexion_kernel);           
    terminar_programa(logger_entradasalida, config);
    return 0;
}