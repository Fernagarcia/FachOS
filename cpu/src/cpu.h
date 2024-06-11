#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>
#include <utils/parse.h>
#include <stdbool.h>

// Funciones basicas de CPU
void Fetch(cont_exec*);
RESPONSE* Decode(char*);
void Execute();
void check_interrupt(int);
void alertar_interrupcion();

// ------------------------

void* gestionar_llegada_memoria(void*);
void* gestionar_llegada_kernel(void*);

void iterator_cpu(t_log*, char*);
void mmu (char* direccion_logica);

// Instrucciones

typedef struct {
    char *command;
    void (*function)(char **);
    char *description;
} INSTRUCTION;

typedef struct {
    int pid;
    int pagina;
    int marco;
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
TLB *inicializar_tlb(char* entradas);
int chequear_en_tlb(char* pagina);
int tlb_controller(char* pagina);

#endif