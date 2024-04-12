#include <kernel.h>

int main(int argc, char* argv[]) {
    int err;
    t_log* logger = iniciar_logger("kernel.log", "kernel-log", LOG_LEVEL_INFO);
    t_config* config = iniciar_config("/home/utnso/Documents/tp-grupal/tp-2024-1c-Grupo-Facha/kernel/kernel.config");
    log_info(logger, "hola xd");
    //TODO: Ver como cambiar esto!
    //err = abrir_servidor(logger);
    log_destroy(logger);
    config_destroy(config);

    return 0;
}
