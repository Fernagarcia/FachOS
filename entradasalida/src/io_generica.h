#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <utils/utils.h>
// DEJE EL NAME COMO INT YA QUE, TIENE QUE SER UNICO Y ES MAS FACIL PARA COMPARAR Y BUSCAR
//APARTE AL SER UNICO DEBERIA DE SER MAS COMO UNA ID

typedef struct correr_io{
	INTERFAZ* interfaz;
}argumentos_correr_io;

//void gestionar_peticion_kernel();

void* conectar_interfaces(void* args);
void iniciar_interfaz(char* nombre, t_config*, t_log*);
void* correr_interfaz(void* args);
//void usar_interfaz(int nombre_interfaz, char* peticion);
//IO_GENERICA buscar_interfaz(int nombre);
TIPO_INTERFAZ get_tipo_interfaz(INTERFAZ*,char*);
void peticion_IO_GEN(char* peticion, t_config* config);
void* gestionar_peticion_kernel(void* args);
void operar_interfaz(INTERFAZ*);
void copiar_operaciones(INTERFAZ* interfaz);
void buscar_y_desconectar(char*, t_list*);
void destruir_interfaz(void*);
void liberar_memoria(char**, int); 
#endif