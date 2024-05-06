#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>
#include <pthread.h>

void enviar_instrucciones_a_cpu(char*);
void encolarPseudocodigo(char*, t_log*);
#endif