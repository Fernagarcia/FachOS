#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <utils/utils.h>

typedef struct correr_io{
	INTERFAZ* interfaz;
}argumentos_correr_io;

// FUNCIONES IO
void conectar_interfaces();
void iniciar_interfaz(char* nombre, t_config*, t_log*);
void* correr_interfaz(INTERFAZ* );
TIPO_INTERFAZ get_tipo_interfaz(INTERFAZ*, char*);

void peticion_IO_GEN( SOLICITUD_INTERFAZ*, t_config*);
void peticion_STDIN(  SOLICITUD_INTERFAZ*, t_config*);
void peticion_STDOUT( SOLICITUD_INTERFAZ*, t_config*);
void peticion_DIAL_FS(SOLICITUD_INTERFAZ*, t_config*, FILE*, FILE*);

void copiar_operaciones(INTERFAZ* interfaz);
SOLICITUD_INTERFAZ* asignar_espacio_a_solicitud(t_list*);
desbloquear_io* crear_solicitud_desbloqueo(char*, char*);
void recibir_peticiones_interfaz(INTERFAZ*, int, t_log*, FILE*, FILE*);

// FUNCIONES FILE
FILE* iniciar_archivo(char*);
FILE* inicializar_archivo_bloques(const char*, int, int);
FILE* inicializar_bitmap(const char*, int);
void leer_bloque(const char*, int, int, char*);
void escribir_bloque(const char*, int, int, const char*);
void escribirBit(const char*, int);
int buscar_bloque_libre(const char*);

// MENU
typedef enum {
    CONECTAR_GENERICA    = 1,
    CONECTAR_STDIN       = 2,
    CONECTAR_STDOUT      = 3,
    CONECTAR_DIALFS      = 4,
    DESCONECTAR_INTERFAZ = 5,
    SALIR                = 6
} MenuOpciones;

#endif