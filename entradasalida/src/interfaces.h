#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <utils/utils.h>

typedef struct correr_io{
	INTERFAZ* interfaz;
}argumentos_correr_io;

typedef struct {
    char nombre_archivo[50];
    int bloque_inicial;
    int tamanio_archivo;
} MetadataArchivo;

// FUNCIONES IO
void conectar_interfaces();
void iniciar_interfaz(char* nombre, t_config*, t_log*);
void* correr_interfaz(INTERFAZ* );
TIPO_INTERFAZ get_tipo_interfaz(INTERFAZ*, char*);
t_config* iniciar_configuracion();

void peticion_IO_GEN(SOLICITUD_INTERFAZ*, INTERFAZ*);
void peticion_STDIN(  SOLICITUD_INTERFAZ*, INTERFAZ*);
void peticion_STDOUT( SOLICITUD_INTERFAZ*, INTERFAZ*);
void peticion_DIAL_FS(SOLICITUD_INTERFAZ*, INTERFAZ*, FILE*, FILE*);

void copiar_operaciones(INTERFAZ* interfaz);
SOLICITUD_INTERFAZ* asignar_espacio_a_solicitud(t_list*);
desbloquear_io* crear_solicitud_desbloqueo(char*, char*);
void recibir_peticiones_interfaz(INTERFAZ*, int, t_log*, FILE*, FILE*);

// FUNCIONES FILE
FILE* iniciar_archivo(char*);
void iniciar_archivo_bloques(const char*);
void leer_bloque(int, char*);
void escribir_bloque(int, const char*);
void crear_metadata(char *, int, int);
void leer_metadata(char*, int*, int*);
bool tiene_espacio_suficiente(int, int, int);
int bloques_libres_contiguos(int, int);
int bloques_libres_a_partir_de(int);

void listar_archivos_metadata(const char*);
void cargar_metadata(const char*, MetadataArchivo*);
MetadataArchivo* encontrar_archivo_por_nombre(const char*);
int indice_de_archivo(const char*);
void modificar_archivo_en_lista(const char*, int, int);
void eliminar_archivo_de_lista(const char*);
void imprimir_lista_archivos();

void crear_y_mapear_bitmap(const char*);
void establecer_bit(int, int);
int obtener_bit(int);
void imprimir_bitmap();
void liberar_bitmap();
int buscar_bloque_libre();
void asignar_espacio_en_bitmap(int, int);
void actualizar_bitmap(int, int, int);

void compactar();
void compactar_archivo_bloques();
void compactar_y_mover_archivo_al_final(char*);

int crear_archivo(char*);
void borrar_archivo(char*);
void truncar(char*, int);
void escribir_en_archivo(const char*, const char*, int, int, char*);
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