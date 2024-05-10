#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>

void enviar_instrucciones_a_cpu(char*);
void encolarPseudocodigo(char*, t_log*);

void* gestionar_llegada_memoria(void*);
void iterator_memoria(t_log*, char*);
#endif