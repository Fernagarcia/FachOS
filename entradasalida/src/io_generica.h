#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <utils/utils.h>
// DEJE EL NAME COMO INT YA QUE, TIENE QUE SER UNICO Y ES MAS FACIL PARA COMPARAR Y BUSCAR
//APARTE AL SER UNICO DEBERIA DE SER MAS COMO UNA ID



typedef struct {
    char* name;
    t_config *configuration;
    pthread_t hilo;
    TIPO_INTERFAZ tipo;
    char* operaciones[5];
} INTERFAZ;

typedef struct correr_io{
	INTERFAZ* interfaz;
}argumentos_correr_io;

//void gestionar_peticion_kernel();

void iniciar_interfaz(char* nombre,t_config* config);
void* correr_interfaz(void* args);
//void usar_interfaz(int nombre_interfaz, char* peticion);
//IO_GENERICA buscar_interfaz(int nombre);
TIPO_INTERFAZ get_tipo_interfaz(INTERFAZ*,char*);
void peticion_IO_GEN(char* peticion, t_config* config);
void* gestionar_peticion_kernel(void* args);
void* operar_interfaz(NUEVA_INTERFAZ*);
void add_Operaciones_Interfaz(char*[],char*[]);
#endif