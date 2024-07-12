#include <memoria.h>
#include <unistd.h>

int server_memoria;
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

t_list *interfaces;
t_list *memoria_de_instrucciones;
t_list *tablas_de_paginas;
MEMORIA *memoria;

int bits_para_marco;
int bits_para_offset;

sem_t paso_instrucciones;

pthread_t hilo[4];

int main(int argc, char *argv[]){
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

    bits_para_marco = (int)log2((int)memoria->numero_marcos);
    bits_para_offset = (int)log2((int)tamanio_pagina);

    bitmap = crear_bitmap();

    tablas_de_paginas = list_create();

    memoria_de_instrucciones = list_create();
    
    /*
    Banco de pruebas
        TABLA_PAGINA* tabla = inicializar_tabla_pagina(1);

        imprimir_bitmap();

        t_dato* dato_a_guardar = malloc(sizeof(t_dato));
        dato_a_guardar->data = "Hoy me siento re zarpado nieri eh cuidado conmigo";
        dato_a_guardar->tipo = 's';

        t_dato* dato_a_guardar2 = malloc(sizeof(t_dato));
        dato_a_guardar2->data = "5";
        dato_a_guardar2->tipo = 'e';

        t_dato* dato_a_guardar3 = malloc(sizeof(t_dato));
        dato_a_guardar3->data = "Hoy me siento re zarpado nieri eh cuidado conmigo";
        dato_a_guardar3->tipo = 's';

        char* direcc_fisica = "0x0F0";
        char* direcc_fisica1 = "0x1A4";
        char* direcc_fisica2 = "0x40B";

        direccion_fisica dir_fisica = obtener_marco_y_offset(0x0F0); // 0000 111 1 0000 - Marco: 7 - Offset: 16
        direccion_fisica dir_fisica1 = obtener_marco_y_offset(0x1A4); // 0001 101 0 0100 - Marco: 13 - Offset: 4
        direccion_fisica dir_fisica2 = obtener_marco_y_offset(0x40B);  // 0100 000 0 1011 - Marco: 32  - Offset: 11

        PAGINA* pagina3 = list_get(tabla->paginas, 3);
        PAGINA* pagina59 = list_get(tabla->paginas, 59);
        PAGINA* pagina20 = list_get(tabla->paginas, 20);

        asignar_marco_a_pagina(pagina3, dir_fisica.nro_marco);
        asignar_marco_a_pagina(pagina59, dir_fisica1.nro_marco);
        asignar_marco_a_pagina(pagina20, dir_fisica2.nro_marco);

        printf("Pre-escritura\n");
        imprimir_bitmap();
        
        escribir_en_memoria(direcc_fisica, dato_a_guardar, "1");
        escribir_en_memoria(direcc_fisica1, dato_a_guardar2, "1");
        escribir_en_memoria(direcc_fisica2, dato_a_guardar3, "1");
        
        printf("\nPost-escritura\n");
        imprimir_bitmap();

        char* string = &memoria->marcos[dir_fisica.nro_marco].data[16];
        char* string2 = memoria->marcos[0].data;
        char* string3 = memoria->marcos[1].data;
        printf("Lei de memoria: %s\n", strcat(strcat(string,string2), string3));

        char* valor = leer_en_memoria(direcc_fisica, "49", "1");
        char* valor2 = leer_en_memoria(direcc_fisica1, "4", "1");
        char* valor3 = leer_en_memoria(direcc_fisica2, "49", "1");

        printf("Lei de memoria: %s\n", valor);
        printf("Lei de memoria numero: %s\n", valor2);
        printf("Lei de memoria: %s\n", valor3);

        ajustar_tamaño(tabla, "96");

        imprimir_bitmap();

    */

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

void enviar_instrucciones_a_cpu(char *program_counter, char* pid){
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

void iterar_tabla_de_paginas_e_imprimir(t_list *lista){
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

void iterar_pseudocodigo_e_imprimir(t_list *lista){
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

void *gestionar_llegada_memoria_cpu(void *args){
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        char *direccion_fisica;
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
                direccion_fisica = list_get(lista, 0);
                pid = list_get(lista, 2);
                char* tamanio = list_get(lista, 1);

                void* response = leer_en_memoria(direccion_fisica, atoi(tamanio), pid);

                paqueteDeMensajes(cliente_fd_cpu, response, RESPUESTA_LEER_MEMORIA);
                break;
            case ESCRIBIR_MEMORIA:
                lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
                direccion_fisica = list_get(lista, 0);
                pid = list_get(lista, 1);
                t_dato* dato_a_escribir = list_get(lista, 2);

                escribir_en_memoria(direccion_fisica, dato_a_escribir, pid);

                free(dato_a_escribir);
                break;
            case ACCEDER_MARCO:
                lista = recibir_paquete(args_entrada->cliente_fd, logger_instrucciones);
                PAQUETE_MARCO* acceso = list_get(lista, 0);
                int index_marco = acceso_a_tabla_de_páginas(acceso->pid, acceso->pagina);
                paqueteDeMensajes(cliente_fd_cpu, string_itoa(index_marco), ACCEDER_MARCO);
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

            log_info(logger_procesos_creados, "-Se solicito espacio para albergar el proceso n°%d-\n", id_proceso);

            response = verificar_marcos_disponibles(1);
            
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
        nueva_pagina->nro_pagina = i;
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
    return memoria->marcos[pagina->marco].tamanio == 0;
}

int cantidad_de_paginas_usadas(TABLA_PAGINA* tabla){
    int contador = 0;

    t_list_iterator* lista_paginas = list_iterator_create(tabla->paginas);
    
    while(list_iterator_has_next(lista_paginas)){
        PAGINA* pagina = list_iterator_next(lista_paginas);
        if(pagina->marco != -1){
            contador++;
        }
    }

    list_iterator_destroy(lista_paginas);
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

        for(int j = (paginas_usadas - 1); j > (cantidad_de_pag_solicitadas - 1); j--){
            PAGINA* pagina_a_borrar = list_get(tabla->paginas, ultima_pagina_usada(tabla->paginas));    
        
            memset(memoria->marcos[pagina_a_borrar->marco].data, 0, memoria->tam_marcos);
            memoria->marcos[pagina_a_borrar->marco].tamanio = 0;
            establecer_bit(pagina_a_borrar->marco, false);

            pagina_a_borrar->marco = -1;
            pagina_a_borrar->bit_validacion = false;
        }   
        paqueteDeMensajes(cliente_fd_cpu, "Se disminuyo la cantidad de paginas correctamente", RESIZE);

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
        paqueteDeMensajes(cliente_fd_cpu, "Se aumento la cantidad de paginas correctamente", RESIZE);
    }
}

//PROCESO
pcb *crear_pcb(c_proceso_data data){
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

void destruir_pcb(pcb *elemento){
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
void *gestionar_llegada_memoria_io (void *args){

    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    char* registro_direccion;
    char* pid;

    while (1){

        int cod_op = recibir_operacion(args_entrada->cliente_fd);

        switch (cod_op){

        case IO_STDIN_READ:

            lista = recibir_paquete(args_entrada->cliente_fd, logger_general);

            registro_direccion = list_get(lista,0);
            char* dato_a_escribir = list_get(lista,1);
            pid = list_get(lista, 2); // PARA LOS LOGS     

            escribir_en_memoria(registro_direccion, dato_a_escribir, pid); /* TODO: Validar si esta bien pasado el dato_a_escribir */          
 
            break;

        case IO_STDOUT_WRITE:

            lista = recibir_paquete(args_entrada->cliente_fd, logger_general);

            registro_direccion = list_get(lista, 0);
            char* registro_tamanio = list_get(lista, 1);
            pid = list_get(lista,2); // PARA LOGS

            char* dato_leido = leer_en_memoria(registro_direccion, registro_tamanio, pid);

            paquete_memoria_io(cliente_fd_io, dato_leido);        

            break;

        case -1:
            log_error(logger_general, "el cliente se desconecto. Terminando servidor");
            return (void*)EXIT_FAILURE;

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

        int socket_io = esperar_cliente(server_memoria, logger_general);     
        int cod_op = recibir_operacion(socket_io);

        if(cod_op != NUEVA_IO){ /* ERROR OPERACION INVALIDA */ exit(-32); }

        lista = recibir_paquete(socket_io,logger_general);

        interfaz_a_agregar = asignar_espacio_a_io(lista);
        interfaz_a_agregar->socket_kernel = socket_io;

        list_add(interfaces,interfaz_a_agregar);
        log_info(logger_general, "\nSe ha conectado la interfaz %s\n",interfaz_a_agregar->datos->nombre);

        interfaces_conectadas();
    }
}

void guardar_en_memoria(direccion_fisica dirr_fisica, t_dato* dato_a_guardar, TABLA_PAGINA* tabla) {
    int bytes_a_copiar = determinar_sizeof(dato_a_guardar);
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
        int bytes_restantes_en_marco = (tamanio_de_pagina - dirr_fisica.offset);

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

        log_info(logger_general, "PID: %d - Accion: ESCRIBIR - Direccion fisica: %d - Tamaño %d", tabla->pid, dirr_fisica.nro_marco, tamanio_a_copiar);        
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

                memoria->marcos[otra_pagina->marco].data = continuacion_del_dato;
                memoria->marcos[otra_pagina->marco].tamanio = tamanio_a_copiar;
                bytes_copiados += tamanio_a_copiar;
                log_info(logger_general, "PID: %d - Accion: ESCRIBIR - Direccion fisica: %d - Tamaño %d", tabla->pid, otra_pagina->marco, tamanio_a_copiar); 
            }
        }     
    }     
    if (copia_dato_a_guardar != NULL) {
        free(copia_dato_a_guardar);
        copia_dato_a_guardar = NULL; // Buena práctica: asignar NULL después de liberar la memoria
    }
}

void escribir_en_memoria(char* direccionFisica, t_dato* data, char* pid) {
    if (strncmp(direccionFisica, "0x", 2) == 0 || strncmp(direccionFisica, "0X", 2) == 0) {
        // La dirección física tiene el prefijo 0x
        direccionFisica += 2; 
    }

    int id_proceso = atoi(pid);

    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(id_proceso, data);
    };

    TABLA_PAGINA* tabla = list_find(tablas_de_paginas, es_pid_de_tabla_aux);

    unsigned int dir_fisica = (unsigned int)strtoul(direccionFisica, NULL, 16);    
    direccion_fisica dirr = obtener_marco_y_offset(dir_fisica);

    guardar_en_memoria(dirr, data, tabla);    
}

void* leer_en_memoria(char* direccionFisica, int registro_tamanio, char* pid) {
    int bytes_leidos = 0;
    void* dato_a_devolver = malloc(registro_tamanio);
    int id_proceso = atoi(pid);

    bool es_pid_de_tabla_aux(void* data){
        return es_pid_de_tabla(id_proceso, data);
    };

    TABLA_PAGINA* tabla_de_proceso = list_find(tablas_de_paginas, es_pid_de_tabla_aux);

    if (strncmp(direccionFisica, "0x", 2) == 0 || strncmp(direccionFisica, "0X", 2) == 0) {
        // La dirección física tiene el prefijo 0x
        direccionFisica += 2; 
    }

    unsigned int dir_fisica = (unsigned int)strtoul(direccionFisica, NULL, 16);
    direccion_fisica dirr = obtener_marco_y_offset(dir_fisica);

    bool pagina_asociada_a_marco_aux(void* data){
        return pagina_asociada_a_marco(dirr.nro_marco, data);
    };
    
    PAGINA* pagina = list_find(tabla_de_proceso->paginas, pagina_asociada_a_marco_aux);
    int pagina_actual = pagina->nro_pagina;
    int byte_restantes_en_marco = memoria->tam_marcos - dirr.offset;
    int bytes_a_leer_en_marco = (registro_tamanio >= byte_restantes_en_marco) ? byte_restantes_en_marco : registro_tamanio;
    
    memcpy(dato_a_devolver, &memoria->marcos[pagina->marco].data[dirr.offset], bytes_a_leer_en_marco);
    log_info(logger_general, "PID: %s - Accion: LEER - Direccion fisica: %s - Tamaño %d", pid, direccionFisica, bytes_a_leer_en_marco);
    
    bytes_leidos += bytes_a_leer_en_marco;
    while(bytes_leidos != registro_tamanio){
        int bytes_restantes_a_leer = registro_tamanio - bytes_leidos;
        pagina_actual++;
        PAGINA* otra_pagina = list_get(tabla_de_proceso->paginas, pagina_actual);    
        bytes_a_leer_en_marco = (bytes_restantes_a_leer >= memoria->tam_marcos) ? memoria->tam_marcos : bytes_restantes_a_leer;

        memcpy(&dato_a_devolver[bytes_leidos], memoria->marcos[otra_pagina->marco].data, bytes_a_leer_en_marco);
        log_info(logger_general, "PID: %s - Accion: LEER - Direccion fisica: %s - Tamaño %d", pid, direccionFisica, bytes_a_leer_en_marco);

        bytes_leidos += bytes_a_leer_en_marco;
    }
    return dato_a_devolver;  
}

direccion_fisica obtener_marco_y_offset(int dir_fisica){
    direccion_fisica resultado;
    int frame_mask = (1 << (32 - bits_para_offset)) - 1; // Máscara para obtener el número de marco

    resultado.nro_marco = (dir_fisica >> bits_para_offset) & frame_mask;
    resultado.offset = dir_fisica & (memoria->tam_marcos - 1); // Máscara para obtener el desplazamiento

    return resultado;
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