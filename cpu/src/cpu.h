#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>
#include <utils/parse.h>
#include <stdbool.h>

// Funciones basicas de CPU
void Fetch(regCPU*);
RESPONSE* Decode(char*);
void Execute();
void CheckInterrupt();
// ------------------------

void* gestionar_llegada_cpu(void*);

void iterator_cpu(t_log*, char*);

// Instrucciones

typedef struct {
    char *command;
    void (*function)(char **, regCPU*);
    char *description;
} INSTRUCTION;
typedef struct {
    const char *name;
    void *ptr;
    char* type;
} REGISTER;

REGISTER* find_register(const char*, regCPU*);

// Instructions definition

void set(char**, regCPU*);
void mov_in(char**, regCPU*);
void mov_out(char**, regCPU*);
void sum(char**, regCPU*);
void sub(char**, regCPU*);
void jnz(char**, regCPU*);
void mov(char**, regCPU*);
void resize(char**, regCPU*);
void copy_string(char**, regCPU*);
void wait(char**, regCPU*);
void SIGNAL(char**, regCPU*);
void io_gen_sleep(char**, regCPU*);
void io_stdin_read(char**, regCPU*);
void EXIT(char**, regCPU*);

//
void solicitar_interfaz(char*, char*, char**);

typedef struct SOLICITUD_INTERFAZ{
  char* nombre;
  char* solicitud;
  char** args;
} SOLICITUD_INTERFAZ;

#endif