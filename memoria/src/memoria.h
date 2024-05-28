#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>

void enviar_instrucciones_a_cpu(char*);
int enlistar_pseudocodigo(char* path_instructions, char* ,t_log*, t_list*);

void* gestionar_llegada_memoria(void*);
void iterator_memoria(void*);

pcb* crear_pcb();
void destruir_pcb(pcb*);

void iterar_lista_e_imprimir(t_list*);
void destruir_instrucciones(void*);
void borrar_lista(t_list*);

#endif