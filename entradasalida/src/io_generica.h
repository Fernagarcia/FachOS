#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <utils/utils.h>

typedef struct {
    const char *name;
    t_config *configuration;
} IO_GENERICA;

void iniciar_interfaz(char* nombre, t_config* config);

#endif