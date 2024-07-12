#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <utils/utils.h>

typedef struct correr_io{
	INTERFAZ* interfaz;
}argumentos_correr_io;

// FUNCIONES IO
#pragma region FUNCIONES_IO

void conectar_interfaces();
void iniciar_interfaz(char* nombre, t_config*, t_log*);
void* correr_interfaz(void* args);
TIPO_INTERFAZ get_tipo_interfaz(INTERFAZ*, char*);

void peticion_IO_GEN(SOLICITUD_INTERFAZ*, t_config*);
void peticion_STDIN(SOLICITUD_INTERFAZ*, t_config*);
void peticion_STDOUT(SOLICITUD_INTERFAZ*, t_config*);
void peticion_DIAL_FS(SOLICITUD_INTERFAZ*, t_config*, FILE*, FILE*);

void copiar_operaciones(INTERFAZ* interfaz);
SOLICITUD_INTERFAZ* asignar_espacio_a_solicitud(t_list*);
desbloquear_io* crear_solicitud_desbloqueo(char*, char*);
void recibir_peticiones_interfaz(INTERFAZ*, int, t_log*, FILE*, FILE*);

#pragma endregion

// FUNCIONES FILE
#pragma region FUNCIONES_FILESYSTEM

FILE* iniciar_archivo(char*);
FILE* inicializar_archivo_bloques(const char*, int, int);
FILE* inicializar_bitmap(const char*, int);
void leer_bloque(const char*, int, int, char*);
void escribir_bloque(const char*, int, int, const char*);
void escribirBit(const char*, int);
int buscar_bloque_libre(const char*);

#pragma endregion

#endif