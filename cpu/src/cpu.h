#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>

#endif

// Funciones basicas de CPU
char* Fetch(contEXEC*);
void Decode();
void Execute();
void CheckInterrupt();
// ------------------------