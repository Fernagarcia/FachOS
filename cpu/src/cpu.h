#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>
#include <utils/parse.h>
#include <stdbool.h>

typedef struct {
    int pagina;
    int offset;
}DIRECCION_LOGICA;

// Funciones basicas de CPU
void procesar_contexto(cont_exec*);
void Fetch(cont_exec*);
RESPONSE* Decode(char*);
void Execute();
t_config* iniciar_configuracion();

// ------------------------

void* gestionar_llegada_memoria(void*);
void* gestionar_llegada_kernel(void*);

char* traducirDireccionLogica(DIRECCION_LOGICA);
void iterator_cpu(t_log*, char*);
char* mmu (DIRECCION_LOGICA direccion_logica);

// Instrucciones

typedef struct {
    char *command;
    void (*function)(char **);
    char *description;
    int posicion_direccion_logica;
} INSTRUCTION;

typedef struct {
    int pid;
    int pagina;
    int marco;
    t_temporal* last_access;
} TLBEntry;

typedef struct {
   t_list *entradas; 
} TLB;

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

// Instructions definition

void set(char**);
void mov_in(char **);
void mov_out(char**);
void sum(char**);
void sub(char**);
void jnz(char**);
void resize(char**);
void copy_string(char**);
void WAIT(char**);
void SIGNAL(char**);
void io_gen_sleep(char**);
void io_stdin_read(char**);
void io_stdout_write(char**);
void io_fs_create(char**);
void io_fs_delete(char**);
void io_fs_trucate(char**);
void io_fs_read(char**);
void io_fs_write(char**);
void EXIT(char**);

//
void solicitar_interfaz(char*, char*, char**);
bool es_motivo_de_salida(const char *command); 
op_code determinar_op(char*);

//TLB
bool es_pid_pag(int, int, void*);
TLB *inicializar_tlb(int entradas);
int chequear_en_tlb(int, int);
void agregar_en_tlb_fifo(int, int, int);
void agregar_en_tlb_lru(int, int, int);
void agregar_en_tlb(int, int, int);
void actualizar_marco_tlb(int, int, int);

DIRECCION_LOGICA obtener_pagina_y_offset(int);

#endif