#include <memoria.h>
#include <unistd.h>

int cliente_fd_cpu;
int cliente_fd_kernel;
int retardo_respuesta;
int retardo_en_segundos;
int id_de_tablas=0;
int cant_pag=0;
int index_marco = 0;

t_log *logger_general;
t_log *logger_instrucciones;
t_log *logger_procesos_creados;
t_log *logger_procesos_finalizados;
t_config *config_memoria;

t_list *lista_tabla_pagina;
MEMORIA *memoria;

t_queue *procesos_activos;

sem_t carga_instrucciones;
sem_t descarga_instrucciones;
sem_t paso_instrucciones;

char *path_instructions;

pthread_t hilo[3];

int enlistar_pseudocodigo(char *path_instructions, char *path, t_log *logger, PAGINA *tabla_pagina){
    char instruccion[50] = {0};
    bool response;
    char *full_path = strdup(path_instructions);
    strcat(full_path, path);

    FILE *f = fopen(full_path, "r");

    if (f == NULL)
    {
        log_error(logger_instrucciones, "No se pudo abrir el archivo de %s (ERROR: %s)", full_path, strerror(errno));
        return EXIT_FAILURE;
    }

    while (fgets(instruccion, sizeof(instruccion), f) != NULL)
    {
        t_dato* instruccion_a_guardar = malloc(sizeof(t_dato));
        instruccion_a_guardar->data = strdup(instruccion);
        instruccion_a_guardar->tipo = 's';
        
        response = guardar_en_memoria(memoria, instruccion_a_guardar, tabla_pagina);
        
        free(instruccion_a_guardar);
        instruccion_a_guardar = NULL;
    
        if(!response){
            break;
        }
    }


    free(full_path);
    full_path = NULL;
    fclose(f);

    sem_post(&paso_instrucciones);

    return response;
}

/*void enviar_instrucciones_a_cpu(char *program_counter, int retardo_respuesta)
{
    int pc = atoi(program_counter);

    if (!list_is_empty(pseudocodigo))
    {
        inst_pseudocodigo *inst_a_mandar  = list_get(pseudocodigo, pc);
        log_info(logger_instrucciones, "Enviaste la instruccion n°%d: %s a CPU exitosamente", pc, inst_a_mandar->instruccion);
        paqueteDeMensajes(cliente_fd_cpu, inst_a_mandar->instruccion, INSTRUCCION);
    }else{  
        paqueteDeMensajes(cliente_fd_cpu, "EXIT", INSTRUCCION);
    }

    
    sem_post(&paso_instrucciones);
}*/

//TODO: Chequear que todo el proceso entre segun la disponibilidad de los marcos

int guardar_en_memoria(MEMORIA* memoria, t_dato* dato_a_guardar, PAGINA* tabla_pagina) {
    int bytes_a_copiar = determinar_sizeof(dato_a_guardar);
    int tamanio_de_pagina = memoria->tam_marcos;
    
    int cantidad_de_pag_a_usar = (int)ceil((double)bytes_a_copiar/(double)tamanio_de_pagina);

    for (int pagina = 1; pagina <= cantidad_de_pag_a_usar; pagina++) {
        int tamanio_a_copiar = (bytes_a_copiar >= tamanio_de_pagina) ? tamanio_de_pagina : bytes_a_copiar;
        void* t_dato = malloc(tamanio_a_copiar);

        memcpy(t_dato, &dato_a_guardar, tamanio_a_copiar);

        int marco_disponible = buscar_marco_disponible();

        memoria->marcos[marco_disponible].data = t_dato;

        dato_a_guardar += tamanio_a_copiar;
        bytes_a_copiar -= tamanio_a_copiar;

        printf("Posicion de marco: %d Direccion instruccion: %p\n", marco_disponible, &memoria->marcos[marco_disponible].data);
    }

   return 1;
}

void inicializar_memoria(MEMORIA* memoria, int num_marcos, int tam_marcos) {
    memoria->marcos = malloc(num_marcos * tam_marcos);

    for(int i = 0; i < num_marcos; i++) {
        memoria->marcos[i].data = NULL;
    }

    memoria->numero_marcos = num_marcos;
    memoria->tam_marcos = tam_marcos;
}

void resetear_memoria(MEMORIA *memoria) { 
    for(int i = 0; i < memoria->numero_marcos; i++) {
        memoria->marcos[i].data = NULL;
        free(memoria->marcos[i].data);
    }

    free(memoria->marcos);
    memoria->marcos = NULL;
    free(memoria);
    memoria = NULL;
}

int buscar_marco_disponible(){
    int nro_marco = 0;
    while(memoria->marcos[nro_marco].data != NULL){
        nro_marco++;
    }
    return nro_marco;
}

int determinar_sizeof(t_dato* dato_a_guardar){
    switch (dato_a_guardar->tipo)
    {
        case 's':
            return strlen((char*)dato_a_guardar->data);
        case 'd':
            return sizeof(int8_t);
        case 'l':
            return sizeof(int32_t);
    }
    return 0;
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

    logger_general = iniciar_logger("mgeneral.log", "memoria_general.log", LOG_LEVEL_INFO);
    logger_instrucciones = iniciar_logger("instructions.log", "instructions.log", LOG_LEVEL_INFO);
    logger_procesos_finalizados = iniciar_logger("fprocess.log", "finalize_process.log", LOG_LEVEL_INFO);
    logger_procesos_creados = iniciar_logger("cprocess.log", "create_process.log", LOG_LEVEL_INFO);

    config_memoria = iniciar_config("../memoria/memoria.config");
    char* puerto_escucha = config_get_string_value(config_memoria, "PUERTO_ESCUCHA");
    retardo_respuesta = config_get_int_value(config_memoria, "RETARDO_RESPUESTA"); 
    int tamanio_pagina=config_get_int_value(config_memoria,"TAM_PAGINA");
    int tamanio_memoria=config_get_int_value(config_memoria,"TAM_MEMORIA");
    path_instructions = config_get_string_value(config_memoria, "PATH_INSTRUCCIONES");
    
    cant_pag = tamanio_memoria/tamanio_pagina;
    retardo_en_segundos = (retardo_respuesta / 1000);

    memoria = malloc(sizeof(MEMORIA));
    inicializar_memoria(memoria, cant_pag, tamanio_pagina);

    //Banco de pruebas

        PAGINA* tabla = inicializar_tabla_pagina();

        t_dato* dato_a_guardar = malloc(sizeof(t_dato));
        dato_a_guardar->data = (int*)100;
        dato_a_guardar->tipo = 'd';

        guardar_en_memoria(memoria, dato_a_guardar, tabla);

        free(dato_a_guardar);

        int i = 0;
        while(memoria->marcos[i].data != NULL){
            printf("%d", *(int*)memoria->marcos[i].data);
            i++;
        }



    lista_tabla_pagina = list_create();

    int server_memoria = iniciar_servidor(logger_general, puerto_escucha);
    log_info(logger_general, "Servidor a la espera de clientes");

    cliente_fd_cpu = esperar_cliente(server_memoria, logger_general);
    cliente_fd_kernel = esperar_cliente(server_memoria, logger_general);
    // int cliente_fd_tres = esperar_cliente(server_memoria, logger_memoria);

    paqueteDeMensajes(cliente_fd_cpu, string_itoa(tamanio_pagina), MENSAJE);

    ArgsGestionarServidor args_sv1 = {logger_instrucciones, cliente_fd_cpu};
    ArgsGestionarServidor args_sv2 = {logger_procesos_creados, cliente_fd_kernel};
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

    terminar_programa(logger_general, config_memoria);
    log_destroy(logger_instrucciones);
    log_destroy(logger_procesos_creados);
    log_destroy(logger_procesos_finalizados);

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
            lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
            char *program_counter = list_get(lista, 0);
            log_info(logger_instrucciones, "Me solicitaron la instruccion n°%s", program_counter);
            //enviar_instrucciones_a_cpu(program_counter, retardo_respuesta);
            break;
        case DESCARGAR_INSTRUCCIONES:
            sem_wait(&paso_instrucciones);
            sem_wait(&descarga_instrucciones);
            char* mensaje = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, DESCARGAR_INSTRUCCIONES);
            //list_clean_and_destroy_elements(pseudocodigo, destruir_instrucciones);
            free(mensaje);
            mensaje = NULL;
            sem_post(&carga_instrucciones);
            break;
        case -1:
            log_error(logger_general, "el cliente se desconecto. Terminando servidor");
            return (void *)EXIT_FAILURE;
        default:
            log_warning(logger_general, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

void *gestionar_llegada_memoria_kernel(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    char* path;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case MENSAJE:
            lista = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
            break;
        case CREAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_procesos_creados);
            char* instrucciones = list_get(lista, 0);
            pcb *new = crear_pcb(instrucciones);
            log_info(logger_procesos_creados, "-Espacio asignado para nuevo proceso-");
            peticion_de_espacio_para_pcb(cliente_fd_kernel, new, CREAR_PROCESO);
            break;
        case FINALIZAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_procesos_finalizados);
            pcb *a_eliminar = list_get(lista, 0);
            a_eliminar->path_instrucciones = list_get(lista, 1);
            a_eliminar->estadoActual = list_get(lista, 2);
            a_eliminar->estadoAnterior = list_get(lista, 3);
            a_eliminar->contexto = list_get(lista, 4);
            a_eliminar->contexto->registros = list_get(lista, 5);
            destruir_pcb(a_eliminar);
            paqueteDeMensajes(cliente_fd_kernel, "Succesful delete. Coming back soon!\n", FINALIZAR_PROCESO);
            break;
        /*case CARGAR_INSTRUCCIONES:
            sem_wait(&carga_instrucciones);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
            path = list_get(lista, 0);
            printf("\n------------------------------NUEVAS INSTRUCCIONES------------------------------\n");
            enlistar_pseudocodigo(path_instructions, path, logger_instrucciones, pseudocodigo);
            sem_post(&descarga_instrucciones);
            break;
        */
        case SOLICITUD_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_general);
            path = list_get(lista, 0);
            int response;
            PAGINA* tabla_pagina = list_get(lista, 1);

            printf("\n\nSe necesita cargar las instrucciones con path: %s\n", path);
            printf("Direccion de tabla de paginas: %p\n\n", &tabla_pagina);

            response = enlistar_pseudocodigo(path_instructions, path, logger_instrucciones, tabla_pagina);
            if (response != -1) {
                paqueteDeMensajes(cliente_fd_kernel, string_itoa(response), MEMORIA_ASIGNADA);
            }
            break;
        case -1:
            log_error(logger_general, "el cliente se desconecto. Terminando servidor");
            return (void *)EXIT_FAILURE;
        default:
            log_warning(logger_general, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

void lista_tablas(TABLA_PAGINA* tb){
    TABLAS* tabla=malloc(sizeof(TABLAS));
    tabla->pid=id_de_tablas;
    tabla->tabla_pagina=tb;
    list_add(lista_tabla_pagina,tabla);
    id_de_tablas++;
}

PAGINA* inicializar_tabla_pagina() {
    TABLA_PAGINA* tabla_pagina = malloc(sizeof(TABLA_PAGINA));
    tabla_pagina->paginas = malloc(cant_pag*sizeof(PAGINA));
    //pcb_nuevo->contexto->registros->PTLR//espacio de memoria del proceso
    for(int i=0;i<cant_pag;i++){
         tabla_pagina->paginas->marco = NULL;
         tabla_pagina->paginas->bit_validacion = NULL;
    }
    //lista_tablas(tabla_pagina);
    return tabla_pagina->paginas;
}

void destruir_pagina(void* data){
    TABLA_PAGINA* destruir = (TABLA_PAGINA*) data;
    free(destruir->paginas);
    destruir->paginas=NULL;
    free(destruir);
    destruir=NULL;
}


unsigned int acceso_a_tabla_de_páginas(int index, int pid){
    TABLA_PAGINA* tb = list_get(lista_tabla_pagina,pid);
        for(int i=0;i<cant_pag;i++){
            if(i==index){
                return tb->paginas[i].marco;
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
    pcb_nuevo->contexto->registros->PTBR = inicializar_tabla_pagina();//puntero a la tb
    return pcb_nuevo;
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
    elemento->estadoAnterior = NULL;
    elemento->estadoActual = NULL;
    free(elemento->path_instrucciones);
    elemento->path_instrucciones = NULL;
    elemento = NULL;
}

void destruir_instrucciones(void* data){
    inst_pseudocodigo *elemento = (inst_pseudocodigo*)data;
    free(elemento->instruccion);
    elemento->instruccion = NULL;
    elemento = NULL;
}