#ifndef CPU_INSTRUCTIONS_H
#define CPU_INSTRUCTIONS_H
#include<iostream>
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
} REGISTER;

void* find_register(const char*, regCPU*);

// Instructions definition

void set(**char, regCPU*);
void mov_in(**char, regCPU*);
void mov_out(**char, regCPU*);
void sum(**char, regCPU*);
void sub(**char, regCPU*);
void jnz(**char, regCPU*);
void mov(**char, regCPU*);
void resize(**char, regCPU*);
void copy_string(**char, regCPU*);
void wait(**char, regCPU*);
void signal(**char, regCPU*);
void io_gen_sleep(**char, regCPU*);
void io_stdin_read(**char, regCPU*);


INSTRUCTION instructions[] = {
    { "SET", set, "Ejecutar SET" },
    { "MOV_IN", mov_in, "Ejecutar MOV_IN"},
    { "MOV_OUT", mov_out, "Ejecutar MOV_OUT"},
    { "SUM", sum, "Ejecutar SUM"},
    { "SUB", sub, "Ejecutar SUB"},
    { "JNZ", jnz, "Ejecutar JNZ"},
    { "MOV", mov, "Ejecutar MOV"},
    { "RESIZE", resize, "Ejecutar RESIZE"},
    { "COPY_STRING", copy_string, "Ejecutar COPY_STRING"},
    { "WAIT", wait, "Ejecutar WAIT"},
    { "SIGNAL", signal, "Ejecutar SIGNAL"},
    { "IO_GEN_SLEEP", io_gen_sleep, "Ejecutar IO_GEN_SLEEP"},
    { "IO_STDIN_READ", io_stdin_read, "Ejecutar IO_STDIN_READ"},
    { NULL, NULL, NULL }
};

#endif