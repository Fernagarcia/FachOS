#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>
//STRUCTS

typedef struct datos_a_memoria{
    void* data;
    char tipo;
}t_dato;

typedef struct{
    int pid;
    t_list* instrucciones;
}instrucciones_a_memoria;

typedef struct{
    char* instruccion;
}inst_pseudocodigo;

typedef struct {
    void* data;
} MARCO_MEMORIA;

typedef struct {
    MARCO_MEMORIA *marcos;
    int numero_marcos;
    int tam_marcos;
} MEMORIA;

//MEMORIA
void inicializar_memoria(MEMORIA*, int, int);
void resetear_memoria(MEMORIA*);
bool guardar_en_memoria(MEMORIA*, t_dato*, t_list*);
int buscar_marco_disponible();
int determinar_sizeof(t_dato*);
int verificar_marcos_disponibles(int);
int size_memoria_restante();
void escribir_en_memoria(char*, void*, char*);
void* leer_en_memoria(char*, char*, char*);
void aumentar_tamanio_tabla(TABLA_PAGINA*, int);
int cantidad_marcos_disponibles();
bool reservar_memoria(TABLA_PAGINA*, int);

//PAGINADO
TABLA_PAGINA* inicializar_tabla_pagina();
void lista_tablas(TABLA_PAGINA*);
void destruir_tabla_pag_proceso(int pid);
void destruir_tabla();
void ajustar_tamaño(TABLA_PAGINA*, char*);
unsigned int acceso_a_tabla_de_páginas(int, int);

//PSEUDOCODIGO
void enlistar_pseudocodigo(char*, t_log*, t_list*);
void iterar_lista_e_imprimir(t_list*);
void iterar_pseudocodigo_e_imprimir(t_list*);

//CONEXIONES
void* gestionar_llegada_memoria_cpu(void*);
void* gestionar_llegada_memoria_kernel(void*);
void* gestionar_llegada_memoria_io(void*);
void enviar_instrucciones_a_cpu(char*,char*);

//PROCESOS
pcb* crear_pcb(c_proceso_data);
void destruir_pcb(pcb*);
void destruir_instrucciones(void*);
bool es_pid_de_tabla(int, void*);
bool son_inst_pid(int pid, void* data);
void destruir_memoria_instrucciones(int pid);

#endif