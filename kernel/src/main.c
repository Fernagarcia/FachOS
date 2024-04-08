#include <stdlib.h>
#include <stdio.h>
#include <utils/server.h>
#include <utils/client.h>


int main(int argc, char* argv[]) {
    
    t_log* logger = iniciar_logger();

    t_config* config = iniciar_config();

    log_destroy(logger);

    config_destroy(config);

    return 0;
}

t_log* iniciar_logger(void)
{
	t_log* nuevo_logger;

	nuevo_logger = log_create("Kernel-Log.log", "Cliente-Kernel", 1, LOG_LEVEL_INFO);
	
	return nuevo_logger;
}

t_config* iniciar_config(void)
{
    t_config* nuevo_config;

    nuevo_config = config_create("/home/utnso/so-commons-library/tpSO/tp-2024-1c-Grupo-Facha/kernel/src");

    return nuevo_config;
}
