#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>
//STRUCTS
typedef struct{
    char* instruccion;
}inst_pseudocodigo;
typedef struct{
    int pid;
    TABLA_PAGINA* tabla_pagina;
}TABLAS;
typedef struct {
    void* data;
} MARCO_MEMORIA;

typedef struct {
    MARCO_MEMORIA *marcos;
    int numero_marcos;
    int tam_marcos;
} MEMORIA;

//MEMORIA
void resetear_memoria(MEMORIA*);

//PAGINADO
PAGINA* inicializar_tabla_pagina();
void lista_tablas(TABLA_PAGINA*);
void destruir_pagina(void*);
void destruir_tabla(int);
void tradurcirDireccion();
int guardar_en_memoria(MEMORIA*,t_list*, PAGINA*);

//PSEUDOCODIGO
int enlistar_pseudocodigo(char*, char*, t_log*, t_list*, PAGINA*);
void iterar_lista_e_imprimir(t_list*);

//CONEXIONES
void* gestionar_llegada_memoria_cpu(void*);
void* gestionar_llegada_memoria_kernel(void*);
void* gestionar_llegada_memoria_io(void*);
void enviar_instrucciones_a_cpu(char*, int);

//PROCESOS
pcb* crear_pcb(char* instrucciones);
void destruir_pcb(pcb*);
void destruir_instrucciones(void*);

#endif