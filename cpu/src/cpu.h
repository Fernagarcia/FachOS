#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>
#include <utils/parse.h>
#include <stdbool.h>

// Funciones basicas de CPU
void Fetch(cont_exec*);
RESPONSE* Decode(char*);
void Execute();
void check_interrupt(char*);
// ------------------------

void* gestionar_llegada_cpu(void*);

void iterator_cpu(t_log*, char*);

// Instrucciones

typedef struct {
    char *command;
    void (*function)(char **, cont_exec*);
    char *description;
} INSTRUCTION;
typedef struct {
    const char *name;
    void *ptr;
    char* type;
} REGISTER;

REGISTER* find_register(const char*, cont_exec*);

// Instructions definition

void set(char**, cont_exec*);
void mov_in(char**, cont_exec*);
void mov_out(char**, cont_exec*);
void sum(char**, cont_exec*);
void sub(char**, cont_exec*);
void jnz(char**, cont_exec*);
void mov(char**, cont_exec*);
void resize(char**, cont_exec*);
void copy_string(char**, cont_exec*);
void wait(char**, cont_exec*);
void SIGNAL(char**, cont_exec*);
void io_gen_sleep(char**, cont_exec*);
void io_stdin_read(char**, cont_exec*);
void EXIT(char**, cont_exec*);

//
void solicitar_interfaz(char*, char*, char**);

#endif