#include <memoria.h>
#include <unistd.h>

int cliente_fd_cpu;
int cliente_fd_kernel;
int cliente_fd_io;
int retardo_respuesta;
int grado_multiprogramacion;

t_log *logger_general;
t_log *logger_instrucciones;
t_log *logger_procesos_creados;
t_log *logger_procesos_finalizados;
t_config *config_memoria;

t_list *memoria_de_instrucciones;
t_list *tablas_de_paginas;
MEMORIA *memoria;

sem_t paso_instrucciones;

pthread_t hilo[4];

void enlistar_pseudocodigo(char *path, t_log *logger, t_list *pseudocodigo){
    char instruccion[50] = {0};
    char* path_instructions = config_get_string_value(config_memoria, "PATH_INSTRUCCIONES");

    char* cabeza_path = malloc(strlen(path_instructions) + 1 + strlen(path) + 1);
    strcpy(cabeza_path, path_instructions);
    strcat(cabeza_path, path);

    FILE *f = fopen(cabeza_path, "r");

    if (f == NULL)
    {
        log_error(logger_instrucciones, "No se pudo abrir el archivo de %s (ERROR: %s)", cabeza_path, strerror(errno));
        return;
    }

    while (fgets(instruccion, sizeof(instruccion), f) != NULL)
    {
        inst_pseudocodigo* instruccion_a_guardar = malloc(sizeof(inst_pseudocodigo));
        instruccion_a_guardar->instruccion = strdup(instruccion);
        list_add(pseudocodigo, instruccion_a_guardar);
    }

    fclose(f);

    free(cabeza_path);
    cabeza_path = NULL;
}


void enviar_instrucciones_a_cpu(char *program_counter, char* pid)
{
    // Si la TLB envia el marco directamente entonces devolvemos la instruccion directa de memoria.
    int pc = atoi(program_counter);
    int id_p = atoi(pid);

    bool son_inst_pid_aux(void* data){
        return son_inst_pid(id_p, data);
    };

    instrucciones_a_memoria* inst_proceso = list_find(memoria_de_instrucciones, son_inst_pid_aux);

    if (list_get(inst_proceso->instrucciones, pc) != NULL)
    {
        inst_pseudocodigo* instruccion = list_get(inst_proceso->instrucciones, pc);
        log_info(logger_instrucciones, "Enviaste la instruccion n°%d: %s a CPU exitosamente", pc, instruccion->instruccion);
        paqueteDeMensajes(cliente_fd_cpu, instruccion->instruccion, RESPUESTA_MEMORIA);
    }else{ 
        paqueteDeMensajes(cliente_fd_cpu, "EXIT", RESPUESTA_MEMORIA);
    }

    sem_post(&paso_instrucciones);
}

bool guardar_en_memoria(MEMORIA* memoria, t_dato* dato_a_guardar, t_list* paginas) {
    int bytes_a_copiar = determinar_sizeof(dato_a_guardar);
    int tamanio_de_pagina = memoria->tam_marcos;
    int cantidad_de_pag_solicitadas = (int)ceil((double)bytes_a_copiar/(double)tamanio_de_pagina);
    int index_marco = verificar_marcos_disponibles(cantidad_de_pag_solicitadas);

    if(index_marco != -1){
        for (int pagina = 1; pagina <= cantidad_de_pag_solicitadas; pagina++) {
            int tamanio_a_copiar = (bytes_a_copiar >= tamanio_de_pagina) ? tamanio_de_pagina : bytes_a_copiar;
            void* t_dato = malloc(tamanio_a_copiar);

            memcpy(t_dato, dato_a_guardar->data, tamanio_a_copiar);

            memoria->marcos[index_marco].data = t_dato;

            dato_a_guardar += tamanio_a_copiar;
            bytes_a_copiar -= tamanio_a_copiar;

            PAGINA* set_pagina = list_get(paginas, 0);
            set_pagina->bit_validacion = true;
            set_pagina->marco = index_marco;

            index_marco++;

            printf("Posicion de marco: %d Direccion de dato en marco: %p\n", index_marco, &memoria->marcos[index_marco].data);
        }

        return true;     
    }
   return false;
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
    int cant_pag = memoria->numero_marcos;
    
    while(memoria->marcos[nro_marco].data != NULL){
        nro_marco++;
    }

    if(nro_marco > (cant_pag - 1)){
        return -1;
    }else{
        return nro_marco;
    }
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

int verificar_marcos_disponibles(int cantidad_de_pag_solicitadas){
    int contador = 0;

    for(int i = 0; i < memoria->numero_marcos; i++) {
        if(memoria->marcos[i].data == NULL) {
            if(contador == cantidad_de_pag_solicitadas) {
                return (i - cantidad_de_pag_solicitadas);
            }
            contador++;
        } else {
            contador = 0;
        }
    }

    return -1;
}

void iterar_tabla_de_paginas_e_imprimir(t_list *lista)
{
    t_list_iterator *lista_a_iterar = list_iterator_create(lista);
    if (lista_a_iterar != NULL)
    {
        printf(" Lista de instrucciones : [ ");
        while (list_iterator_has_next(lista_a_iterar))
        {
            PAGINA *pagina = list_iterator_next(lista_a_iterar);
            if (list_iterator_has_next(lista_a_iterar))
            {
                printf("%d <- ", pagina->marco);
            }
            else
            {
                printf("%d", pagina->marco);
            }
        }
        printf(" ]\tElementos totales: %d\n", list_size(lista));
    }
    list_iterator_destroy(lista_a_iterar);
}

void iterar_pseudocodigo_e_imprimir(t_list *lista)
{
    t_list_iterator *lista_a_iterar = list_iterator_create(lista);
    if (lista_a_iterar != NULL)
    {
        printf(" Lista de instrucciones : [ ");
        while (list_iterator_has_next(lista_a_iterar))
        {
            inst_pseudocodigo *inst = list_iterator_next(lista_a_iterar);
            if (list_iterator_has_next(lista_a_iterar))
            {
                printf("%s <- ", inst->instruccion);
            }
            else
            {
                printf("%s", inst->instruccion);
            }
        }
        printf(" ]\tElementos totales: %d\n", list_size(lista));
    }
    list_iterator_destroy(lista_a_iterar);
}

int main(int argc, char *argv[])
{
    sem_init(&paso_instrucciones, 1, 1);

    logger_general = iniciar_logger("mgeneral.log", "memoria_general.log", LOG_LEVEL_INFO);
    logger_instrucciones = iniciar_logger("instructions.log", "instructions.log", LOG_LEVEL_INFO);
    logger_procesos_finalizados = iniciar_logger("fprocess.log", "finalize_process.log", LOG_LEVEL_INFO);
    logger_procesos_creados = iniciar_logger("cprocess.log", "create_process.log", LOG_LEVEL_INFO);

    config_memoria = iniciar_config("../memoria/memoria.config");
    char* puerto_escucha = config_get_string_value(config_memoria, "PUERTO_ESCUCHA");
    retardo_respuesta = config_get_int_value(config_memoria, "RETARDO_RESPUESTA"); 
    int tamanio_pagina=config_get_int_value(config_memoria,"TAM_PAGINA");
    int tamanio_memoria=config_get_int_value(config_memoria,"TAM_MEMORIA");
    
    int cant_pag = tamanio_memoria/tamanio_pagina;
   
    memoria = malloc(sizeof(MEMORIA));
    inicializar_memoria(memoria, cant_pag, tamanio_pagina);

    tablas_de_paginas = list_create();

    memoria_de_instrucciones = list_create();
    
    /*Banco de pruebas

        TABLA_PAGINA* tabla = inicializar_tabla_pagina();

        t_dato* dato_a_guardar = malloc(sizeof(t_dato));
        dato_a_guardar->data = (int*)100;
        dato_a_guardar->tipo = 'd';

        enlistar_pseudocodigo("scripts_memoria/IO_A", logger_general, tabla);
        enlistar_pseudocodigo("scripts_memoria/IO_B", logger_general, tabla);
        enlistar_pseudocodigo("scripts_memoria/IO_C", logger_general, tabla);
        enlistar_pseudocodigo("scripts_memoria/IO_D", logger_general, tabla);

    */
    int server_memoria = iniciar_servidor(logger_general, puerto_escucha);
    log_info(logger_general, "Servidor a la espera de clientes");

    cliente_fd_cpu = esperar_cliente(server_memoria, logger_general);
    cliente_fd_kernel = esperar_cliente(server_memoria, logger_general);
    // int cliente_fd_tres = esperar_cliente(server_memoria, logger_memoria);
    cliente_fd_io = esperar_cliente(server_memoria, logger_general);

    paqueteDeMensajes(cliente_fd_cpu, string_itoa(tamanio_pagina), MENSAJE);

    ArgsGestionarServidor args_sv1 = {logger_instrucciones, cliente_fd_cpu};
    ArgsGestionarServidor args_sv2 = {logger_procesos_creados, cliente_fd_kernel};
    ArgsGestionarServidor args_sv3 = {logger_general, cliente_fd_io};

    pthread_create(&hilo[0], NULL, gestionar_llegada_memoria_cpu, &args_sv1);
    pthread_create(&hilo[1], NULL, gestionar_llegada_memoria_kernel, &args_sv2);
    pthread_create(&hilo[2],NULL, gestionar_llegada_memoria_io, &args_sv3 );

    for (int i = 0; i < 4; i++)
    {
        pthread_join(hilo[i], NULL);
    }

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
        char *index_marco;
        char* pid;
        switch (cod_op)
        {
        case MENSAJE:
            lista = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
            break;
        case INSTRUCCION:
            sem_wait(&paso_instrucciones);
            usleep(retardo_respuesta * 1000);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
            char *program_counter = list_get(lista, 0);
            pid = list_get(lista, 1);
            log_info(logger_instrucciones, "Proceso n°%d solicito la instruccion n°%s.\n", atoi(pid), program_counter);
            enviar_instrucciones_a_cpu(program_counter, pid);
            break;
        case LEER_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
            index_marco = list_get(lista, 0);
            pid = list_get(lista, 1);

            void* response;

            response = leer_en_memoria(index_marco, string_itoa(memoria->tam_marcos), pid);

            paqueteDeMensajes(cliente_fd_cpu, response, RESPUESTA_LEER_MEMORIA);
            break;
        case ESCRIBIR_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
            index_marco = list_get(lista, 0);
            pid = list_get(lista, 1);
            void* dato_a_escribir = list_get(lista, 2);

            if(sizeof(dato_a_escribir) == 8) {
                uint8_t *dato_a_escribir_8 = (uint8_t*)dato_a_escribir;
                escribir_en_memoria(index_marco, dato_a_escribir_8, pid);
            } else {
                uint32_t *dato_a_escribir_32 = (uint32_t*)dato_a_escribir;
                escribir_en_memoria(index_marco, dato_a_escribir_32, pid);
            }
            break;
        case RESIZE:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
            t_resize* info_rsz = list_get(lista, 0);
            info_rsz->tamanio = list_get(lista, 1);

            bool es_pid_de_tabla_aux(void* data){
                return es_pid_de_tabla(info_rsz->pid, data);
            };

            TABLA_PAGINA* tabla = list_find(tablas_de_paginas, es_pid_de_tabla_aux);

            ajustar_tamaño(tabla, info_rsz->tamanio);
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
    char* pid;
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
            c_proceso_data data;
            pid = list_get(lista, 0);
            data.id_proceso = atoi(pid);
            data.path = list_get(lista, 1);
            pcb *new = crear_pcb(data);
            log_info(logger_procesos_creados, "-Espacio asignado para nuevo proceso-");
            peticion_de_espacio_para_pcb(cliente_fd_kernel, new, CREAR_PROCESO);
            break;
        case FINALIZAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_procesos_finalizados);
            pcb *a_eliminar = list_get(lista, 0);
            a_eliminar->estadoActual = list_get(lista, 1);
            a_eliminar->estadoAnterior = list_get(lista, 2);
            a_eliminar->contexto = list_get(lista, 3);
            a_eliminar->contexto->registros = list_get(lista, 4);
            a_eliminar->contexto->registros->PTBR = list_get(lista, 5);
            destruir_pcb(a_eliminar);
            paqueteDeMensajes(cliente_fd_kernel, "Succesful delete. Coming back soon!", FINALIZAR_PROCESO);
            break;
        case SOLICITUD_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_general);
            pid = list_get(lista, 0);
            int id_proceso = atoi(pid);
            bool response;

            log_info(logger_procesos_creados, "Se solicito espacio para proceso");

            bool es_pid_de_tabla_aux(void* data){
                return es_pid_de_tabla(id_proceso, data);
            };

            TABLA_PAGINA* tabla_de_proceso = list_find(tablas_de_paginas, es_pid_de_tabla_aux);

            int cant_pag_por_proceso = (int)floor((double)(memoria->numero_marcos/grado_multiprogramacion));

            response = reservar_memoria(tabla_de_proceso, cant_pag_por_proceso);
            
            if(response){
                paqueteDeMensajes(cliente_fd_kernel, string_itoa(1), MEMORIA_ASIGNADA);
            }else{
                paqueteDeMensajes(cliente_fd_kernel, string_itoa(-1), MEMORIA_ASIGNADA);
            }
            break;
        case MULTIPROGRAMACION:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_general);
            char* mulp = list_get(lista, 0);
            grado_multiprogramacion = atoi(mulp);
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

bool es_pid_de_tabla(int pid, void* data){
    TABLA_PAGINA* tabla = (TABLA_PAGINA*)data;
    return tabla->pid == pid;
}

TABLA_PAGINA* inicializar_tabla_pagina(int pid) {
    TABLA_PAGINA* tabla_pagina = malloc(sizeof(TABLA_PAGINA));
    tabla_pagina->pid = pid;
    tabla_pagina->paginas = list_create();

    list_add(tablas_de_paginas, tabla_pagina);

    //TODO: Ver logica de bit a nivel de la estructura de la tabla de paginas

    return tabla_pagina;
}

bool reservar_memoria(TABLA_PAGINA* tabla_de_proceso, int cantidad){
    
    int index_marco = verificar_marcos_disponibles(cantidad);
    
    if(index_marco != -1){
        for(int i = 0; i < cantidad; i++){
            PAGINA* new_pagina = malloc(sizeof(PAGINA));
            new_pagina->marco = index_marco;
            new_pagina->bit_validacion = true;
            list_add(tabla_de_proceso->paginas, new_pagina);
            index_marco++;
        }
        return true;
    }
    return false;
}

int cantidad_marcos_disponibles(){
    int i = memoria->numero_marcos;
    for(int j = 1; j < list_size(tablas_de_paginas); j++){
        TABLA_PAGINA* tabla_proceso = list_get(tablas_de_paginas, j);
        i -= list_size(tabla_proceso->paginas);
    }  
    return i;
}

void destruir_tabla_pag_proceso(int pid){
    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(pid, data);
    };

    TABLA_PAGINA* destruir = list_find(tablas_de_paginas, es_pid_de_tabla_aux);
    list_destroy_and_destroy_elements(destruir->paginas, free);
    destruir->paginas=NULL;
    free(destruir);
    destruir=NULL;
}

unsigned int acceso_a_tabla_de_páginas(int pid,int pagina){
    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(pid, data);
    };
    TABLA_PAGINA* tabla = list_find(tablas_de_paginas,es_pid_de_tabla_aux);
    PAGINA* pag = list_get(tabla->paginas, pagina);
    log_info(logger_general, "PID: %d - Pagina: %d - Marco: %d\n", tabla->pid, pagina, pag->marco);
    return pag->marco;
}

void aumentar_tamanio_tabla(TABLA_PAGINA* tabla, int marco_disponible){
    PAGINA* nueva_pagina = malloc(sizeof(PAGINA));
    nueva_pagina->marco = marco_disponible;
    nueva_pagina->bit_validacion = true;

    list_add(tabla->paginas, nueva_pagina);
}
// planteamiento general cantAumentar claramente esta mal, pero es una idea de como seria

void ajustar_tamaño(TABLA_PAGINA* tabla,  char* tamanio){
    int tamanio_solicitado = atoi(tamanio);
    int cantidad_de_pag_solicitadas = (int)ceil((double)tamanio_solicitado/(double)(memoria->tam_marcos));

    int tam_lista = list_size(tabla->paginas);

    if(tam_lista > cantidad_de_pag_solicitadas){
        log_info(logger_instrucciones, "PID: %d - Tamanio actual: %d - Tamanio a reducir: %d\n", tabla->pid, tam_lista, cantidad_de_pag_solicitadas);

        for(int j = (tam_lista - 1); j > (cantidad_de_pag_solicitadas - 1); j--){
            PAGINA* pagina_a_borrar = list_get(tabla->paginas, j);    
            
            if(pagina_a_borrar->marco != -1){
                free(memoria->marcos[pagina_a_borrar->marco].data);
                memoria->marcos[pagina_a_borrar->marco].data = NULL;
            }

            list_remove_and_destroy_element(tabla->paginas, j, free);
        }   
        iterar_tabla_de_paginas_e_imprimir(tabla->paginas);
        paqueteDeMensajes(cliente_fd_cpu, "Se disminuyo la cantidad de paginas correctamente", RESIZE);

    }else{
        log_info(logger_instrucciones, "PID: %d - Tamanio actual: %d - Tamanio a ampliar: %d\n", tabla->pid, tam_lista, cantidad_de_pag_solicitadas);
        int marcos_necesarios = cantidad_de_pag_solicitadas - tam_lista;
        int marcos_disponibles = cantidad_marcos_disponibles();

        if(marcos_necesarios <= marcos_disponibles){
            int inicio_marco = verificar_marcos_disponibles();
            for(int r = 0; r < marcos_necesarios; r++){
                aumentar_tamanio_tabla(tabla, inicio_marco);
            }
        }else{
            log_error(logger_instrucciones , "OUT OF MEMORY for process %d.\n", tabla->pid);
            paqueteDeMensajes(cliente_fd_cpu, "OUT OF MEMORY", OUT_OF_MEMORY);
            return;
        }
        iterar_tabla_de_paginas_e_imprimir(tabla->paginas);
        paqueteDeMensajes(cliente_fd_cpu, "Se aumento la cantidad de paginas correctamente", RESIZE);
    }

}


//PROCESO
pcb *crear_pcb(c_proceso_data data)
{
    pcb *pcb_nuevo = malloc(sizeof(pcb));
    pcb_nuevo->recursos_adquiridos = list_create();


    pcb_nuevo->contexto = malloc(sizeof(cont_exec));
    pcb_nuevo->contexto->PID = data.id_proceso;
    pcb_nuevo->contexto->registros = malloc(sizeof(regCPU));
    pcb_nuevo->contexto->registros->PTBR = malloc(sizeof(TABLA_PAGINA));

    // Implementacion de tabla vacia de paginas
    pcb_nuevo->contexto->registros->PTBR = inicializar_tabla_pagina(data.id_proceso); //puntero a la tabla

    eliminarEspaciosBlanco(data.path);

    instrucciones_a_memoria* new_instrucciones = malloc(sizeof(instrucciones_a_memoria));
    new_instrucciones->pid = data.id_proceso;
    new_instrucciones->instrucciones = list_create();
    enlistar_pseudocodigo(data.path, logger_procesos_creados, new_instrucciones->instrucciones);

    list_add(memoria_de_instrucciones, new_instrucciones);

    return pcb_nuevo;
}

void destruir_tabla(int pid){
    list_destroy_and_destroy_elements(tablas_de_paginas, free);
}

void destruir_pcb(pcb *elemento)
{  
    destruir_memoria_instrucciones(elemento->contexto->PID);
    destruir_tabla_pag_proceso(elemento->contexto->PID); 
    free(elemento->contexto->registros->PTBR);
    free(elemento->contexto->registros);
    elemento->contexto->registros = NULL;
    free(elemento->contexto);
    elemento->contexto = NULL;
    elemento->estadoAnterior = NULL;
    elemento->estadoActual = NULL;
    elemento = NULL;
}

void destruir_instrucciones(void* data){
    inst_pseudocodigo *elemento = (inst_pseudocodigo*)data;
    free(elemento->instruccion);
    elemento->instruccion = NULL;
    elemento = NULL;
}

void destruir_memoria_instrucciones(int pid){
    bool son_inst_pid_aux(void* data){
        return son_inst_pid(pid, data);
    };

    instrucciones_a_memoria* destruir = list_find(memoria_de_instrucciones, son_inst_pid_aux);

    list_destroy_and_destroy_elements(destruir->instrucciones, destruir_instrucciones);

    free(destruir);
    destruir = NULL;
}

bool son_inst_pid(int pid, void* data){
    instrucciones_a_memoria* destruir = (instrucciones_a_memoria*)data;
    return destruir->pid == pid; 
}
// INTERFACES

void *gestionar_llegada_memoria_io (void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    char* registro_direccion;
    char* pid;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case IO_STDIN_READ:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_general);
            registro_direccion = list_get(lista,0);
            char* dato_a_escribir = list_get(lista,1);
            pid = list_get(lista, 2); // ESTO ES PARA LOS LOGS

            // TODO: Validar si esta bien pasado el dato_a_escribir
            escribir_en_memoria(registro_direccion, dato_a_escribir, pid);
            
            break;
        case IO_STDOUT_WRITE:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_general);
            registro_direccion = list_get(lista, 0);
            char* registro_tamanio = list_get(lista, 1);
            pid = list_get(lista,2); // PARA LOGS

            char* dato_a_devolver = leer_en_memoria(registro_direccion, registro_tamanio, pid);

            paquete_memoria_io(cliente_fd_io, dato_a_devolver);
            
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

void escribir_en_memoria(char* direccionFisica, void* data, char* pid) {
    int index_marco = atoi(direccionFisica);
    int bytes_a_copiar = determinar_sizeof(data);

    void* t_dato = malloc(bytes_a_copiar);

    memcpy(t_dato, data, bytes_a_copiar);

    if(!(index_marco < 0 || index_marco > memoria->numero_marcos)) {
        MARCO_MEMORIA *marco = &(memoria->marcos[index_marco]);
        marco->data = t_dato;
        char* tamanio_dato = string_itoa(sizeof(data));
        log_info(logger_general, "PID: %s - Accion: ESCRIBIR - Direccion fisica: %s - Tamaño %s", pid, direccionFisica, tamanio_dato);
        log_info(logger_general, "Se escribio en el marco con indice: %d con el dato %s\n", index_marco, (char*)t_dato);
    } else {
        log_error(logger_general, "Indice de marco fuera de rango: %d\n", index_marco);
    }
}

void* leer_en_memoria(char* direccionFisica, char* registro_tamanio, char* pid) {
    int index_marco = atoi(direccionFisica);
    int tamanio = atoi(registro_tamanio);
    if(!(index_marco < 0 || index_marco > memoria->numero_marcos)) {
        MARCO_MEMORIA *marco = &(memoria->marcos[index_marco]);
        if(marco->data != NULL) {
            void* dato_a_devolver = malloc(tamanio);
            memcpy(dato_a_devolver, &marco->data, tamanio);
            log_info(logger_general, "PID: %s - Accion: LEER - Direccion fisica: %s - Tamaño %s", pid, direccionFisica, registro_tamanio);
            return dato_a_devolver;
        } else {
            log_warning(logger_general, "No hay ningun dato para leer en el marco: %d\n", index_marco);
        }
    } else {
        log_error(logger_general, "Indice de marco fuera de rango: %d\n", index_marco);
    }
    return NULL;
}