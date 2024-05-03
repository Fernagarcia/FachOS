#ifndef KERNEL_H_
#define KERNEL_H_

#include<sys/types.h>
#include<sys/file.h>
#include<sys/stat.h>
#include<sys/errno.h>
#include<utils/utils.h>

typedef struct sv_kernel{
    t_log* logger;
    int sv_kernel;
}args_inicializar_servidor;

void* leer_consola();
void iterar_cola_e_imprimir(t_queue*);
void planificadorCortoPlazo();
void FIFO();
void cambiar_pcb_de_cola(t_queue*, t_queue*, pcb*);
int buscar_y_borrar_pcb_en_cola(t_queue* , int);

/* Funciones de la consola interactiva TODO: Cambiar una vez realizadas las funciones */
int ejecutar_script(char*);
void iniciar_proceso(char*);
int finalizar_proceso(char*);
int iniciar_planificacion();
int detener_planificacion();
void multiprogramacion(char*);
void proceso_estado();

/* Estructura que los comandos a ejecutar en la consola pueden entender */
typedef struct {
  char *name;			/* Nombre de la funcion ingresada por consola */
  Function *func;		/* Funcion a la que se va a llamar  */
  char *doc;			/* Descripcion de lo que va a hacer la funcion  */
} COMMAND;

// Declaraciones de la consola interactiva

char* dupstr (char* s);
int execute_line(char*, t_log*);
COMMAND* find_command (char*);
char* stripwhite (char*);

bool es_igual_a(int,void*);
void destruir_pcb(void*);

#endif

