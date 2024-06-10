#include <memoria.h>
#include <unistd.h>

int cliente_fd_cpu;
int cliente_fd_kernel;
int retardo_respuesta;
int retardo_en_segundos;
int id_de_tablas=0;
int cant_pag=0;

t_log *logger_memoria;
t_config *config_memoria;

t_list *pseudocodigo;
t_list *lista_tabla_pagina;
MEMORIA *memoria;

t_queue *procesos_activos;

sem_t carga_instrucciones;
sem_t descarga_instrucciones;
sem_t paso_instrucciones;

char *path;
char *path_instructions;

pthread_t hilo[3];

int enlistar_pseudocodigo(char *path_instructions, char *path, t_log *logger, t_list *pseudocodigo)
{
    char instruccion[30];
    
    char *full_path = strdup(path_instructions);
    strcat(full_path, path);

    FILE *f = fopen(full_path, "rb");

    if (f == NULL)
    {
        log_error(logger_memoria, "No se pudo abrir el archivo de %s (ERROR: %s)", full_path, strerror(errno));
        return EXIT_FAILURE;
    }

    while (!feof(f))
    {
        inst_pseudocodigo *inst_a_lista = malloc(sizeof(inst_pseudocodigo));
        char* linea_instruccion = fgets(instruccion, sizeof(instruccion), f);
        inst_a_lista->instruccion = strdup(linea_instruccion);
        list_add(pseudocodigo, inst_a_lista);
        linea_instruccion = NULL;
    }

    paginar_proceso(pseudocodigo);

    iterar_lista_e_imprimir(pseudocodigo);

    free(full_path);
    full_path = NULL;
    fclose(f);

    sem_post(&paso_instrucciones);

    return EXIT_SUCCESS;
}

void enviar_instrucciones_a_cpu(char *program_counter, int retardo_respuesta)
{
    int pc = atoi(program_counter);

    if (!list_is_empty(pseudocodigo))
    {
        inst_pseudocodigo *inst_a_mandar  = list_get(pseudocodigo, pc);
        log_info(logger_memoria, "Enviaste la instruccion n°%d: %s a CPU exitosamente", pc, inst_a_mandar->instruccion);
        paqueteDeMensajes(cliente_fd_cpu, inst_a_mandar->instruccion, INSTRUCCION);
    }else{  
        paqueteDeMensajes(cliente_fd_cpu, "EXIT", INSTRUCCION);
    }

    
    sem_post(&paso_instrucciones);
}

void paginar_proceso(t_list *pseudocodigo) {
    for(int i = 0; i < list_size(pseudocodigo); i++) {
        printf("INSTRUCTION: %s", list_get(pseudocodigo, i));
    }
}

void inicializar_memoria(MEMORIA* memoria, int num_marcos, int tam_marcos) {
    MARCO_MEMORIA* marcos = malloc(num_marcos * sizeof(uint32_t));

    for(int i = 0; i < num_marcos; i++) {
        marcos[i].data = malloc(sizeof(uint32_t));
    }

    memoria = malloc(sizeof(MEMORIA));
    memoria->numero_marcos = num_marcos;
    memoria->marcos = marcos;
}

void resetear_memoria(MEMORIA *memoria) { 
    memoria = NULL;
    memoria->marcos = NULL;

    for(int i = 0; i < memoria->numero_marcos; i++) {
        memoria->marcos[i].data = NULL;
        free(memoria->marcos[i].data);
    }

    free(memoria->marcos);
    free(memoria);
}

void iterar_lista_e_imprimir(t_list *lista)
{
    t_list_iterator *lista_a_iterar = list_iterator_create(lista);
    if (lista_a_iterar != NULL)
    {
        printf(" Lista de instrucciones : [ ");
        while (list_iterator_has_next(lista_a_iterar))
        {
            inst_pseudocodigo *elemento_actual = list_iterator_next(lista_a_iterar);
            if (list_iterator_has_next(lista_a_iterar))
            {
                printf("%s <- ", elemento_actual->instruccion);
            }
            else
            {
                printf("%s", elemento_actual->instruccion);
            }
        }
        printf(" ]\tElementos totales: %d\n", list_size(lista));
    }
    list_iterator_destroy(lista_a_iterar);
}

int main(int argc, char *argv[])
{
    sem_init(&carga_instrucciones, 1, 1);
    sem_init(&descarga_instrucciones, 1, 0);
    sem_init(&paso_instrucciones, 1, 0);

    logger_memoria = iniciar_logger("memoria.log", "memoria-log", LOG_LEVEL_INFO);
    log_info(logger_memoria, "Logger Creado.");

    config_memoria = iniciar_config("../memoria/memoria.config");
    char* puerto_escucha = config_get_string_value(config_memoria, "PUERTO_ESCUCHA");
    retardo_respuesta = config_get_int_value(config_memoria, "RETARDO_RESPUESTA"); 
    int tamanio_pagina=config_get_int_value(config_memoria,"TAM_PAGINA");
    int tamanio_memoria=config_get_int_value(config_memoria,"TAM_MEMORIA");
    path_instructions = config_get_string_value(config_memoria, "PATH_INSTRUCCIONES");
    
    cant_pag=tamanio_memoria/tamanio_pagina;
    retardo_en_segundos = (retardo_respuesta / 1000);

    inicializar_memoria(memoria, tamanio_pagina, cant_pag);

    pseudocodigo = list_create();
    lista_tabla_pagina = list_create();

    int server_memoria = iniciar_servidor(logger_memoria, puerto_escucha);
    log_info(logger_memoria, "Servidor a la espera de clientes");

    cliente_fd_cpu = esperar_cliente(server_memoria, logger_memoria);
    cliente_fd_kernel = esperar_cliente(server_memoria, logger_memoria);
    // int cliente_fd_tres = esperar_cliente(server_memoria, logger_memoria);

    paqueteDeMensajes(cliente_fd_cpu, string_itoa(tamanio_pagina), MENSAJE);

    ArgsGestionarServidor args_sv1 = {logger_memoria, cliente_fd_cpu};
    ArgsGestionarServidor args_sv2 = {logger_memoria, cliente_fd_kernel};
    // ArgsGestionarServidor args_sv3 = {logger_memoria, cliente_fd_tres};

    pthread_create(&hilo[0], NULL, gestionar_llegada_memoria_cpu, &args_sv1);
    pthread_create(&hilo[1], NULL, gestionar_llegada_memoria_kernel, &args_sv2);
    // pthread_create(&hilo[2], NULL, gestionar_llegada, &args_sv3);

    for (int i = 0; i < 3; i++)
    {
        pthread_join(hilo[i], NULL);
    }

    sem_destroy(&carga_instrucciones);
    sem_destroy(&descarga_instrucciones);
    sem_destroy(&paso_instrucciones);
    
    return 0;
}

void *gestionar_llegada_memoria_cpu(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case MENSAJE:
            lista = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
            break;
        case INSTRUCCION:
            sem_wait(&paso_instrucciones);
            sleep(retardo_en_segundos);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_memoria);
            char *program_counter = list_get(lista, 0);
            log_info(logger_memoria, "Me solicitaron la instruccion n°%s", program_counter);
            enviar_instrucciones_a_cpu(program_counter, retardo_respuesta);
            break;
        case DESCARGAR_INSTRUCCIONES:
            sem_wait(&paso_instrucciones);
            sem_wait(&descarga_instrucciones);
            char* mensaje = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, DESCARGAR_INSTRUCCIONES);
            list_clean_and_destroy_elements(pseudocodigo, destruir_instrucciones);
            free(mensaje);
            mensaje = NULL;
            sem_post(&carga_instrucciones);
            break;
        case -1:
            log_error(logger_memoria, "el cliente se desconecto. Terminando servidor");
            return (void *)EXIT_FAILURE;
        default:
            log_warning(logger_memoria, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

void *gestionar_llegada_memoria_kernel(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case MENSAJE:
            lista = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
            break;
        case CREAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, args_entrada->logger);
            char* instrucciones = list_get(lista, 0);
            pcb *new = crear_pcb(instrucciones);
            log_info(logger_memoria, "-Espacio asignado para nuevo proceso-");
            peticion_de_espacio_para_pcb(cliente_fd_kernel, new, CREAR_PROCESO);
            break;
        case FINALIZAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, args_entrada->logger);
            pcb *a_eliminar = list_get(lista, 0);
            a_eliminar->path_instrucciones = list_get(lista, 1);
            a_eliminar->estadoActual = list_get(lista, 2);
            a_eliminar->estadoAnterior = list_get(lista, 3);
            a_eliminar->contexto = list_get(lista, 4);
            a_eliminar->contexto->registros = list_get(lista, 5);
            destruir_pcb(a_eliminar);
            paqueteDeMensajes(cliente_fd_kernel, "Succesful delete. Coming back soon!\n", FINALIZAR_PROCESO);
            break;
        case CARGAR_INSTRUCCIONES:
            sem_wait(&carga_instrucciones);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_memoria);
            char *path = list_get(lista, 0);
            printf("\n------------------------------NUEVAS INSTRUCCIONES------------------------------\n");
            enlistar_pseudocodigo(path_instructions, path, logger_memoria, pseudocodigo);
            sem_post(&descarga_instrucciones);
            break;
        case -1:
            log_error(logger_memoria, "el cliente se desconecto. Terminando servidor");
            return (void *)EXIT_FAILURE;
        default:
            log_warning(logger_memoria, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}
//PAGINADO
//VOY A TENER UNA TB X PCB Y pcb_nuevo->contexto->registros->ptbr APUNTA A SU TB CORRESPONDIENTE
void tradurcirDireccion(){
//   unsigned int desplazamiento= (direccionLogica%TAM_PAGINA);
//    unsigned int dl=(nro_pag*TAM_PAG)+desplazamiento;
//    unsigned int numero_marco = proceso->tabla_paginas[numero_pagina];
//    unsigned int df= (numero_marco * TAMANO_PAGINA) + desplazamiento;
}

void lista_tablas(TABLA_PAGINA* tb){
    TABLAS* tabla=malloc(sizeof(TABLAS));
    tabla->pid=id_de_tablas;
    tabla->tabla_pagina=tb;
    list_add(lista_tabla_pagina,tabla);
    id_de_tablas++;
}

uint32_t* inicializar_tabla_pagina(char* instrucciones) {
    TABLA_PAGINA* tabla_pagina = malloc(cant_pag*sizeof(TABLA_PAGINA));
    //pcb_nuevo->contexto->registros->PTLR//espacio de memoria del proceso
        for(int i=0;i<=cant_pag;i++){//cada 32 char cambiar a la siguiente pagina hacerlo con esto strcpy
             tabla_pagina[i].marcos=malloc(sizeof(uint32_t));
             tabla_pagina[i].bit_validacion=false;
        }
    lista_tablas(tabla_pagina);
    return &tabla_pagina->marcos[0];
}

void ajustar_tamaño(){
}

unsigned int acceso_a_tabla_de_páginas(int index, int pid){
    TABLA_PAGINA* tb = list_get(lista_tabla_pagina,pid);
        for(int i=0;i<cant_pag;i++){
            if(i==index){
                return tb[i].marcos;
            }
        }
    return -1;
}

//PROCESO
pcb *crear_pcb(char* instrucciones)
{
    pcb *pcb_nuevo = malloc(sizeof(pcb));
    pcb_nuevo->recursos_adquiridos=list_create();

    eliminarEspaciosBlanco(instrucciones);
    pcb_nuevo->path_instrucciones = strdup(instrucciones);

    pcb_nuevo->contexto = malloc(sizeof(cont_exec));
    pcb_nuevo->contexto->registros = malloc(sizeof(regCPU));
    pcb_nuevo->contexto->registros->PTBR = NULL;
    
    // Implementacion de tabla vacia de paginas
    // pcb_nuevo->contexto->registros->PTBR = inicializar_tabla_pagina(pcb_nuevo->path_instrucciones);//puntero a la tb
    return pcb_nuevo;
}

void destruir_pagina(void* data){
    TABLAS* destruir = (TABLAS*) data;
    free(destruir->tabla_pagina);
    destruir->tabla_pagina=NULL;
    free(destruir);
    destruir=NULL;
}

void destruir_tabla(int pid){
    list_remove_and_destroy_element(lista_tabla_pagina,pid,destruir_pagina);
}

void destruir_pcb(pcb *elemento)
{  
    //destruir_tabla(elemento->contexto->PID); 
    free(elemento->contexto->registros);
    elemento->contexto->registros = NULL;
    free(elemento->contexto);
    elemento->contexto = NULL;
    free(elemento->estadoAnterior);
    elemento->estadoAnterior = NULL;
    free(elemento->estadoActual);
    elemento->estadoActual = NULL;
    free(elemento->path_instrucciones);
    elemento->path_instrucciones = NULL;
    free(elemento);
    elemento = NULL;
}

void destruir_instrucciones(void* data){
    inst_pseudocodigo *elemento = (inst_pseudocodigo*)data;
    free(elemento->instruccion);
    elemento->instruccion = NULL;
    elemento = NULL;
}