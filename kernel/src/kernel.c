#include <kernel.h>

int main(int argc, char* argv[]) {
    char* path_config = "../kernel/kernel.config";
    char* puerto_escucha;

    // CREAMOS LOG Y CONFIG

    t_log* logger_kernel = iniciar_logger("kernel.log", "kernel-log", LOG_LEVEL_INFO);
    log_info(logger_kernel, "Logger Creado.");

    t_config* config = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");

    abrir_servidor(logger_kernel, puerto_escucha);

    log_destroy(logger_kernel);
    
    config_destroy(config);

    //terminar_programa(conexion, logger_kernel, config);

    return 0;
}
