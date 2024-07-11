#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <utils/utils.h>

typedef struct correr_io{
	INTERFAZ* interfaz;
}argumentos_correr_io;

// FUCIONES_IO
#pragma region FUNCIONES_IO

void* conectar_interfaces(void* args);

void iniciar_interfaz(char* nombre, t_config*, t_log*);
TIPO_INTERFAZ get_tipo_interfaz(INTERFAZ*,char*);
void copiar_operaciones(INTERFAZ* interfaz);

void* correr_interfaz(void* args);
void recibir_peticiones_interfaz(INTERFAZ*, int, t_log*, FILE*, FILE*);
desbloquear_io* crear_solicitud_desbloqueo(char* , char*);

void peticion_IO_GEN(SOLICITUD_INTERFAZ*, t_config*);
void peticion_STDIN(SOLICITUD_INTERFAZ*, t_config*);
void peticion_STDOUT(SOLICITUD_INTERFAZ*, t_config*);
void peticion_DIAL_FS(SOLICITUD_INTERFAZ*, t_config*, FILE*, FILE*);

void* gestionar_peticion_kernel(void* args);
SOLICITUD_INTERFAZ* asignar_espacio_a_solicitud(t_list*);

#pragma endregion

// FILE
#pragma region FUNCIONES_FILE

FILE* iniciar_archivo(char*);
FILE* inicializar_archivo_bloques(const char*, int, int);
FILE* inicializar_bitmap(const char*, int);
int buscar_bloque_libre(const char*);
void leer_bloque(const char*, int, int, char*);
void escribir_bloque(const char*, int, int, const char*);
void escribirBit(const char*, int);
void crear_archivo(const char*, const char*, int);

#pragma endregion

int solicitud_valida(char**, char*); // SE USA?

#endif