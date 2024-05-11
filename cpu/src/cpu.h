#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>

#endif

typedef struct {
    char *command;
    void (*function)(char **);
    char *description;
} INSTRUCTION;

// Funciones basicas de CPU
char* Fetch(contextoDeEjecucion*);
void Execute();
void CheckInterrupt();
// ------------------------

void* gestionar_llegada_cpu(void*);

void iterator_cpu(t_log*, char*);

// Instrucciones
