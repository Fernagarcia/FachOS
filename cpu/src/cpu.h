#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>
#include <utils/parse.h>

// Funciones basicas de CPU
void Fetch(regCPU*);
RESPONSE* Decode(char*);
void Execute();
void CheckInterrupt();
// ------------------------

void* gestionar_llegada_cpu(void*);

void iterator_cpu(t_log*, char*);

// Instrucciones

#endif