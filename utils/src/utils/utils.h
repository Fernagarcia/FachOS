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
#include<commons/memory.h>

#include<readline/readline.h>
#include<readline/history.h>



typedef enum operaciones{
	MENSAJE,
	PAQUETE,
	INSTRUCCION,
	PATH
}op_code;

typedef struct{
	int size;
	void* stream;
} t_buffer;

typedef struct{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

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
	char* prueba;
}regCPU;

/*enum state{
	NEW,
	READY,
	EXECUTE,
	BLOCKED,
	EXIT
};*/

typedef struct pcb{
	int PID;
	int quantum;
	regCPU* contexto;
	char* estadoAnterior;
	char* estadoActual;
	char* path_instrucciones;
}pcb;

// FUNCIONES UTILS 

t_log* iniciar_logger(char* log_path, char* log_name, t_log_level log_level);
t_config* iniciar_config(char* config_path);
void terminar_programa(t_log* logger, t_config* config);

// FUNCIONES CLIENTE

int crear_conexion(char* ip, char* puerto);
void enviar_operacion(char* mensaje, int socket_cliente, op_code);
t_paquete* crear_paquete(void);
void paqueteDeMensajes(int conexion);
void paqueteDePCB(int conexion, pcb* pcb);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void liberar_conexion(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);


// FUNCIONES SERVER

typedef struct {
    t_log* logger;
    char* puerto_escucha;
} ArgsAbrirServidor;

extern t_log* logger;

typedef struct gestionar{
	t_log* logger;
	int cliente_fd;
}ArgsGestionarServidor;

typedef struct consola{
	t_log* logger;	
}ArgsLeerConsola;

void* recibir_buffer(int*, int);
void* gestionar_llegada(void*);
int iniciar_servidor(t_log* logger, char* puerto_escucha);
int esperar_cliente(int server_fd, t_log* logger);
t_list* recibir_paquete(int);
void* recibir_mensaje(int, t_log*, op_code);
int recibir_operacion(int);
void iterator(t_log* logger, char* value);

#endif