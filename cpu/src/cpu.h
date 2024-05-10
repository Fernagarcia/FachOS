#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>
#include <utils/semaforos.h>

#endif

typedef struct {
  char *name;			/* Nombre de la funcion ingresada por consola */
  char **params;
  Function *func;		/* Funcion a la que se va a llamar  */
  char *doc;			/* Descripcion de lo que va a hacer la funcion  */
} INSTRUCTION;

// Funciones basicas de CPU
char* Fetch(contEXEC*);
bool Decode(char*);
void Execute();
void CheckInterrupt();
// ------------------------

void* gestionar_llegada_cpu(void*);

void iterator_cpu(t_log*, char*);

// Instrucciones

int set();
