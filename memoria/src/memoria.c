#include <memoria.h>
#include <unistd.h>

int cliente_fd_cpu;
int cliente_fd_kernel;
int retardo_respuesta;
int retardo_en_segundos;
int id_de_tablas=0;
int cant_pag=0;
int index_marco = 0;
int tamanio_pagina=0;
int tamanio_memoria=0;

t_log *logger_general;
t_log *logger_instrucciones;
t_log *logger_procesos_creados;
t_log *logger_procesos_finalizados;
t_config *config_memoria;

t_list *tablas_de_paginas;
MEMORIA *memoria;

sem_t paso_instrucciones;

char *path_instructions;

pthread_t hilo[3];

bool enlistar_pseudocodigo(char *path_instructions, char *path, t_log *logger, TABLA_PAGINA *tabla_pagina){
    char instruccion[50] = {0};
    int response;
    
    char *full_path = malloc(strlen(path_instructions) + strlen(path) + 2);
    full_path = strdup(path_instructions);
    strcat(full_path, path);

    FILE *f = fopen(full_path, "rb");

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
        
        response = guardar_en_memoria(memoria, instruccion_a_guardar, tabla_pagina->paginas);
        
        free(instruccion_a_guardar->data);
        instruccion_a_guardar->data = NULL;
    
        if(response == -1){
            break;
        }
    }

    iterar_lista_e_imprimir(tabla_pagina->paginas);


    free(full_path);
    full_path = NULL;
    fclose(f);

    return response;
}


void enviar_instrucciones_a_cpu(char *program_counter, char* pid, int retardo_respuesta, char* index_marco)
{
    // Si la TLB envia el marco directamente entonces devolvemos la instruccion directa de memoria.
    int index_marco_int = atoi(index_marco);
    if (index_marco_int != -1) {
        paqueteDeRespuestaInstruccion(cliente_fd_cpu, memoria->marcos[index_marco_int].data, index_marco);
        return;
    }

    int pc = atoi(program_counter);
    int id_p = atoi(pid);

    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(id_p, data);
    };

    TABLA_PAGINA* tabla = list_find(tablas_de_paginas, es_pid_de_tabla_aux);

    if (list_get(tabla->paginas, pc) != NULL)
    {
        PAGINA* pagina = list_get(tabla->paginas, pc);
        log_info(logger_instrucciones, "Enviaste la instruccion n°%d: %s a CPU exitosamente", pc, (char*)memoria->marcos[pagina->marco].data);
        paqueteDeRespuestaInstruccion(cliente_fd_cpu, memoria->marcos[pagina->marco].data, string_itoa(pagina->marco));
    }else{ 
        paqueteDeRespuestaInstruccion(cliente_fd_cpu, "EXIT", string_itoa(-1));
    }

    sem_post(&paso_instrucciones);
}

bool guardar_en_memoria(MEMORIA* memoria, t_dato* dato_a_guardar, t_list* paginas) {
    int bytes_a_copiar = determinar_sizeof(dato_a_guardar);
    int tamanio_de_pagina = memoria->tam_marcos;
    int cantidad_de_pag_a_usar = (int)ceil((double)bytes_a_copiar/(double)tamanio_de_pagina);
    int index_marco = verificar_marcos_disponibles(cantidad_de_pag_a_usar);

    if(index_marco != -1){
        for (int pagina = 1; pagina <= cantidad_de_pag_a_usar; pagina++) {
            int tamanio_a_copiar = (bytes_a_copiar >= tamanio_de_pagina) ? tamanio_de_pagina : bytes_a_copiar;
            void* t_dato = malloc(tamanio_a_copiar);

            memcpy(t_dato, dato_a_guardar->data, tamanio_a_copiar);

            memoria->marcos[index_marco].data = t_dato;

            dato_a_guardar += tamanio_a_copiar;
            bytes_a_copiar -= tamanio_a_copiar;

            PAGINA* set_pagina = malloc(sizeof(PAGINA));
            set_pagina->bit_validacion = true;
            set_pagina->marco = index_marco;//int tam_lista=(list_size(paginas)*tamanio_de_pagina);

            index_marco++;

            list_add(paginas, set_pagina);

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

int verificar_marcos_disponibles(int cantidad_de_pag_a_usar){
    int contador = 0;

    for(int i = 0; i < memoria->numero_marcos; i++) {
        if(memoria->marcos[i].data == NULL) {
            if(contador == cantidad_de_pag_a_usar) {
                return (i - cantidad_de_pag_a_usar);
            }
            contador++;
        } else {
            contador = 0;
        }
    }

    return -1;
}

void iterar_lista_e_imprimir(t_list *lista)
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
    tamanio_pagina=config_get_int_value(config_memoria,"TAM_PAGINA");
    tamanio_memoria=config_get_int_value(config_memoria,"TAM_MEMORIA");
    path_instructions = config_get_string_value(config_memoria, "PATH_INSTRUCCIONES");
    
    cant_pag = tamanio_memoria/tamanio_pagina;
    retardo_en_segundos = (retardo_respuesta / 1000);

    memoria = malloc(sizeof(MEMORIA));
    inicializar_memoria(memoria, cant_pag, tamanio_pagina);

    tablas_de_paginas = list_create();
    
    /*Banco de pruebas

        PAGINA* tabla = inicializar_tabla_pagina();

        t_dato* dato_a_guardar = malloc(sizeof(t_dato));
        dato_a_guardar->data = (int*)100;
        dato_a_guardar->tipo = 'd';

        enlistar_pseudocodigo(path_instructions, "instrucciones1.txt", logger_general, tabla);
        enlistar_pseudocodigo(path_instructions, "instrucciones2.txt", logger_general, tabla1);
        enlistar_pseudocodigo(path_instructions, "instrucciones3.txt", logger_general, tabla2);
        enlistar_pseudocodigo(path_instructions, "instrucciones4.txt", logger_general, tabla3);


    */

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
            char *pid = list_get(lista, 1);
            char *index_marco = list_get(lista, 2);

            log_info(logger_instrucciones, "Proceso n°%d solicito la instruccion n°%s.\n", atoi(pid), program_counter);
            enviar_instrucciones_a_cpu(program_counter, pid, retardo_respuesta, index_marco);
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
            a_eliminar->path_instrucciones = list_get(lista, 1);
            a_eliminar->estadoActual = list_get(lista, 2);
            a_eliminar->estadoAnterior = list_get(lista, 3);
            a_eliminar->contexto = list_get(lista, 4);
            a_eliminar->contexto->registros = list_get(lista, 5);
            a_eliminar->contexto->registros->PTBR = list_get(lista, 6);
            destruir_pcb(a_eliminar);
            paqueteDeMensajes(cliente_fd_kernel, "Succesful delete. Coming back soon!", FINALIZAR_PROCESO);
            break;
        case SOLICITUD_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_general);
            path = list_get(lista, 0);
            pid = list_get(lista, 1);

            int id_proceso = atoi(pid);
            bool response;

            log_info(logger_procesos_creados, "Se solicito espacio para proceso");

            bool es_pid_de_tabla_aux(void* data){
                return es_pid_de_tabla(id_proceso, data);
            };

            TABLA_PAGINA* tabla_de_proceso = list_find(tablas_de_paginas, es_pid_de_tabla_aux);

            response = enlistar_pseudocodigo(path_instructions, path, logger_instrucciones, tabla_de_proceso);
            
            paqueteDeMensajes(cliente_fd_kernel, string_itoa(response), MEMORIA_ASIGNADA);
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

int size_memoria_restante(){
    return tamanio_memoria-(memoria->numero_marcos*memoria->tam_marcos);
}

unsigned int acceso_a_tabla_de_páginas(int pid,int pagina){
    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(pid, data);
    };
    TABLA_PAGINA* tb = list_find(tablas_de_paginas,es_pid_de_tabla_aux);
    PAGINA* pag = list_get(tb->paginas,pagina);
    return pag->marco;
}
// planteamiento general cantAumentar claramente esta mal, pero es una idea de como seria

void ajustar_tamaño(char* tipoAjuste, int pid, int tamanio){
    TABLA_PAGINA* tb;
    TABLA_PAGINA* aux;
    PAGINA* pagina;
    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(pid, data);
    };
    tb = list_find(tablas_de_paginas,es_pid_de_tabla_aux);

    if(strcmp(tipoAjuste,"aumentar")){
        int tam_aumentar=list_size(tb->paginas);
// cuando sepamos reacomodar las cuestiones esto se modifica.. por ahora me fijo si tengo un lugar 
//para meter la lista entera
        if (size_memoria_restante()>=(tam_aumentar*tamanio_pagina)+tamanio){
            int pag=((tam_aumentar*tamanio_pagina)+tamanio)/memoria->tam_marcos;
            log_info(logger_general,"AUMENTO VALIDO PARA EL PROCESO %d ",pid);
            int inicio_marco=verificar_marcos_disponibles(pag);
            for(int i=0;i<=pag;i++){
                if(list_get(tb->paginas,i)!=NULL){
                pagina=list_get(tb->paginas,i);
//NO DEBERIA DE PODER HACER ESTO XQ AL CAMBIAR EL NRO DE MARCO NO CAMBIAMOS LA DIRECC DE MEMORIA, OSEA NO TIENE SENTIDO XD
                pagina->marco=inicio_marco+i;
                }else{
                pagina->marco=NULL;
                pagina->bit_validacion=false;
                }
            }
            destruir_tabla_pag_proceso(pid); 
            //lo uso recien aca, para que no me borre el aux tmb la funcion de arriba
            aux->pid=pid;
            list_add(tb->paginas,aux);
        }else{
            log_error(logger_general,"OUT OF MEMORY");
        }
    }else if(strcmp(tipoAjuste,"disminuir")){

    }
}

//PROCESO
pcb *crear_pcb(c_proceso_data data)
{
    pcb *pcb_nuevo = malloc(sizeof(pcb));
    pcb_nuevo->recursos_adquiridos=list_create();

    eliminarEspaciosBlanco(data.path);
    pcb_nuevo->path_instrucciones = strdup(data.path);

    pcb_nuevo->contexto = malloc(sizeof(cont_exec));
    pcb_nuevo->contexto->PID = data.id_proceso;
    pcb_nuevo->contexto->registros = malloc(sizeof(regCPU));
    pcb_nuevo->contexto->registros->PTBR = malloc(sizeof(TABLA_PAGINA));

    // Implementacion de tabla vacia de paginas
    pcb_nuevo->contexto->registros->PTBR = inicializar_tabla_pagina(data.id_proceso);//puntero a la tb
    return pcb_nuevo;
}

void destruir_tabla(int pid){
    list_destroy_and_destroy_elements(tablas_de_paginas, free);
}

void destruir_pcb(pcb *elemento)
{  
    destruir_tabla_pag_proceso(elemento->contexto->PID); 
    free(elemento->contexto->registros->PTBR);
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

void escribir_en_memoria(char* direccionFisica, void* data) {
    int index_marco = atoi(direccionFisica);
    int bytes_a_copiar = determinar_sizeof(data);

    void* t_dato = malloc(bytes_a_copiar);

    memcpy(t_dato, data, bytes_a_copiar);

    if(!(index_marco < 0 || index_marco > memoria->numero_marcos)) {
        MARCO_MEMORIA *marco = &(memoria->marcos[index_marco]);
        marco->data = t_dato;
        log_info(logger_general, "Se escribio en el marco con indice: %d con el dato %s\n", index_marco, (char*)t_dato);
    } else {
        log_error(logger_general, "Indice de marco fuera de rango: %d\n", index_marco);
    }
}

void* leer_en_memoria(char* direccionFisica) {
    int index_marco = atoi(direccionFisica);

    if(!(index_marco < 0 || index_marco > memoria->numero_marcos)) {
        MARCO_MEMORIA *marco = &(memoria->marcos[index_marco]);
        if(marco->data != NULL) {
            return marco->data;
        } else {
            log_warning(logger_general, "No hay ningun dato para leer en el marco: %d\n", index_marco);
        }
    } else {
        log_error(logger_general, "Indice de marco fuera de rango: %d\n", index_marco);
    }

}