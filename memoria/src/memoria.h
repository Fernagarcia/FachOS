#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>
//STRUCTS

typedef struct datos_a_memoria{
    void* data;
    char tipo;
}t_dato;

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
TABLA_PAGINA* inicializar_tabla_pagina();
void lista_tablas(TABLA_PAGINA*);
void destruir_pagina(void*);
void destruir_tabla(int);
void tradurcirDireccion();
int guardar_en_memoria(MEMORIA*, t_dato*, t_list*);
int buscar_marco_disponible();
int determinar_sizeof(t_dato*);

//PSEUDOCODIGO
int enlistar_pseudocodigo(char*, char*, t_log*, TABLA_PAGINA*);
void iterar_lista_e_imprimir(t_list*);

//CONEXIONES
void* gestionar_llegada_memoria_cpu(void*);
void* gestionar_llegada_memoria_kernel(void*);
void enviar_instrucciones_a_cpu(char*, int);

//PROCESOS
pcb* crear_pcb(char* instrucciones);
void destruir_pcb(pcb*);
void destruir_instrucciones(void*);

#endif