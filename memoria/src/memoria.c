#include <memoria.h>
#include <unistd.h>

int cliente_fd_cpu;
int cliente_fd_kernel;
int cliente_fd_io;
int retardo_respuesta;
int grado_multiprogramacion;
char* bitmap; 

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

void guardar_en_memoria(MEMORIA* memoria, t_dato* dato_a_guardar, TABLA_PAGINA* tabla) {
    int bytes_a_copiar = determinar_sizeof(dato_a_guardar);
    int tamanio_de_pagina = memoria->tam_marcos;
    
    void* copia_dato_a_guardar = malloc(bytes_a_copiar);
    memcpy(copia_dato_a_guardar, dato_a_guardar->data, bytes_a_copiar);

    int bytes_copiados = 0;
    //Itero para guardar dicho dato en los marcos asignados
    while(bytes_copiados != bytes_a_copiar){
        int tamanio_a_copiar;
        int bytes_restantes = (bytes_a_copiar - bytes_copiados);
        void* dato_a_memoria;

        //Busco una pagina vacia de la tabla y la modifico para poder guardar ese dato consecutivamente 
        PAGINA* set_pagina = list_get(tabla->paginas, ultima_pagina_usada(tabla));

        if(set_pagina != NULL){
            //Guardo en el tamaño lo que me falta para llenar la pagina
            if(pagina_vacia(set_pagina)){
                tamanio_a_copiar = (bytes_restantes >= tamanio_de_pagina) ? tamanio_de_pagina : bytes_restantes;
            }else{
                int tamanio_restante = tamanio_de_pagina - memoria->marcos[set_pagina->marco].tamanio;
                tamanio_a_copiar = (bytes_restantes >= tamanio_restante) ? tamanio_restante : bytes_restantes;
            }
            dato_a_memoria = malloc(tamanio_a_copiar);

            //Copio la memoria necesaria desde el punto en donde me quede
            memcpy(dato_a_memoria, &copia_dato_a_guardar[bytes_copiados], tamanio_a_copiar);
            
            //Completo el marco de memoria con lo que resta de memoria
            memoria->marcos[set_pagina->marco].data = dato_a_memoria;
            memoria->marcos[set_pagina->marco].tamanio = tamanio_a_copiar;
            
            bytes_copiados += tamanio_a_copiar;
            printf("Posicion de marco: %d Direccion de dato en marco: %p\n", set_pagina->marco, &memoria->marcos[set_pagina->marco].data);
        }else{
            //Si me quedo sin paginas y existen mas marcos disponibles pido mas memoria
            int paginas_restantes = (int)ceil((double)bytes_a_copiar/(double)tamanio_de_pagina);
            bool response = reservar_memoria(tabla, paginas_restantes);
            if(!response){
                //En el caso de no tener memoria disponible devuelvo el proceso a EXIT
                log_error(logger_instrucciones , "OUT OF MEMORY for process %d.\n", tabla->pid);
                paqueteDeMensajes(cliente_fd_cpu, "OUT OF MEMORY", OUT_OF_MEMORY);
                break;
            }else{
                PAGINA* set_pagina = list_get(tabla->paginas, ultima_pagina_usada(tabla) + 1);
                
                tamanio_a_copiar = (bytes_restantes >= tamanio_de_pagina) ? tamanio_de_pagina : bytes_restantes;
                dato_a_memoria = malloc(tamanio_a_copiar);
                
                //Copio la memoria necesaria desde el punto en donde me quede
                memcpy(dato_a_memoria, &copia_dato_a_guardar[bytes_copiados], tamanio_a_copiar);

                memoria->marcos[set_pagina->marco].data = dato_a_memoria;
                memoria->marcos[set_pagina->marco].tamanio = tamanio_a_copiar;
                bytes_copiados += tamanio_a_copiar;
                printf("Posicion de marco: %d Direccion de dato en marco: %p\n", set_pagina->marco, &memoria->marcos[set_pagina->marco].data);
            }
        }     
    }     
    free(copia_dato_a_guardar);
    copia_dato_a_guardar = NULL;
}

void inicializar_memoria(MEMORIA* memoria, int num_marcos, int tam_marcos) {
    memoria->marcos = malloc(num_marcos * tam_marcos);

    for(int i = 0; i < num_marcos; i++) {
        memoria->marcos[i].data = malloc(tam_marcos);
        memoria->marcos[i].tamanio = 0;
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
        case 'e':
            return sizeof(int);
        case 'd':
            return sizeof(double);
        case 'l':
            return sizeof(long);
    }
    return 0;
}

// -------------------------- Bit map --------------------------

char* crear_bitmap() {
    int tamanio = (memoria->numero_marcos + 8 - 1) / 8;
    char *bitmap = (char*)calloc(tamanio, sizeof(char));
    if (bitmap == NULL) {
        fprintf(stderr, "Error al asignar memoria para el bitmap\n");
        exit(1);
    }
    return bitmap;
}

void establecer_bit(int indice, bool valor) {
    int byteIndex = indice / 8;
    int bitIndex = indice % 8;
    if (valor) {
        bitmap[byteIndex] |= (1 << bitIndex); 
    } else {
        bitmap[byteIndex] &= ~(1 << bitIndex);
    }
}

bool obtener_bit(int indice) {
    int byteIndex = indice / 8;
    int bitIndex = indice % 8;
    return (bitmap[byteIndex] & (1 << bitIndex)) != 0;
}

void imprimir_bitmap() {
    for (int i = 0; i < memoria->numero_marcos; i++) {
        printf("El bit %d está %s\n", i, obtener_bit(i) ? "ocupado" : "libre");
    }
}

void liberar_bitmap() {
    free(bitmap);
    bitmap = NULL;
}

int buscar_marco_libre() {
    for (int i = 0; i < memoria->numero_marcos; i++) {
        if (!obtener_bit(i)) {
            return i;
        }
    }
    return -1;
}

// --------------------------------------------------------------

bool verificar_marcos_disponibles(int cantidad_de_pag_solicitadas){
    int contador = 0;

    for(int i = 0; i < memoria->numero_marcos; i++) {
        if(obtener_bit(i) == false) {
            if(contador == cantidad_de_pag_solicitadas) {
                return true;
            }
            contador++;
        }
    }
    return false;
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

    bitmap = crear_bitmap();

    tablas_de_paginas = list_create();

    memoria_de_instrucciones = list_create();
    
    //Banco de pruebas
        TABLA_PAGINA* tabla = inicializar_tabla_pagina(1);
        reservar_memoria(tabla, 10);

        imprimir_bitmap();

        t_dato* dato_a_guardar = malloc(sizeof(t_dato));
        dato_a_guardar->data = "Hoy me siento re zarpado nieri eh cuidado conmigo";
        dato_a_guardar->tipo = 's';

        t_dato* dato_a_guardar2 = malloc(sizeof(t_dato));
        dato_a_guardar2->data = (int*)5;
        dato_a_guardar2->tipo = 'l';

        guardar_en_memoria(memoria, dato_a_guardar, tabla);
        guardar_en_memoria(memoria, dato_a_guardar2, tabla);

        char* dato_0 = memoria->marcos[acceso_a_tabla_de_páginas(1, 0)].data;
        printf("%s\n", dato_0);
        
        char* dato_1 = memoria->marcos[acceso_a_tabla_de_páginas(1, 1)].data;
        char* dato_guardado = strcat(dato_0, dato_1);
        
        printf("%s\n", dato_1);
        printf("%s\n", dato_guardado);

    

    int server_memoria = iniciar_servidor(logger_general, puerto_escucha);
    log_info(logger_general, "Servidor a la espera de clientes");

    cliente_fd_cpu = esperar_cliente(server_memoria, logger_general);
    cliente_fd_kernel = esperar_cliente(server_memoria, logger_general);
    // int cliente_fd_tres = esperar_cliente(server_memoria, logger_memoria);
    cliente_fd_io = esperar_cliente(server_memoria, logger_general);

    paqueteDeMensajes(cliente_fd_cpu, string_itoa(tamanio_pagina), MENSAJE);
    paqueteDeMensajes(cliente_fd_kernel, string_itoa(retardo_respuesta), TIEMPO_RESPUESTA);

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

    liberar_bitmap(bitmap);

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

            log_info(logger_procesos_creados, "-Se solicito espacio para proceso %d-\n", id_proceso);

            bool es_pid_de_tabla_aux(void* data){
                return es_pid_de_tabla(id_proceso, data);
            };

            TABLA_PAGINA* tabla_de_proceso = list_find(tablas_de_paginas, es_pid_de_tabla_aux);

            int cant_pag_por_proceso = (int)floor((double)(memoria->numero_marcos/grado_multiprogramacion));

            response = reservar_memoria(tabla_de_proceso, cant_pag_por_proceso);
            
            if(response){
                log_info(logger_procesos_creados, "-Se asigno espacio en memoria para proceso %d-\n", id_proceso);
                paqueteDeMensajes(cliente_fd_kernel, string_itoa(1), MEMORIA_ASIGNADA);
            }else{
                log_info(logger_procesos_creados, "-Se denego el espacio en memoria para proceso %d-\n", id_proceso);
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

t_list* crear_tabla_de_paginas(){
    t_list* lista_paginas = list_create();

    for(int i = 0; i < memoria->numero_marcos; i++){
        PAGINA* nueva_pagina = malloc(sizeof(PAGINA));
        nueva_pagina->marco = -1;
        nueva_pagina->bit_validacion = false;
        list_add(lista_paginas, nueva_pagina);
    }

    return lista_paginas;
}


TABLA_PAGINA* inicializar_tabla_pagina(int pid) {
    TABLA_PAGINA* tabla_pagina = malloc(sizeof(TABLA_PAGINA));
    tabla_pagina->pid = pid;
    tabla_pagina->paginas = crear_tabla_de_paginas();

    list_add(tablas_de_paginas, tabla_pagina);

    //TODO: Ver logica de bit a nivel de la estructura de la tabla de paginas

    return tabla_pagina;
}

bool reservar_memoria(TABLA_PAGINA* tabla_de_proceso, int cantidad){    
    if(verificar_marcos_disponibles(cantidad)){
        for(int i = 0; i < cantidad; i++){
            int index_marco = buscar_marco_libre();
            asignar_marco_a_pagina(tabla_de_proceso, index_marco);
        }
        return true;
    }
    return false;
}

void asignar_marco_a_pagina(TABLA_PAGINA* tabla, int marco_disponible){
    PAGINA* pagina = list_find(tabla->paginas, pagina_sin_frame);

    memset(memoria->marcos[marco_disponible].data, 0, memoria->tam_marcos);

    pagina->marco = marco_disponible;
    pagina->bit_validacion = true;

    establecer_bit(marco_disponible, true);
}

bool pagina_sin_frame(void* data){
    PAGINA* pagina = (PAGINA*)data;
    return pagina->marco == -1;
}

bool pagina_vacia(PAGINA* pagina){
    return memoria->marcos[pagina->marco].tamanio == 0;
}

int cantidad_de_paginas_usadas(TABLA_PAGINA* tabla){
    int contador = 0;

    t_list_iterator* lista_paginas = list_iterator_create(tabla->paginas);
    
    while(list_iterator_has_next(lista_paginas)){
        PAGINA* pagina = list_iterator_next(lista_paginas);
        if(pagina->marco == -1){
            list_iterator_destroy(lista_paginas);
            return contador;
        }
        contador++;
    }
    list_iterator_destroy(lista_paginas);
    return contador;
}

int ultima_pagina_usada(TABLA_PAGINA* tabla){
    int contador = 0;

    t_list_iterator* lista_paginas = list_iterator_create(tabla->paginas);
    
    while(list_iterator_has_next(lista_paginas)){
        PAGINA* pagina = list_iterator_next(lista_paginas);
        if(memoria->marcos[pagina->marco].tamanio < memoria->tam_marcos){
            list_iterator_destroy(lista_paginas);
            return contador;
        }
        contador++;
    }

    list_iterator_destroy(lista_paginas);
    return contador;
}

void destruir_tabla_pag_proceso(int pid){
    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(pid, data);
    };

    TABLA_PAGINA* destruir = list_find(tablas_de_paginas, es_pid_de_tabla_aux);

    for(int i = 0; i < list_size(destruir->paginas); i++){
        PAGINA* pagina = list_get(destruir->paginas, i);
        if(obtener_bit(pagina->marco) == true){
            establecer_bit(pagina->marco, false);
        }else{
            break;
        }
    }

    list_destroy_and_destroy_elements(destruir->paginas, free);
    destruir->paginas=NULL;
    free(destruir);
    destruir=NULL;
}

unsigned int acceso_a_tabla_de_páginas(int pid, int pagina){
    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(pid, data);
    };
    TABLA_PAGINA* tabla = list_find(tablas_de_paginas,es_pid_de_tabla_aux);
    PAGINA* pag = list_get(tabla->paginas, pagina);
    log_info(logger_general, "PID: %d - Pagina: %d - Marco: %d\n", tabla->pid, pagina, pag->marco);
    return pag->marco;
}

// planteamiento general cantAumentar claramente esta mal, pero es una idea de como seria

void ajustar_tamaño(TABLA_PAGINA* tabla, char* tamanio){
    int tamanio_solicitado = atoi(tamanio);
    int cantidad_de_pag_solicitadas = (int)ceil((double)tamanio_solicitado/(double)(memoria->tam_marcos));

    int paginas_usadas = cantidad_de_paginas_usadas(tabla);

    if(paginas_usadas > cantidad_de_pag_solicitadas){
        log_info(logger_instrucciones, "PID: %d - Tamanio actual: %d - Tamanio a reducir: %d\n", tabla->pid, paginas_usadas, cantidad_de_pag_solicitadas);

        for(int j = (paginas_usadas - 1); j > (cantidad_de_pag_solicitadas - 1); j--){
            PAGINA* pagina_a_borrar = list_get(tabla->paginas, j);    
        
            establecer_bit(pagina_a_borrar->marco, false);
            memset(memoria->marcos[pagina_a_borrar->marco].data, 0, memoria->tam_marcos);
            memoria->marcos[pagina_a_borrar->marco].tamanio = 0;
            
            list_remove_and_destroy_element(tabla->paginas, j, free);
        }   
        iterar_tabla_de_paginas_e_imprimir(tabla->paginas);
        paqueteDeMensajes(cliente_fd_cpu, "Se disminuyo la cantidad de paginas correctamente", RESIZE);

    }else{
        log_info(logger_instrucciones, "PID: %d - Tamanio actual: %d - Tamanio a ampliar: %d\n", tabla->pid, paginas_usadas, cantidad_de_pag_solicitadas);
        int marcos_necesarios = cantidad_de_pag_solicitadas - paginas_usadas;

        if(verificar_marcos_disponibles(marcos_necesarios)){
            int inicio_marco = buscar_marco_libre();
            for(int r = 0; r < marcos_necesarios; r++){
                asignar_marco_a_pagina(tabla, inicio_marco);
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