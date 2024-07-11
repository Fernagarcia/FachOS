#include <memoria.h>
#include <unistd.h>

int server_memoria;
int cliente_fd_cpu;
int cliente_fd_kernel;
int cliente_fd_io;
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

t_list *interfaces;

t_list *tablas_de_paginas;
MEMORIA *memoria;

sem_t paso_instrucciones;

char *path_instructions;

pthread_t hilo[5];

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
        instruccion_a_guardar = NULL;
    
        if(response == -1){
            break;
        }
    }

    iterar_lista_e_imprimir(tabla_pagina->paginas);

    fclose(f);

    free(full_path);
    full_path = NULL;
    
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

    interfaces = list_create();

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

    server_memoria = iniciar_servidor(logger_general, puerto_escucha);
    log_info(logger_general, "Servidor a la espera de clientes");

    cliente_fd_cpu = esperar_cliente(server_memoria, logger_general);
    cliente_fd_kernel = esperar_cliente(server_memoria, logger_general);
    // int cliente_fd_tres = esperar_cliente(server_memoria, logger_memoria);
    cliente_fd_io = esperar_cliente(server_memoria, logger_general);

    paqueteDeMensajes(cliente_fd_cpu, string_itoa(tamanio_pagina), MENSAJE);

    ArgsGestionarServidor args_sv1 = {logger_instrucciones, cliente_fd_cpu};
    ArgsGestionarServidor args_sv2 = {logger_procesos_creados, cliente_fd_kernel};
    ArgsGestionarServidor args_sv3 = {logger_procesos_creados, cliente_fd_io};
    // ArgsGestionarServidor args_sv3 = {logger_memoria, cliente_fd_tres};

    pthread_create(&hilo[0], NULL, gestionar_llegada_memoria_cpu, &args_sv1);
    pthread_create(&hilo[1], NULL, gestionar_llegada_memoria_kernel, &args_sv2);
    pthread_create(&hilo[3],NULL, gestionar_llegada_memoria_io, &args_sv3 );
    pthread_create(&hilo[4],NULL, esperar_nuevo_io, NULL );

    // pthread_create(&hilo[2], NULL, gestionar_llegada, &args_sv3);

    for (int i = 0; i < 5; i++)
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
            sleep(retardo_en_segundos);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
            char *program_counter = list_get(lista, 0);
            char *pid = list_get(lista, 1);
            index_marco = list_get(lista, 2);

            log_info(logger_instrucciones, "Proceso n°%d solicito la instruccion n°%s.\n", atoi(pid), program_counter);
            enviar_instrucciones_a_cpu(program_counter, pid, retardo_respuesta, index_marco);
            break;
        case LEER_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
            index_marco = list_get(lista, 0);
            pid = list_get(lista, 1);

            void* response;

            response = leer_en_memoria(index_marco, sizeof(uint32_t), pid);

            paqueteDeRespuestaInstruccion(cliente_fd_cpu, response, index_marco);
            break;
        case ESCRIBIR_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
            index_marco = list_get(lista, 0);
            pid = list_get(lista, 1);
            void* dato_a_escribir = list_get(lista, 2);

            if(sizeof(dato_a_escribir) == 8) {
                uint8_t *dato_a_escribir_8 = (uint8_t)dato_a_escribir;
                escribir_en_memoria(index_marco, dato_a_escribir_8, pid);
            } else {
                uint32_t *dato_a_escribir_32 = (uint32_t)dato_a_escribir;
                escribir_en_memoria(index_marco, dato_a_escribir_32, pid);
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
PAGINA* modificar_marco_memoria(PAGINA* pagina,int inicio_marco){
    PAGINA* aux;
        if(list_get(pagina,0)!=NULL){
                aux=list_get(pagina,0);
                memoria->marcos[inicio_marco].data = memoria->marcos[pagina->marco].data;
                memoria->marcos[pagina->marco].data=NULL;
                aux->marco=inicio_marco;                
        }else{
                aux->marco=inicio_marco;
                aux->bit_validacion=false;
            }
            return aux;
}
// planteamiento general cantAumentar claramente esta mal, pero es una idea de como seria

void ajustar_tamaño(int pid, char* tamanio){
    TABLA_PAGINA* tb;
    PAGINA* pagina;
    int tamanio_cpu=atoi(tamanio);
    int pag=tamanio_cpu/memoria->tam_marcos;
    int inicio_marco;
    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(pid, data);
    };
    tb = list_find(tablas_de_paginas,es_pid_de_tabla_aux);

        int tam_lista=list_size(tb->paginas);
        if(tam_lista>tamanio_cpu){
            printf("LA LISTA %d SE DEBE DISMINUIR", tb->pid);
        }else{
            printf("LA LISTA %d SE DEBE AUMENTAR", tb->pid);
            if (size_memoria_restante()>=tamanio_cpu){
                log_info(logger_general,"AUMENTO VALIDO PARA EL PROCESO %d ",pid);
                printf("se debera aumentar %d paginas",pag);
                inicio_marco=verificar_marcos_disponibles(pag);
            }else{
            log_error(logger_general,"OUT OF MEMORY");
        }
        }
            PAGINA* pagina_nueva;
            for(int i=0;i<pag;i++){
                pagina=list_get(tb->paginas,i);
                pagina_nueva=modificar_marco_memoria(pagina,(inicio_marco+i));
                list_add_in_index(tb->paginas,i,pagina_nueva);
            }
            destruir_tabla_pag_proceso(pid); 
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

// INTERFACES

void *gestionar_llegada_memoria_io (void *args){

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

void *esperar_nuevo_io(){

    while(1){

        INTERFAZ* interfaz_a_agregar;
        t_list *lista;

        int socket_io = esperar_cliente(server_memoria,logger_general);

        int cod_op = recibir_operacion(socket_io);

        if(cod_op != NUEVA_IO) {return;}

        lista = recibir_paquete(socket_io,logger_general);

        interfaz_a_agregar = asignar_espacio_a_io(lista);
        interfaz_a_agregar->socket_kernel = socket_io;
        list_add(interfaces,interfaz_a_agregar);

        log_info(logger_general,"\nSe ha conectado la interfaz %s\n",interfaz_a_agregar->datos->nombre);

        interfaces_conectadas();
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

// puede ir al utils?
INTERFAZ* asignar_espacio_a_io(t_list* lista){
    INTERFAZ* nueva_interfaz = malloc(sizeof(INTERFAZ));
    nueva_interfaz = list_get(lista, 0);
    nueva_interfaz->datos = malloc(sizeof(DATOS_INTERFAZ));
    nueva_interfaz->datos = list_get(lista, 1);
    nueva_interfaz->datos->nombre = list_get(lista, 2);
    nueva_interfaz->datos->operaciones = list_get(lista, 3);
    
    nueva_interfaz->procesos_bloqueados = queue_create();
    
    int j = 0;
    for (int i = 4; i < list_size(lista); i++){
        nueva_interfaz->datos->operaciones[j] = strdup((char*)list_get(lista, i));
        j++;
    }

    nueva_interfaz->estado = LIBRE;
    return nueva_interfaz;
}

// puede ir al utils?
void iterar_lista_interfaces_e_imprimir(t_list *lista){
    INTERFAZ *interfaz;
    t_list_iterator *lista_a_iterar = list_iterator_create(lista);
    if (lista_a_iterar != NULL)
    { // Verificar que el iterador se haya creado correctamente
        printf(" [ ");
        while (list_iterator_has_next(lista_a_iterar))
        {
            interfaz = list_iterator_next(lista_a_iterar); // Convertir el puntero genérico a pcb*

            if (list_iterator_has_next(lista_a_iterar))
            {
                printf("%s - ", interfaz->datos->nombre);
            }
            else
            {
                printf("%s", interfaz->datos->nombre);
            }
        }
        printf(" ]\tInterfaces conectadas: %d\n", list_size(lista));
    }
    list_iterator_destroy(lista_a_iterar);
}

// puede ir al utils?
int interfaces_conectadas(){
    printf("CONNECTED IOs.\n");
    iterar_lista_interfaces_e_imprimir(interfaces);
    return 0;
}