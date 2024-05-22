#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <utils/utils.h>
// DEJE EL NAME COMO INT YA QUE, TIENE QUE SER UNICO Y ES MAS FACIL PARA COMPARAR Y BUSCAR
//APARTE AL SER UNICO DEBERIA DE SER MAS COMO UNA ID

enum TIPO_INTERFAZ{
  GENERICA,
  STDIN,
  STDOUT,
  DIAL_FS
};

typedef struct {
    int name;
    t_config *configuration;
    enum TIPO_INTERFAZ tipo;
    pthread_t* hilo; 
} INTERFAZ;

typedef struct correr_io{
	INTERFAZ* interfaz;
}argumentos_correr_io;



void iniciar_interfaz(t_config* config);
void* correr_interfaz(void* args);
//void usar_interfaz(int nombre_interfaz, char* peticion);
//IO_GENERICA buscar_interfaz(int nombre);
enum TIPO_INTERFAZ get_tipo_interfaz(char* tipo_nombre);
void peticion_IO_GEN(char* peticion, t_config* config);

#endif