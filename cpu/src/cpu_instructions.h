#ifndef CPU_INSTRUCTIONS_H
#define CPU_INSTRUCTIONS_H

#include<utils/utils.h>
#include <stdbool.h>

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


#endif