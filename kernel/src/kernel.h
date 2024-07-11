#ifndef KERNEL_H_
#define KERNEL_H_

#include<utils/utils.h>



// ALGORITMOS DE PLANIFICACION
#pragma region ALGORITMOS_DE_PLANIFICACION
void* FIFO();
void* RR();
void* VRR();

typedef enum{
  ALG_FIFO,
  ALG_RR,
  ALG_VRR,
  ERROR
}ALG_PLANIFICACION;

ALG_PLANIFICACION determinar_planificacion(char*);
#pragma endregion

// FUNCIONES KERNEL
#pragma region FUNCIONES_KERNEL
void* leer_consola();
t_config* iniciar_configuracion();
int redondear_quantum(int tiempo);
void iterar_cola_e_imprimir(t_queue*);
void iterar_lista_interfaces_e_imprimir(t_list*);
void iterar_lista_recursos_e_imprimir(t_list*);
pcb* buscar_pcb_en_cola(t_queue* cola, int PID);
void liberar_todos_recursos_asignados(pcb*);
int liberar_recursos(int, MOTIVO_SALIDA);
void* gestionar_llegada_kernel_cpu(void* args);
void* gestionar_llegada_io_kernel(void* args);
void* gestionar_llegada_kernel_memoria(void* args);
void abrir_hilo_interrupcion(int);
void* interrumpir_por_quantum(void*);
void llenar_lista_de_recursos(char**, char**, t_list*);
void eliminar_recursos(void*);
bool es_t_recurso_buscado(char*, void*);
bool es_p_recurso_buscado(char*, void*);
void liberar_instancia_recurso(pcb*, char*);
void asignar_instancia_recurso(pcb*, char*);
void checkear_pasaje_a_ready();
bool proceso_posee_recurso(pcb*, char*);
int procesos_bloqueados_en_recursos();
#pragma endregion

// FUNCIONES IO
#pragma region FUNCIONES_IO
op_code determinar_operacion_io(INTERFAZ*);
INTERFAZ* asignar_espacio_a_io(t_list*);
void checkear_estado_interfaz(INTERFAZ*, pcb*);
void desocupar_io(INTERFAZ*);
void liberar_solicitud_de_desbloqueo(desbloquear_io*);
void *esperar_nuevo_io();
void guardar_solicitud_a_io(SOLICITUD_INTERFAZ*);
bool es_solicitud_de_pid(int, void*);
#pragma endregion

// FUNCIONES PCB
#pragma region FUNCIONES_PCB
void cambiar_de_new_a_ready(pcb* pcb);
void cambiar_de_ready_a_execute(pcb* pcb);
void cambiar_de_ready_prioridad_a_execute(pcb *pcb);
void cambiar_de_execute_a_blocked(pcb* pcb);
void cambiar_de_execute_a_blocked_io(pcb*, INTERFAZ*);
void cambiar_de_blocked_io_a_ready(pcb*, INTERFAZ*);
void cambiar_de_blocked_io_a_ready_prioridad(pcb*, INTERFAZ*);
void cambiar_de_blocked_a_ready(pcb* pcb);
void cambiar_de_blocked_a_ready_prioridad(pcb *pcb);
void cambiar_de_execute_a_exit(pcb* pcb);
void cambiar_de_new_a_exit(pcb* pcb);
void cambiar_de_ready_a_exit(pcb* pcb);
void cambiar_de_blocked_a_exit(pcb* pcb);
void cambiar_de_execute_a_ready(pcb* pcb);
void cambiar_de_blocked_a_resourse_blocked(pcb*, char*);
void cambiar_de_resourse_blocked_a_ready_prioridad(pcb*, char*);
void cambiar_de_resourse_blocked_a_ready(pcb*, char*);
void cambiar_de_resourse_blocked_a_exit(pcb*, char*);
void cambiar_de_blocked_a_ready_prioridad_first(pcb*);
void cambiar_de_blocked_a_ready_first(pcb*);
#pragma endregion

// FUNCIONES DE CONSOLA INTERATIVA
#pragma region CONSOLA_INTERATIVA
int ejecutar_script(char*);
int iniciar_proceso(char*);
int finalizar_proceso(char*);
int iniciar_planificacion();
int detener_planificacion();
int multiprogramacion(char*);
int proceso_estado();
int interfaces_conectadas();
int recursos_actuales();
int algoritmo_planificacion(char*);

// CONSOLA INTERATIVA
char* dupstr (char* s);
int execute_line(char*, t_log*);
COMMAND* find_command (char*);
char* stripwhite (char*);
bool es_igual_a(int, void*);
bool lista_seek_interfaces(char*);
bool lista_validacion_interfaces(INTERFAZ*, char*);
INTERFAZ* interfaz_encontrada(char*);
void limpiar_recurso(void*);
#pragma endregion

// ESTRUCTURA DE COMANDOS A LEER
typedef struct {
  char *name;			/* Nombre de la funcion ingresada por consola */
  Function *func;		/* Funcion a la que se va a llamar  */
  char *doc;			/* Descripcion de lo que va a hacer la funcion  */
} COMMAND;

typedef struct{
  int tiempo_a_esperar;
}args_hilo_interrupcion;

#endif

