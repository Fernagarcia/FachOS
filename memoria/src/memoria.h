#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>
//STRUCTS
typedef struct{
    char* instruccion;
}inst_pseudocodigo;
typedef struct {
    unsigned int* marcos;
    bool bit_validacion;
} TABLA_PAGINA;

//DEPENDIENDO EL PID DEL PROCESO VA A TENER TAL TABLA EJ PID=1 TABLA=1 :3
typedef struct{
    int id_tabla;
    TABLA_PAGINA* tabla_pagina;
}TABLAS;

//PAGINADO
unsigned int inicializar_tabla_pagina();
void lista_tablas(TABLA_PAGINA*);
void destruir_pagina(void*);
void destruir_tabla(int);
void tradurcirDireccion();


//PSEUDOCODIGO
int enlistar_pseudocodigo(char* path_instructions, char* ,t_log*, t_list*);
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