#ifndef KERNEL_H_
#define KERNEL_H_

#include<stdio.h>
#include<stdlib.h>
#include <utils/server.h>
#include <utils/client.h>
#include <utils/utils.h>

t_log* iniciar_logger(char* log_path, char* log_name, t_log_level log_level);
t_config* iniciar_config(char* config_path);

#endif

