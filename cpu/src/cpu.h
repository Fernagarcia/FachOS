#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>

#endif

// Funciones basicas de CPU
void Fetch(PCB* pcb);
void Decode();
void Execute();
void CheckInterrupt();
// ------------------------