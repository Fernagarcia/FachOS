#include <memoria.h>
#include <unistd.h>

int server_memoria;
int cliente_fd_cpu;
int cliente_fd_kernel;
int cliente_fd_io;
int retardo_respuesta;
char* bitmap; 

t_log *logger_general;
t_log *logger_interfaces;
t_log *logger_instrucciones;
t_log *logger_procesos_creados;
t_log *logger_procesos_finalizados;
t_config *config_memoria;

t_list *memoria_de_instrucciones;
t_list *tablas_de_paginas;
t_list *interfaces_conectadas;
MEMORIA *memoria;

int bits_para_marco;
int bits_para_offset;

sem_t paso_instrucciones;

pthread_mutex_t mutex_interfaz = PTHREAD_MUTEX_INITIALIZER;
pthread_t hilo[2];
pthread_mutex_t mutex_guardar_memoria = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]){

    sem_init(&paso_instrucciones, 1, 1);

    logger_general = iniciar_logger("mgeneral.log", "memoria_general.log", LOG_LEVEL_INFO);
    logger_interfaces = iniciar_logger("IOMemoria.log", "interfaces-memoria.log", LOG_LEVEL_INFO);
    logger_instrucciones = iniciar_logger("instructions.log", "instructions.log", LOG_LEVEL_INFO);
    logger_procesos_finalizados = iniciar_logger("fprocess.log", "finalize_process.log", LOG_LEVEL_INFO);
    logger_procesos_creados = iniciar_logger("cprocess.log", "create_process.log", LOG_LEVEL_INFO);

    config_memoria = iniciar_configuracion();
    char* puerto_escucha = config_get_string_value(config_memoria, "PUERTO_ESCUCHA");
    retardo_respuesta = config_get_int_value(config_memoria, "RETARDO_RESPUESTA"); 
    int tamanio_pagina=config_get_int_value(config_memoria,"TAM_PAGINA");
    int tamanio_memoria=config_get_int_value(config_memoria,"TAM_MEMORIA");
    
    int cant_pag = tamanio_memoria/tamanio_pagina;
   
    memoria = malloc(sizeof(MEMORIA));

    inicializar_memoria(memoria, cant_pag, tamanio_pagina);

    bits_para_marco = (int)log2((int)memoria->numero_marcos);
    bits_para_offset = (int)log2((int)tamanio_pagina);

    bitmap = crear_bitmap();

    interfaces_conectadas = list_create();
    tablas_de_paginas = list_create();
    memoria_de_instrucciones = list_create();

    server_memoria = iniciar_servidor(logger_general, puerto_escucha);
    log_info(logger_general, "Servidor a la espera de clientes");

    cliente_fd_cpu = esperar_cliente(server_memoria, logger_general);
    log_info(logger_general, "SE CONECTO CPU");

    cliente_fd_kernel = esperar_cliente(server_memoria, logger_general);
    log_info(logger_general, "SE CONECTO KERNEL");

    paqueteDeMensajes(cliente_fd_cpu, string_itoa(tamanio_pagina), MENSAJE);
    paqueteDeMensajes(cliente_fd_kernel, string_itoa(retardo_respuesta), TIEMPO_RESPUESTA);

    ArgsGestionarServidor args_sv1 = {logger_instrucciones, cliente_fd_cpu};
    ArgsGestionarServidor args_sv2 = {logger_procesos_creados, cliente_fd_kernel};

    pthread_create(&hilo[0], NULL, gestionar_llegada_memoria_cpu, &args_sv1);
    pthread_create(&hilo[1], NULL, gestionar_llegada_memoria_kernel, &args_sv2);
    //pthread_create(&hilo[2], NULL, , NULL);

    esperar_nuevo_io();

    for (int i = 0; i <= 2; i++)
    {
        pthread_join(hilo[i], NULL);
    }

    sem_destroy(&paso_instrucciones);

    liberar_bitmap(bitmap);

    terminar_programa(logger_general, config_memoria);
    log_destroy(logger_instrucciones);
    log_destroy(logger_interfaces);
    log_destroy(logger_procesos_creados);
    log_destroy(logger_procesos_finalizados);

    return 0;
}

void enlistar_pseudocodigo(char *path, t_log *logger, t_list *pseudocodigo){
    char instruccion[100] = {0};
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

void enviar_instrucciones_a_cpu(char *program_counter, char* pid){
    int pc = atoi(program_counter);
    int id_p = atoi(pid);

    bool son_inst_pid_aux(void* data){
        return son_inst_pid(id_p, data);
    };

    instrucciones_a_memoria* inst_proceso = list_find(memoria_de_instrucciones, son_inst_pid_aux);

    if (list_get(inst_proceso->instrucciones, pc) != NULL)
    {
        inst_pseudocodigo* instruccion = list_get(inst_proceso->instrucciones, pc);
        log_debug(logger_instrucciones, "Enviaste la instruccion n°%d: %s a CPU exitosamente", pc, instruccion->instruccion);
        paqueteDeMensajes(cliente_fd_cpu, instruccion->instruccion, RESPUESTA_MEMORIA);
    }else{ 
        paqueteDeMensajes(cliente_fd_cpu, "EXIT", RESPUESTA_MEMORIA);
    }

    sem_post(&paso_instrucciones);
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

// -------------------------- Bit map -------------------------- //

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
// -------------------------------------------------------------- //

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

void *gestionar_llegada_memoria_cpu(void *args){

    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;

    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        char *direccion_fisica;
        char* pid;
        char* tamanio;
        bool escritura;
        switch (cod_op)
        {
            case MENSAJE:
                recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
                break;

            case INSTRUCCION:
                sem_wait(&paso_instrucciones);
                usleep(retardo_respuesta * 1000);
                lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
                char *program_counter = list_get(lista, 0);
                pid = list_get(lista, 1);
                log_info(logger_instrucciones, "Proceso n°%d solicito la instruccion n°%s.\n", atoi(pid), program_counter);
                enviar_instrucciones_a_cpu(program_counter, pid);
                list_destroy(lista);
                break;

            case LEER_MEMORIA:
                lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
                direccion_fisica = list_get(lista, 0);
                tamanio = list_get(lista, 1);
                pid = list_get(lista, 2);

                t_dato* dato_a_mandar = malloc(sizeof(t_dato));

                void* lectura = malloc(atoi(tamanio));
                lectura = leer_en_memoria(direccion_fisica, atoi(tamanio), pid);

                dato_a_mandar->data = lectura;
                dato_a_mandar->tamanio = atoi(tamanio);

                paqueT_dato(cliente_fd_cpu, dato_a_mandar);

                list_destroy(lista);
                free(lectura);
                lectura = NULL;
                free(dato_a_mandar);
                dato_a_mandar = NULL;
                break;

            case ESCRIBIR_MEMORIA:
                pthread_mutex_lock(&mutex_guardar_memoria);
                lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
                PAQUETE_ESCRITURA* paquete_recibido = malloc(sizeof(PAQUETE_ESCRITURA));
                paquete_recibido= list_get(lista, 0);
                paquete_recibido->direccion_fisica = list_get(lista, 1);
                paquete_recibido->dato = list_get(lista, 2);
                paquete_recibido->dato->data = list_get(lista, 3);

                escritura = escribir_en_memoria(paquete_recibido->direccion_fisica, paquete_recibido->dato, string_itoa(paquete_recibido->pid));
                
                if(escritura){
                    paqueteDeMensajes(args_entrada->cliente_fd, "OK", RESPUESTA_ESCRIBIR_MEMORIA);
                }
                
                list_destroy(lista);
                free(paquete_recibido->dato);
                paquete_recibido->dato = NULL;
                free(paquete_recibido);
                paquete_recibido = NULL;
                pthread_mutex_unlock(&mutex_guardar_memoria);
                break;

            case ACCEDER_MARCO:
                lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
                PAQUETE_MARCO* acceso = list_get(lista, 0);
                int index_marco = acceso_a_tabla_de_páginas(acceso->pid, acceso->pagina);
                paqueteDeMensajes(cliente_fd_cpu, string_itoa(index_marco), ACCEDER_MARCO);
                list_destroy(lista);
                break;

            case RESIZE:
                lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
                t_resize* info_rsz = list_get(lista, 0);
                info_rsz->tamanio = list_get(lista, 1);

                bool es_pid_de_tabla_aux(void* data){
                    return es_pid_de_tabla(info_rsz->pid, data);
                };

                TABLA_PAGINA* tabla = list_find(tablas_de_paginas, es_pid_de_tabla_aux);

                ajustar_tamanio(tabla, info_rsz->tamanio);

                list_destroy(lista);
                break;

            case COPY_STRING:
                pthread_mutex_lock(&mutex_guardar_memoria);
                lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
                char* direccion_fisica_origen = list_get(lista, 0);
                char* direccion_fisica_destino = list_get(lista, 1);
                tamanio = list_get(lista, 2);
                char* pid = list_get(lista, 3);

                void* response = malloc(atoi(tamanio));

                response = leer_en_memoria(direccion_fisica_origen, atoi(tamanio), pid);
                
                t_dato* dato_a_escribir = malloc(sizeof(t_dato));
                dato_a_escribir->data = response;
                dato_a_escribir->tamanio = atoi(tamanio); 

                escritura = escribir_en_memoria(direccion_fisica_destino, dato_a_escribir, pid);
                
                if(escritura){   
                    paqueteDeMensajes(args_entrada->cliente_fd, "OK", RESPUESTA_ESCRIBIR_MEMORIA);
                }
                
                list_destroy(lista);
                free(response);
                response = NULL;
                free(dato_a_escribir);
                dato_a_escribir = NULL;
                pthread_mutex_unlock(&mutex_guardar_memoria);
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

void *gestionar_llegada_memoria_kernel(void *args){

    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    char* pid;

    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {

        case MENSAJE:
            recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
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
            destruir_pcb(a_eliminar);
            paqueteDeMensajes(cliente_fd_kernel, "Succesful delete. Coming back soon!", FINALIZAR_PROCESO);
            break;

        case SOLICITUD_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_general);
            pid = list_get(lista, 0);
            int id_proceso = atoi(pid);
            bool response;

            log_info(logger_procesos_creados, "-Se solicito espacio para albergar el proceso n°%d-", id_proceso);

            response = verificar_marcos_disponibles(1);
            
            if(response){
                log_debug(logger_procesos_creados, "-Se asigno espacio en memoria para proceso %d-\n", id_proceso);
                paqueteDeMensajes(cliente_fd_kernel, string_itoa(1), MEMORIA_ASIGNADA);
            }else{
                log_debug(logger_procesos_creados, "-Se denego el espacio en memoria para proceso %d-\n", id_proceso);
                paqueteDeMensajes(cliente_fd_kernel, string_itoa(-1), MEMORIA_ASIGNADA);
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

bool es_pid_de_tabla(int pid, void* data){
    TABLA_PAGINA* tabla = (TABLA_PAGINA*)data;
    return tabla->pid == pid;
}

t_list* crear_tabla_de_paginas(){
    t_list* lista_paginas = list_create();

    for(int i = 0; i < memoria->numero_marcos; i++){
        PAGINA* nueva_pagina = malloc(sizeof(PAGINA));
        nueva_pagina->nro_pagina = i;
        nueva_pagina->marco = -1;
        nueva_pagina->bit_validacion = false;
        list_add(lista_paginas, nueva_pagina);
    }

    return lista_paginas;
}

void inicializar_tabla_pagina(int pid) {
    TABLA_PAGINA* tabla_pagina = malloc(sizeof(TABLA_PAGINA));
    tabla_pagina->pid = pid;
    tabla_pagina->paginas = crear_tabla_de_paginas();

    log_info(logger_procesos_creados, "PID: < %d > - Tamaño: < %d >", tabla_pagina->pid, memoria->numero_marcos);

    list_add(tablas_de_paginas, tabla_pagina);
}

bool reservar_memoria(TABLA_PAGINA* tabla_de_proceso, int cantidad){    
    if(verificar_marcos_disponibles(cantidad)){
        for(int i = 0; i < cantidad; i++){
            int index_marco = buscar_marco_libre();
            PAGINA* pagina = list_find(tabla_de_proceso->paginas, pagina_sin_frame);
            asignar_marco_a_pagina(pagina, index_marco);
        }
        return true;
    }
    return false;
}

void asignar_marco_a_pagina(PAGINA* pagina, int marco_disponible){
    memset(memoria->marcos[marco_disponible].data, 0, memoria->tam_marcos);

    pagina->marco = marco_disponible;
    pagina->bit_validacion = true;

    establecer_bit(marco_disponible, true);
}

bool pagina_sin_frame(void* data){
    PAGINA* pagina = (PAGINA*)data;
    return pagina->marco == -1;
}

bool pagina_vacia(void* data){
    PAGINA* pagina = (PAGINA*)data;
    if(pagina->marco != -1){
        return memoria->marcos[pagina->marco].tamanio == 0;
    }else{
        return false;
    }
}

bool pagina_no_vacia(void* data){
    PAGINA* pagina = (PAGINA*)data;
    if(pagina->marco != -1){
        return memoria->marcos[pagina->marco].tamanio > 0;
    }else{
        return false;
    }
}

int cantidad_de_paginas_usadas(TABLA_PAGINA* tabla){
    int contador = 0;

    for(int i = 0; i < list_size(tabla->paginas); i++){
        PAGINA* pagina = list_get(tabla->paginas, i);
        if(pagina->marco != -1){
            contador++;
        }
    }
    return contador;
}

int ultima_pagina_usada(t_list* paginas){
    int contador = list_size(paginas) - 1;
    
    for(int i = (list_size(paginas) - 1); i > 0; i--){
        PAGINA* pagina = list_get(paginas, i);
        if(pagina->marco != -1){
            break;
        }
        contador--;
    }

    return contador;
}

bool pagina_asociada_a_marco(int marco, void* data){
    PAGINA* pagina = (PAGINA*)data;
    return pagina->marco == marco;
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
        }
    }

    log_info(logger_procesos_finalizados, "PID: < %d > - TAMANIO: < 0 >", destruir->pid);

    list_destroy_and_destroy_elements(destruir->paginas, free);
    destruir->paginas=NULL;
    free(destruir);
    destruir=NULL;
}

unsigned int acceso_a_tabla_de_páginas(int pid, int pagina){
    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(pid, data);
    };

    TABLA_PAGINA* tabla = list_find(tablas_de_paginas, es_pid_de_tabla_aux);
    
    PAGINA* pag = list_get(tabla->paginas, pagina);
    if(pag->marco == -1){
        asignar_marco_a_pagina(pag, buscar_marco_libre());
    }

    log_info(logger_general, "PID: %d - Pagina: %d - Marco: %d\n", tabla->pid, pagina, pag->marco);
    
    return pag->marco;
}

// Planteamiento general cantAumentar claramente esta mal, pero es una idea de como seria
void ajustar_tamanio(TABLA_PAGINA* tabla, char* tamanio){
    int tamanio_solicitado = atoi(tamanio);
    int cantidad_de_pag_solicitadas = (int)ceil((double)tamanio_solicitado/(double)(memoria->tam_marcos));

    int paginas_usadas = cantidad_de_paginas_usadas(tabla);

    if(paginas_usadas > cantidad_de_pag_solicitadas){
        log_info(logger_instrucciones, "PID: %d - Tamanio actual: %d - Tamanio a reducir: %d\n", tabla->pid, paginas_usadas, cantidad_de_pag_solicitadas);

        char* cadena_respuesta = string_new();

        for(int j = (paginas_usadas - 1); j > (cantidad_de_pag_solicitadas - 1); j--){
            PAGINA* pagina_a_borrar = list_get(tabla->paginas, ultima_pagina_usada(tabla->paginas));    
        
            memset(memoria->marcos[pagina_a_borrar->marco].data, 0, memoria->tam_marcos);
            memoria->marcos[pagina_a_borrar->marco].tamanio = 0;
            establecer_bit(pagina_a_borrar->marco, false);

            pagina_a_borrar->marco = -1;
            pagina_a_borrar->bit_validacion = false;

            string_append(&cadena_respuesta, string_itoa(pagina_a_borrar->marco));
            string_append(&cadena_respuesta, " ");
        }   
        string_trim_right(&cadena_respuesta);
        paqueteDeMensajes(cliente_fd_cpu, cadena_respuesta, RESIZE);

    }else{
        log_info(logger_instrucciones, "PID: %d - Tamanio actual: %d - Tamanio a ampliar: %d\n", tabla->pid, paginas_usadas, cantidad_de_pag_solicitadas);
        int marcos_necesarios = cantidad_de_pag_solicitadas - paginas_usadas;

        if(verificar_marcos_disponibles(marcos_necesarios)){
            for(int r = 0; r < marcos_necesarios; r++){
                int inicio_marco = buscar_marco_libre();
                PAGINA* pagina = list_find(tabla->paginas, pagina_sin_frame);
                asignar_marco_a_pagina(pagina, inicio_marco);
            }
        }else{
            log_error(logger_instrucciones , "OUT OF MEMORY for process %d.\n", tabla->pid);
            paqueteDeMensajes(cliente_fd_cpu, "OUT OF MEMORY", OUT_OF_MEMORY);
            return;
        }
        paqueteDeMensajes(cliente_fd_cpu, "OK", RESIZE);
    }
}

//PROCESO
pcb *crear_pcb(c_proceso_data data){
    pcb *pcb_nuevo = malloc(sizeof(pcb));
    pcb_nuevo->recursos_adquiridos = list_create();


    pcb_nuevo->contexto = malloc(sizeof(cont_exec));
    pcb_nuevo->contexto->PID = data.id_proceso;
    pcb_nuevo->contexto->registros = malloc(sizeof(regCPU));

    inicializar_tabla_pagina(data.id_proceso);

    eliminarEspaciosBlanco(data.path);

    instrucciones_a_memoria* new_instrucciones = malloc(sizeof(instrucciones_a_memoria));
    new_instrucciones->pid = data.id_proceso;
    new_instrucciones->instrucciones = list_create();
    enlistar_pseudocodigo(data.path, logger_procesos_creados, new_instrucciones->instrucciones);
    list_add(memoria_de_instrucciones, new_instrucciones);
    free(data.path);
    data.path=NULL;
    return pcb_nuevo;
}

void destruir_tabla(int pid){
    list_destroy_and_destroy_elements(tablas_de_paginas, free);
}

void destruir_pcb(pcb *elemento){
    destruir_memoria_instrucciones(elemento->contexto->PID);
    destruir_tabla_pag_proceso(elemento->contexto->PID); 
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
    free(elemento);
    elemento = NULL;
}

void destruir_memoria_instrucciones(int pid){
    bool son_inst_pid_aux(void* data){
        return son_inst_pid(pid, data);
    };

    instrucciones_a_memoria* destruir = list_find(memoria_de_instrucciones, son_inst_pid_aux);

    if(!list_is_empty(destruir->instrucciones)){
        list_destroy_and_destroy_elements(destruir->instrucciones, destruir_instrucciones);
    }else{
        list_destroy(destruir->instrucciones);
    }

    free(destruir);
    destruir = NULL;
}

bool son_inst_pid(int pid, void* data){
    instrucciones_a_memoria* destruir = (instrucciones_a_memoria*)data;
    return destruir->pid == pid; 
}

// INTERFACES
void* esperar_nuevo_io(){

    while(1){

        pthread_mutex_lock(&mutex_interfaz);
        DATOS_CONEXION* datos_interfaz = malloc(sizeof(DATOS_CONEXION));
        t_list *lista;

        int socket_interfaz = esperar_cliente(server_memoria, logger_interfaces);

        int cod_op = recibir_operacion(socket_interfaz);

        if(cod_op != NUEVA_IO){ 
            /* ERROR OPERACION INVALIDA */
            log_error(logger_interfaces, "El codigo de operacion no pertenece a una nueva IO");               
            exit(-32); 
        }

        lista = recibir_paquete(socket_interfaz, logger_interfaces);
        datos_interfaz = list_get(lista, 0);
        datos_interfaz->nombre = strdup(list_get(lista, 1));
        datos_interfaz->cliente_fd = socket_interfaz;

        list_add(interfaces_conectadas, datos_interfaz);
        log_warning(logger_interfaces, "¿Quien cayo del cielo? %s, el corazon de seda. \n", datos_interfaz->nombre);
        
        args_gestionar_interfaz* args_interfaz = malloc(sizeof(args_gestionar_interfaz));
        args_interfaz->logger = logger_interfaces;
        args_interfaz->datos = datos_interfaz;
        pthread_create(&datos_interfaz->hilo_de_llegada_memoria, NULL, gestionar_nueva_io, (void*)args_interfaz);
        pthread_detach(datos_interfaz->hilo_de_llegada_memoria);

        pthread_mutex_unlock(&mutex_interfaz);
        list_destroy(lista);
    }
    return NULL;
}

void *gestionar_nueva_io (void *args){

    args_gestionar_interfaz *args_entrada = (args_gestionar_interfaz*)args;

    t_list *lista;
    char* registro_direccion;
    char* pid;

    while (1){

        int cod_op = recibir_operacion(args_entrada->datos->cliente_fd);

        switch (cod_op){

        case ESCRIBIR_MEMORIA:
            pthread_mutex_lock(&mutex_guardar_memoria);
            lista = recibir_paquete(args_entrada->datos->cliente_fd, args_entrada->logger);
            PAQUETE_ESCRITURA* paquete = list_get(lista, 0);
            paquete->direccion_fisica = list_get(lista, 1);
            paquete->dato = list_get(lista, 2);
            paquete->dato->data = list_get(lista, 3);

            escribir_en_memoria(paquete->direccion_fisica, paquete->dato, string_itoa(paquete->pid));   
            
            pthread_mutex_unlock(&mutex_guardar_memoria);       
            list_destroy(lista);
            break;
        case LEER_MEMORIA:
            lista = recibir_paquete(args_entrada->datos->cliente_fd, args_entrada->logger);
            registro_direccion = list_get(lista, 0);
            char* registro_tamanio = list_get(lista, 1);
            pid = list_get(lista, 2);

            char* dato_leido = (char*)leer_en_memoria(registro_direccion, atoi(registro_tamanio), pid);

            paquete_memoria_io(args_entrada->datos->cliente_fd, dato_leido);
            list_destroy(lista);       
            break;
        case -1:
            bool es_nombre_de_interfaz_aux(void* data){
                return nombre_de_interfaz(args_entrada->datos->nombre, data);
            };
            log_error(args_entrada -> logger, "%s se desconecto. Terminando servidor", args_entrada->datos->nombre);

            list_remove_and_destroy_by_condition(interfaces_conectadas, es_nombre_de_interfaz_aux, destruir_datos_io);
            
            free(args_entrada);
            args_entrada = NULL;
            return (void*)EXIT_FAILURE;

        default:
            log_warning(args_entrada -> logger, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

bool guardar_en_memoria(direccion_fisica dirr_fisica, t_dato* dato_a_guardar, TABLA_PAGINA* tabla) {
    int bytes_a_copiar = dato_a_guardar->tamanio;
    int tamanio_de_pagina = memoria->tam_marcos;
    
    void* copia_dato_a_guardar = malloc(bytes_a_copiar);
    memcpy(copia_dato_a_guardar, dato_a_guardar->data, bytes_a_copiar);

    bool pagina_asociada_a_marco_aux(void* data){
        return pagina_asociada_a_marco(dirr_fisica.nro_marco, data);
    };

    int bytes_copiados = 0;
    //Itero para guardar dicho dato en los marcos asignados
    if(bytes_a_copiar > 0){
        int tamanio_a_copiar;
        int bytes_restantes_en_marco = (tamanio_de_pagina - dirr_fisica.offset); // 2 0010000   10110 

        //Busco una pagina vacia de la tabla y la modifico para poder guardar ese dato consecutivamente 
        PAGINA* set_pagina = list_find(tabla->paginas, pagina_asociada_a_marco_aux);

        //Guardo en el tamaño lo que me falta para llenar la pagina
        tamanio_a_copiar = (bytes_restantes_en_marco >= bytes_a_copiar) ? bytes_a_copiar : bytes_restantes_en_marco;
        
        void* dato_a_memoria = malloc(tamanio_a_copiar);
        //Copio la memoria necesaria desde el punto en donde me quede
        memcpy(dato_a_memoria, &copia_dato_a_guardar[bytes_copiados], tamanio_a_copiar);
        
        //Completo el marco de memoria con lo que resta de memoria
        memcpy(&memoria->marcos[set_pagina->marco].data[dirr_fisica.offset], dato_a_memoria, tamanio_a_copiar);
        memoria->marcos[set_pagina->marco].tamanio += tamanio_a_copiar;
        
        bytes_copiados += tamanio_a_copiar;

        int paginas_restantes = (int)ceil((double)(bytes_a_copiar - bytes_copiados)/(double)tamanio_de_pagina);
        int pagina_actual = set_pagina->nro_pagina;

        log_info(logger_general, "PID: %d - Accion: ESCRIBIR - Direccion fisica: %d %d - Tamaño %d", tabla->pid, dirr_fisica.nro_marco, dirr_fisica.offset, tamanio_a_copiar);        
        while(bytes_copiados != bytes_a_copiar){
            //Si me quedo sin paginas y existen mas marcos disponibles pido mas memoria
            pagina_actual++;
            PAGINA* otra_pagina = list_get(tabla->paginas, pagina_actual);

            bool response = true;
            if(otra_pagina->marco == -1){
                response = verificar_marcos_disponibles(paginas_restantes);
            }
            
            if(!response){
                //En el caso de no tener memoria disponible devuelvo el proceso a EXIT
                log_error(logger_instrucciones , "OUT OF MEMORY for process %d.\n", tabla->pid);
                paqueteDeMensajes(cliente_fd_cpu, "OUT OF MEMORY", OUT_OF_MEMORY);
                break;
            }else{
                asignar_marco_a_pagina(otra_pagina, buscar_marco_libre());
                int bytes_restantes = bytes_a_copiar - bytes_copiados;
                
                tamanio_a_copiar = (bytes_restantes >= tamanio_de_pagina) ? tamanio_de_pagina : bytes_restantes;
                void* continuacion_del_dato = malloc(tamanio_a_copiar);
                
                //Copio la memoria necesaria desde el punto en donde me quede
                memcpy(continuacion_del_dato, &copia_dato_a_guardar[bytes_copiados], tamanio_a_copiar);

                memcpy(memoria->marcos[otra_pagina->marco].data, continuacion_del_dato, tamanio_a_copiar);
                memoria->marcos[otra_pagina->marco].tamanio = tamanio_a_copiar;
                
                free(continuacion_del_dato);
                bytes_copiados += tamanio_a_copiar;
                log_info(logger_general, "PID: %d - Accion: ESCRIBIR - Direccion fisica: %d %d - Tamaño %d", tabla->pid, dirr_fisica.nro_marco, dirr_fisica.offset, tamanio_a_copiar);
            }
        }     
    }     
    free(copia_dato_a_guardar);
    copia_dato_a_guardar = NULL;
    return true;
}

bool escribir_en_memoria(char* dir_fisica, t_dato* data, char* pid) {
    int id_proceso = atoi(pid);

    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(id_proceso, data);
    };

    TABLA_PAGINA* tabla = list_find(tablas_de_paginas, es_pid_de_tabla_aux);
    
    direccion_fisica dirr = obtener_marco_y_offset(dir_fisica);

    return guardar_en_memoria(dirr, data, tabla);    
}

void* leer_en_memoria(char* dir_fisica, int registro_tamanio, char* pid) {
    int bytes_leidos = 0;
    void* dato_a_devolver = malloc(registro_tamanio);
    int id_proceso = atoi(pid);

    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(id_proceso, data);
    };

    TABLA_PAGINA* tabla_de_proceso = list_find(tablas_de_paginas, es_pid_de_tabla_aux);

    direccion_fisica dirr = obtener_marco_y_offset(dir_fisica);

    bool pagina_asociada_a_marco_aux(void* data){
        return pagina_asociada_a_marco(dirr.nro_marco, data);
    };
    
    PAGINA* pagina = list_find(tabla_de_proceso->paginas, pagina_asociada_a_marco_aux);
        
    int pagina_actual = pagina->nro_pagina;
    int byte_restantes_en_marco = memoria->tam_marcos - dirr.offset;
    int bytes_a_leer_en_marco = (registro_tamanio >= byte_restantes_en_marco) ? byte_restantes_en_marco : registro_tamanio;
    
    memcpy(dato_a_devolver, &memoria->marcos[pagina->marco].data[dirr.offset], bytes_a_leer_en_marco);
    log_info(logger_general, "PID: %s - Accion: LEER - Direccion fisica: %s - Tamaño %d", pid, dir_fisica, bytes_a_leer_en_marco);
    
    bytes_leidos += bytes_a_leer_en_marco;
    while(bytes_leidos != registro_tamanio){
        int bytes_restantes_a_leer = registro_tamanio - bytes_leidos;
        pagina_actual++;
        PAGINA* otra_pagina = list_get(tabla_de_proceso->paginas, pagina_actual);

        if(otra_pagina->marco != -1){
            bytes_a_leer_en_marco = (bytes_restantes_a_leer >= memoria->tam_marcos) ? memoria->tam_marcos : bytes_restantes_a_leer;

            memcpy(&dato_a_devolver[bytes_leidos], memoria->marcos[otra_pagina->marco].data, bytes_a_leer_en_marco);
            log_info(logger_general, "PID: %s - Accion: LEER - Direccion fisica: %s - Tamaño %d", pid, dir_fisica, bytes_a_leer_en_marco);

            bytes_leidos += bytes_a_leer_en_marco;
        }else{
            return dato_a_devolver;
        }
    }    
    return dato_a_devolver;  
}

direccion_fisica obtener_marco_y_offset(char* dir_fisica){
    direccion_fisica resultado;

    char** direccion = string_n_split(dir_fisica, 2, " ");

    resultado.nro_marco = atoi(direccion[0]);
    resultado.offset = atoi(direccion[1]);

    return resultado;
}

t_config* iniciar_configuracion(){
    printf("1. Cargar configuracion para pruebas 1, 2 y 3\n");
    printf("2. Cargar configuracion para pruebas 4, 5\n");
    printf("3. Cargar configuracion para pruebas 6\n");
    char* opcion_en_string = readline("Seleccione una opción: ");
    int opcion = atoi(opcion_en_string);
    free(opcion_en_string);

    switch (opcion)
        {
        case 1:
            log_info(logger_general, "Se cargo la configuracion 1 2 3 correctamente");
            return iniciar_config("../memoria/configs/prueba_1_2_3.config");
        case 2:
            log_info(logger_general, "Se cargo la configuracion 4 5 correctamente");
            return iniciar_config("../memoria/configs/prueba_4_5.config");
        case 3:
            log_info(logger_general, "Se cargo la configuracion 6 correctamente");
            return iniciar_config("../memoria/configs/prueba_6.config");
        default:
            log_info(logger_general, "Se cargo la configuracion 1 2 3 correctamente");
            return iniciar_config("../memoria/configs/prueba_1_2_3.config");
        }
}

bool nombre_de_interfaz(char *nombre, void *data)
{
    DATOS_CONEXION *interfaz = (DATOS_CONEXION *)data;

    return !strcmp(interfaz->nombre, nombre);
}
