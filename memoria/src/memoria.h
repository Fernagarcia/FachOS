#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>
//STRUCTS
typedef struct{
    int pid;
    t_list* instrucciones;
}instrucciones_a_memoria;

typedef struct{
    char* instruccion;
}inst_pseudocodigo;

typedef struct{
    int nro_marco;
    int offset;
}direccion_fisica;

typedef struct {
    int tamanio;
    void* data;
} MARCO_MEMORIA;

typedef struct {
    MARCO_MEMORIA *marcos;
    int numero_marcos;
    int tam_marcos;
} MEMORIA;

//MEMORIA
void inicializar_memoria(MEMORIA*, int, int);
void resetear_memoria(MEMORIA*);
void guardar_en_memoria(direccion_fisica, t_dato*, TABLA_PAGINA*);
void guardar_en_memoria_v2(direccion_fisica, t_dato*, TABLA_PAGINA*);
bool verificar_marcos_disponibles(int);
void escribir_en_memoria(char*, void*, char*);
void* leer_en_memoria(char*, char*, char*);
bool reservar_memoria(TABLA_PAGINA*, int);
void asignar_marco_a_pagina(PAGINA*, int);
direccion_fisica obtener_marco_y_offset(int);

//PAGINADO
TABLA_PAGINA* inicializar_tabla_pagina();
t_list* crear_tabla_de_paginas();
void lista_tablas(TABLA_PAGINA*);
void destruir_tabla_pag_proceso(int pid);
void destruir_tabla();
void ajustar_tamanio(TABLA_PAGINA*, char*);
unsigned int acceso_a_tabla_de_páginas(int, int);
int ultima_pagina_usada(t_list*);
int cantidad_de_paginas_usadas(TABLA_PAGINA*);

bool pagina_vacia(void*);
bool pagina_sin_frame(void*);
bool pagina_asociada_a_marco(int, void*);

//PSEUDOCODIGO
void enlistar_pseudocodigo(char*, t_log*, t_list*);
void iterar_lista_e_imprimir(t_list*);
void iterar_pseudocodigo_e_imprimir(t_list*);

//CONEXIONES
void* gestionar_llegada_memoria_cpu(void*);
void* gestionar_llegada_memoria_kernel(void*);
void* gestionar_llegada_memoria_io(void*);
void enviar_instrucciones_a_cpu(char*,char*);

//PROCESOS
pcb* crear_pcb(c_proceso_data);
void destruir_pcb(pcb*);
void destruir_instrucciones(void*);
bool es_pid_de_tabla(int, void*);
bool son_inst_pid(int pid, void* data);
void destruir_memoria_instrucciones(int pid);

//BITMAP
char* crear_bitmap();
void establecer_bit(int, bool);
void imprimir_bitmap();
bool obtener_bit(int);
void liberar_bitmap();
int buscar_marco_libre();

//INTERFACES
INTERFAZ* asignar_espacio_a_io(t_list*);
void *esperar_nuevo_io();
void iterar_lista_interfaces_e_imprimir(t_list*);
int interfaces_conectadas();

#endif