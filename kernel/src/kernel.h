#ifndef KERNEL_H_
#define KERNEL_H_

#include<utils/utils.h>

void* leer_consola();
void iterar_cola_e_imprimir(t_queue*);
void iterar_lista_e_imprimir(t_list*);
void* FIFO();
void* RR();
pcb* buscar_pcb_en_cola(t_queue* cola, int PID);
int liberar_recursos(int, MOTIVO_SALIDA);
void* gestionar_llegada_kernel_cpu(void* args);
void* gestionar_llegada_io_kernel(void* args);
void* gestionar_llegada_kernel_memoria(void* args);
op_code determinar_operacion_io(INTERFAZ*);
INTERFAZ* asignar_espacio_a_io(t_list*);


// Movilizacion de pcbs por colas (REPITEN LOGICA PERO SON AUXILIARES PARA CAMBIAR ESTADOS INTERNOS DE LOS PCB)

void cambiar_de_new_a_ready(pcb* pcb);
void cambiar_de_ready_a_execute(pcb* pcb);
void cambiar_de_execute_a_blocked(pcb* pcb);
void cambiar_de_blocked_a_ready(pcb* pcb);
void cambiar_de_execute_a_exit(pcb* pcb);
void cambiar_de_new_a_exit(pcb* pcb);
void cambiar_de_ready_a_exit(pcb* pcb);
void cambiar_de_blocked_a_exit(pcb* pcb);
void cambiar_de_execute_a_ready(pcb* pcb);

/* Funciones de la consola interactiva TODO: Cambiar una vez realizadas las funciones */
int ejecutar_script(char*);
int iniciar_proceso(char*);
int finalizar_proceso(char*);
int iniciar_planificacion();
int detener_planificacion();
int multiprogramacion(char*);
int proceso_estado();
int interfaces_conectadas();

/* Estructura que los comandos a ejecutar en la consola pueden entender */
typedef struct {
  char *name;			/* Nombre de la funcion ingresada por consola */
  Function *func;		/* Funcion a la que se va a llamar  */
  char *doc;			/* Descripcion de lo que va a hacer la funcion  */
} COMMAND;


//ESTRUCTURA DE INTERFACES
//TODO no esta implementado los semaforos.


// Declaraciones de la consola interactiva

char* dupstr (char* s);
int execute_line(char*, t_log*);
COMMAND* find_command (char*);
char* stripwhite (char*);
bool es_igual_a(int, void*);
bool lista_seek_interfaces(char*);
bool lista_validacion_interfaces(INTERFAZ*, char*);
INTERFAZ* interfaz_encontrada(char*);


#endif

