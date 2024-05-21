#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <utils/utils.h>

typedef struct {
    const char *name;
    t_config *configuration;
    enum TIPO_INTERFAZ tipo;
} INTERFAZ;

enum TIPO_INTERFAZ{
    GENERICA,
    STDIN,
    STDOUT,
    DIAL_FS
}

void iniciar_interfaz(char* nombre, t_config* config);
void usar_interfaz(char* nombre_interfaz, char* peticion);
IO_GENERICA buscar_interfaz(char* nombre);
enum TIPO_INTERFAZ get_tipo_interfaz(char* tipo_nombre);
void peticion_IO_GEN(char* peticion, t_config* config);

#endif