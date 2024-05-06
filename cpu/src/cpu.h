#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>

#endif

typedef struct {
  char *name;			/* Nombre de la funcion ingresada por consola */
  char **params;
  Function *func;		/* Funcion a la que se va a llamar  */
  char *doc;			/* Descripcion de lo que va a hacer la funcion  */
} INSTRUCTION;

// Funciones basicas de CPU
char* Fetch(contEXEC*);
void Decode();
void Execute();
void CheckInterrupt();
// ------------------------