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
t_config* iniciar_configuracion();

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
void leer_bloque(int, char*);
void escribir_bloque(int, const char*);
void borrar_bloque(int);
void set_bit(int, int);
int get_bit(int);
void crear_metadata(char *, int, int);
void leer_metadata(char*, int*, int*);
int buscar_bloque_libre();
bool tiene_espacio_suficiente(int, int, int);
void asignar_espacio_en_bitmap(int, int);
int bloques_libres_contiguos(int, int);
int bloques_libres_a_partir_de(int);

void compactar();
int crear_archivo(char*);
void borrar_archivo(char*);
void truncar(char*, int);
void menu_interactivo_fs_para_pruebas();


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