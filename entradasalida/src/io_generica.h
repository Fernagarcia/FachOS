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
void peticion_IO_GEN(SOLICITUD_INTERFAZ*, t_config*);
void peticion_STDIN(SOLICITUD_INTERFAZ*, t_config*);
void peticion_STDOUT(SOLICITUD_INTERFAZ*, t_config*);
void peticion_DIAL_FS(SOLICITUD_INTERFAZ*, t_config*, FILE*, FILE*);
void* gestionar_peticion_kernel(void* args);
void copiar_operaciones(INTERFAZ* interfaz);
SOLICITUD_INTERFAZ* asignar_espacio_a_solicitud(t_list*);
desbloquear_io* crear_solicitud_desbloqueo(char* , char*);
int solicitud_valida(char**, char*);
void recibir_peticiones_interfaz(INTERFAZ*, int, t_log*, FILE*, FILE*);


// FILE

FILE* iniciar_archivo(char*);
void leer_bloque(const char*, int, int, char*);
void escribir_bloque(const char*, int, int, const char*);
FILE* inicializar_archivo_bloques(const char*, int, int);
FILE* inicializar_bitmap(const char*, int);
int buscar_bloque_libre(const char*);
void escribirBit(const char*, int);


#endif