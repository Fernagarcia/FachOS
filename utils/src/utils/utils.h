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
#include<readline/history.h>
#include<pthread.h>

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

// FUNCIONES UTILS 

t_log* iniciar_logger(char* log_path, char* log_name, t_log_level log_level);
t_config* iniciar_config(char* config_path);
void terminar_programa(t_log* logger, t_config* config);

// FUNCIONES CLIENTE

int crear_conexion(char* ip, char* puerto);
void enviar_mensaje(char* mensaje, int socket_cliente);
t_paquete* crear_paquete(void);
void paquete(int conexion);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void liberar_conexion(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
void leer_consola(t_log*);

// FUNCIONES SERVER

typedef struct {
    t_log* logger;
    char* puerto_escucha;
} ArgsAbrirServidor;

extern t_log* logger;

typedef struct gestionar{
	t_log* logger;
	int server_fd;
}ArgsGestionarServidor;

void* recibir_buffer(int*, int);
void* gestionar_llegada(void*);
int iniciar_servidor(t_log* logger, char* puerto_escucha);
int esperar_cliente(int server_fd, t_log* logger);
t_list* recibir_paquete(int);
void recibir_mensaje(int, t_log* logger);
int recibir_operacion(int);
void iterator(t_log* logger, char* value);

#endif