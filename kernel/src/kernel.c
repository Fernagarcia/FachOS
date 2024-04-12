#include <kernel.h>

int main(int argc, char* argv[]) {
    t_log* logger = iniciar_logger("kernel.log", "kernel-log", LOG_LEVEL_INFO);
    log_info(logger, "hola xd");
    //t_config* config = iniciar_config("./kernel.config");

    //log_destroy(logger);

    //config_destroy(config);

    return 0;
}
