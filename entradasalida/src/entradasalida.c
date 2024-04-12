#include <stdlib.h>
#include <stdio.h>
#include <entradasalida.h>


int main(int argc, char* argv[]) {
    int err, conexion;
    char* ip, puerto, valor;

    char* path_config = "../entradasalida/entradasalida.config";

    // CREAMOS LOG Y CONFIG

    t_log* logger = iniciar_logger("entradasalida.log", "entradasalida_log", LOG_LEVEL_INFO);
    log_info(logger, "Logger Creado. Esperando mensaje para enviar...");
    
    t_config* config = iniciar_config(path_config);
    ip = config_get_string_value(config, "IP");
	puerto = config_get_string_value(config, "PUERTO");
	valor = config_get_string_value(config, "CLAVE");

    // CREAMOS LA CONEXION
    conexion = crear_conexion(ip, puerto);


    
    //err = abrir_servidor(logger);

    terminar_programa(conexion, logger, config);

    return 0;
}
