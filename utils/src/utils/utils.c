#include <utils/utils.h>

t_log* iniciar_logger(char* log_path, char* log_name, t_log_level log_level)
{
	t_log* nuevo_logger;
	nuevo_logger = log_create(log_path, log_name, 1, log_level);

	return nuevo_logger;
}

t_config* iniciar_config(char* config_path)
{
    t_config* nuevo_config;
    nuevo_config = config_create(config_path);

    return nuevo_config;
}