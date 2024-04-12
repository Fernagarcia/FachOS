#include <kernel.h>

int main(int argc, char* argv[]) {
    int err;

    char* path_config = "../kernel/kernel.config";

    // CREAMOS LOG Y CONFIG

    t_log* logger = iniciar_logger("kernel.log", "kernel-log", LOG_LEVEL_INFO);
    
    t_config* config = iniciar_config(path_config);
    

    log_info(logger, "hola xd");

    
    //err = abrir_servidor(logger);
    log_destroy(logger);
    config_destroy(config);

    //terminar_programa(conexion, logger, config);

    return 0;
}
