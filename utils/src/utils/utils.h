#ifndef UTILS_H_
#define UTILS_H_

#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<errno.h>
#include<utils/utils.h>
#include<commons/log.h>
#include<commons/config.h>
#include<commons/collections/list.h>
#include<readline/readline.h>
#include <readline/history.h>
#include<commons/collections/queue.h>
typedef enum
{
	MENSAJE,
	PAQUETE
}op_code;

typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

typedef struct registroCPU
{
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
}

typedef struct contextoDeEjecucion
{
	registroCPU registro;
}

enum estado{
	NEW,
	READY,
	EXECUTE,
	BLOCKED,
	EXIT
}
// deberia de tener una lista con los procesos a ejecutar, es una estructura con una lista dentro.
typedef struct PCB
{
	int PID;
	int quantum;
	// Implementar datos faltantes del PCB llegado el debido momento
	contextoDeEjecucion contexto;
	estado estado;
}


// FUNCIONES UTILS 

t_log* iniciar_logger(char* log_path, char* log_name, t_log_level log_level);
t_config* iniciar_config(char* config_path);
void terminar_programa(t_log* logger, t_config* config);

// FUNCIONES CLIENTE

int crear_conexion(char* ip, char* puerto);
void enviar_mensaje(char* mensaje, int socket_cliente);
t_paquete* crear_paquete(void);
void paquete(int conexion);
void paquetePCB(int conexion,contextoDeEjecucion contexto);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void liberar_conexion(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
void leer_consola(t_log*);

// FUNCIONES SERVER

extern t_log* logger;

void* recibir_buffer(int*, int);

int gestionar_llegada(t_log* logger, int server_fd);
int iniciar_servidor(t_log* logger, char* puerto_escucha);
int esperar_cliente(int server_fd, t_log* logger);
t_list* recibir_paquete(int);
void recibir_mensaje(int, t_log* logger);
int recibir_operacion(int);
void iterator(t_log* logger, char* value);

#endif