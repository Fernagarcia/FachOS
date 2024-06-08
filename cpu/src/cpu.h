#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>
#include <utils/parse.h>
#include <stdbool.h>

// Funciones basicas de CPU
void Fetch(cont_exec*);
RESPONSE* Decode(char*);
void Execute();
int check_interrupt(char*);
// ------------------------

void* gestionar_llegada_cpu(void*);

void iterator_cpu(t_log*, char*);

// Instrucciones

typedef struct {
    char *command;
    void (*function)(char **);
    char *description;
} INSTRUCTION;

typedef enum {
    TYPE_UINT8,
    TYPE_UINT32
} REGISTER_TYPE;

typedef struct {
    const char *name;
    void* registro;
    REGISTER_TYPE type;
} REGISTER;

REGISTER* find_register(const char*);
void upload_register_map();

typedef struct {
    char* nroPagina;
    char* nroMarco;
} TLB;

// Instructions definition

void set(char**);
void mov_in(char**);
void mov_out(char**);
void sum(char**);
void sub(char**);
void jnz(char**);
void mov(char**);
void resize(char**);
void copy_string(char**);
void WAIT(char**);
void SIGNAL(char**);
void io_gen_sleep(char**);
void io_stdin_read(char**);
void EXIT(char**);

//
void solicitar_interfaz(char*, char*, char**);
bool es_motivo_de_salida(const char *command); 

#endif