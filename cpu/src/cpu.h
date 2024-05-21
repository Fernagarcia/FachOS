#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>

#endif

// Funciones basicas de CPU
void Fetch(regCPU* registro);
void Execute();
void CheckInterrupt();
// ------------------------

void* gestionar_llegada_cpu(void*);

void iterator_cpu(t_log*, char*);

// Instrucciones
