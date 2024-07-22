#ifndef UTILS_H_
#define UTILS_H_

#include<utils/utils.h>

#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<netdb.h>
#include<string.h>
#include<errno.h>
#include<pthread.h>
#include<semaphore.h>
#include<math.h>

#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<sys/file.h>

#include<commons/txt.h>
#include<commons/string.h>
#include<commons/log.h>
#include<commons/config.h>
#include<commons/collections/list.h>
#include<commons/collections/queue.h>
#include<commons/process.h>
#include<commons/temporal.h>
#include<commons/memory.h>

#include<readline/readline.h>
#include<readline/history.h>

typedef enum operaciones{
	MENSAJE,
	CONTEXTO,
	INSTRUCCION,
	INTERRUPCION,
	CREAR_PROCESO,
	FINALIZAR_PROCESO,
	NUEVA_IO,
	SOLICITUD_IO,
	DESCONECTAR_IO,
	DESBLOQUEAR_PID,
	IO_GENERICA,
	IO_STDIN,
	IO_STDOUT,
	IO_DIALFS,
	O_WAIT,
	O_SIGNAL,
	SOLICITUD_MEMORIA,
	MEMORIA_ASIGNADA,
	ACCEDER_MARCO,
	// falta agregar los de dial_fs
	MULTIPROGRAMACION,
	TIEMPO_RESPUESTA,
	RESPUESTA_MEMORIA,
	LEER_MEMORIA,
	RESPUESTA_LEER_MEMORIA,
	ESCRIBIR_MEMORIA,
	RESPUESTA_ESCRIBIR_MEMORIA,
	RESIZE,
	OUT_OF_MEMORY,
	COPY_STRING,
	USER_INTERRUPTED
}op_code;

typedef struct{
	int size;
	void* stream;
}t_buffer;

typedef struct{
  char* nombre;
  int instancia;
  t_queue* procesos_bloqueados;
}t_recurso;

typedef struct{
  char* nombre;
  int instancia;
}p_recurso;

typedef struct{
	op_code codigo_operacion;
	t_buffer* buffer;
}t_paquete;

typedef enum SALIDAS{
	FIN_INSTRUCCION,
	QUANTUM,
	INTERRUPTED,
	IO,
	T_WAIT,
	T_SIGNAL,
	SIN_MEMORIA,
}MOTIVO_SALIDA;

typedef enum INTERFACES{
  GENERICA,
  STDIN,
  STDOUT,
  DIAL_FS
}TIPO_INTERFAZ;

typedef struct {
    int pid;
    int pagina;
} PAQUETE_MARCO;

typedef struct {
	char* direccion_fisica;
	char* tamanio;
	char* pid;
}PAQUETE_LECTURA;

typedef struct {
	char* direccion_fisica_destino;
	char* direccion_fisica_origen;
	char* tamanio;
	char* pid;
}PAQUETE_COPY_STRING;

typedef enum ESTADO_INTERFAZ{
	LIBRE,
	OCUPADA
}estados_interfaz;

typedef struct {
	int nro_pagina;
    int marco;
    bool bit_validacion;
}PAGINA;

typedef struct{
    int pid;
    t_list* paginas;
}TABLA_PAGINA;

typedef struct registroCPU{
	uint32_t PC;		// Program counter
	uint8_t AX;			// registro númerico de propósito general
	uint8_t BX;			// registro númerico de propósito general
	uint8_t CX;			// registro númerico de propósito general
	uint8_t DX;			// registro númerico de propósito general
	uint32_t EAX;		// registro númerico de propósito general
	uint32_t EBX;		// registro númerico de propósito general
	uint32_t ECX;		// registro númerico de propósito general
	uint32_t EDX;		// registro númerico de propósito general
	uint32_t SI;		// dirección lógica de memoria de origen desde donde se va a copiar un string
	uint32_t DI;		// dirección lógica de memoria de destino desde donde se va a copiar un string
	TABLA_PAGINA *PTBR; // Page Table Base Register. Almacena el puntero hacia la tabla de pagina de un proceso.
    uint32_t PTLR;      // Page Table Length Register. Sirve para delimitar el espacio de memoria de un proceso.
}regCPU;

typedef struct contexto{
	int PID;
	int quantum;
	regCPU* registros;
	MOTIVO_SALIDA motivo;
}cont_exec;

typedef struct pcb{
	cont_exec* contexto;
	char* estadoAnterior;
	char* estadoActual;
	t_list* recursos_adquiridos;
}pcb;

typedef struct SOLICITUD_INTERFAZ{
  char* nombre;
  char* solicitud;
  char** args;
  char* pid;
}SOLICITUD_INTERFAZ;

typedef struct NEW_INTERFACE{
    TIPO_INTERFAZ tipo;
    char** operaciones;
}DATOS_INTERFAZ;

typedef struct {
	char* nombre;
	int cliente_fd;
	int conexion_kernel;
	int conexion_memoria;
	pthread_t hilo_de_llegada_kernel;
	pthread_t hilo_de_llegada_memoria;
}DATOS_CONEXION;

typedef struct {
    DATOS_INTERFAZ* datos;
	DATOS_CONEXION* sockets;
    t_config *configuration;
	estados_interfaz estado;
	t_queue* procesos_bloqueados;
	pthread_mutex_t mutex;
	int proceso_asignado;	
} INTERFAZ;

typedef struct {
	char* pid;
	char* nombre;
}desbloquear_io;

typedef struct{
	int id_proceso;
	char* path;
}c_proceso_data;

typedef struct{
	char* pid;
	char* pc;
}t_instruccion;

typedef struct{
	char* tamanio;
	int pid;
}t_resize;

typedef struct datos_a_memoria{
    void* data;
    int tamanio;
}t_dato;

typedef struct {
	char* direccion_fisica;
	int pid;
	t_dato* dato;
}PAQUETE_ESCRITURA;

// FUNCIONES UTILS 

t_log* iniciar_logger(char* log_path, char* log_name, t_log_level log_level);
t_config* iniciar_config(char* config_path);
void terminar_programa(t_log* logger, t_config* config);
void eliminarEspaciosBlanco(char*);
bool es_nombre_de_interfaz(char*, void*);
void buscar_y_desconectar(char*, t_list*, t_log*);
void destruir_interfaz(void*);
void destruir_datos_io(void*);
void liberar_memoria(char**, int); 
void eliminar_io_solicitada(void*);

// FUNCIONES CLIENTE

int crear_conexion(char* ip, char* puerto);
void enviar_operacion(char* mensaje, int socket_cliente, op_code);
t_paquete* crear_paquete(op_code);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void liberar_conexion(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);

// PAQUETES DE DATOS (Repiten logica pero cada una es para mandar datos distinto hacia o desde lugares distintos)

void enviar_solicitud_io(int , SOLICITUD_INTERFAZ*, op_code);
void paqueteDeMensajes(int, char*, op_code);
void paqueteDeDesbloqueo(int conexion, desbloquear_io *solicitud);
void paqueteRecurso(int, cont_exec*, char*, op_code);
void peticion_de_espacio_para_pcb(int, pcb*, op_code);
void peticion_de_eliminacion_espacio_para_pcb(int, pcb*, op_code);
void paqueteIO(int, SOLICITUD_INTERFAZ*, cont_exec*);
void paqueT_dato(int, t_dato*);
void paquete_creacion_proceso(int, c_proceso_data*);
void paquete_solicitud_instruccion(int, t_instruccion*);
void paquete_llegada_io_memoria(int, DATOS_CONEXION*);
void paquete_resize(int, t_resize*);
void paquete_nueva_IO(int, INTERFAZ*);
void paquete_guardar_en_memoria(int, pcb*);
void enviar_contexto_pcb(int, cont_exec*, op_code);
void paquete_memoria_io(int, char*);
void paquete_leer_memoria(int, PAQUETE_LECTURA*);
void paquete_escribir_memoria(int, PAQUETE_ESCRITURA*);
void paquete_marco(int, PAQUETE_MARCO*);
void paquete_copy_string(int, PAQUETE_COPY_STRING*);

// FUNCIONES SERVER
typedef struct {
    t_log* logger;
    char* puerto_escucha;
} ArgsAbrirServidor;

extern t_log* logger;

typedef struct {
	t_log* logger;
	int cliente_fd;
	char* nombre;
}ArgsGestionarServidor;

typedef struct {
	t_log* logger;
	DATOS_CONEXION* datos;
}args_gestionar_interfaz;

typedef struct {
	t_log* logger;
	int cliente_fd;
	INTERFAZ* interfaz;
}ArgsGestionarHiloInterfaz;

typedef struct consola{
	t_log* logger;	
}ArgsLeerConsola;

void* recibir_buffer(int*, int);
int iniciar_servidor(t_log* logger, char* puerto_escucha);
int esperar_cliente(int server_fd, t_log* logger);
t_list* recibir_paquete(int, t_log*);
void* recibir_mensaje(int, t_log*, op_code);
int recibir_operacion(int);

#endif