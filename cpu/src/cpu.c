#include <cpu.h>
#include <utils/parse.h>

int conexion_memoria;
int server_interrupt;
int server_dispatch;
int cliente_fd_dispatch;
int tam_pagina;
int pagina_aux;

bool flag_ejecucion;
bool flag_escritura;

char *instruccion_a_ejecutar;
char *interrupcion;
void* memoria_response;
char *memoria_marco_response;
TLB *tlb;
int cant_ent_tlb;
char *algoritmo_tlb;

t_log *logger_cpu;
t_config *config;

cont_exec *contexto;

REGISTER register_map[11];
const int num_register = sizeof(register_map) / sizeof(REGISTER);

pthread_mutex_t mutex_ejecucion = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_tlb = PTHREAD_MUTEX_INITIALIZER;

pthread_t hilo_proceso;

sem_t sem_contexto;
sem_t sem_ejecucion;
sem_t sem_instruccion;
sem_t sem_interrupcion;
sem_t sem_respuesta_memoria;
sem_t sem_respuesta_marco;

INSTRUCTION instructions[] = {
    {"SET", set, "Ejecutar SET", 0},
    {"MOV_IN", mov_in, "Ejecutar MOV_IN", 1},
    {"MOV_OUT", mov_out, "Ejecutar MOV_OUT", 0},
    {"SUM", sum, "Ejecutar SUM", 0},
    {"SUB", sub, "Ejecutar SUB", 0},
    {"JNZ", jnz, "Ejecutar JNZ", 0},
    {"RESIZE", resize, "Ejecutar RESIZE", 0},
    {"COPY_STRING", copy_string, "Ejecutar COPY_STRING", 0},
    {"WAIT", WAIT, "Ejecutar WAIT", 0},
    {"SIGNAL", SIGNAL, "Ejecutar SIGNAL", 0},
    {"IO_GEN_SLEEP", io_gen_sleep, "Ejecutar IO_GEN_SLEEP", 0},
    {"IO_STDIN_READ", io_stdin_read, "Ejecutar IO_STDIN_READ", 1},
    {"IO_STDOUT_WRITE", io_stdout_write, "Ejecutar IO_STDOUT_WRITE", 1},
    {"IO_FS_CREATE", io_fs_create, "Ejecutar IO_FS_CREATE", 0},
    {"IO_FS_DELETE", io_fs_delete, "Ejecutar IO_FS_DELETE", 0},
    {"IO_FS_TRUNCATE", io_fs_trucate, "Ejecutar IO_FS_TRUNCATE", 0},
    {"IO_FS_READ", io_fs_read, "Ejecutar IO_FS_READ", 2},
    {"IO_FS_WRITE", io_fs_write, "Ejecutar IO_FS_WRITE", 2},
    {"EXIT", EXIT, "Syscall, devuelve el contexto a kernel", 0},
    {NULL, NULL, NULL, 0}};

const char *instrucciones_logicas[6] = {"MOV_IN", "MOV_OUT", "IO_STDIN_READ", "IO_STDOUT_WRITE", "IO_FS_WRITE", "IO_FS_READ"};

int main(int argc, char *argv[])
{
    int i;
    flag_escritura = true;
    logger_cpu = iniciar_logger("../cpu/cpu.log", "cpu-log", LOG_LEVEL_INFO);
    log_info(logger_cpu, "logger para CPU creado exitosamente.");

    config = iniciar_configuracion();

    pthread_t hilo_id[4];

    char *ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    char *puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    char *puerto_dispatch = config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH");
    char *puerto_interrupt = config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT");

    cant_ent_tlb = config_get_int_value(config, "CANTIDAD_ENTRADAS_TLB");
    algoritmo_tlb = config_get_string_value(config, "ALGORITMO_TLB");

    log_info(logger_cpu, "INFO DE MEMORIA %s %s", ip_memoria, puerto_memoria);

    // Inicializar tlb
    if(cant_ent_tlb > 0){
        tlb = inicializar_tlb(cant_ent_tlb);
    }

    server_dispatch = iniciar_servidor(logger_cpu, puerto_dispatch);
    log_info(logger_cpu, "Servidor dispatch abierto");
    server_interrupt = iniciar_servidor(logger_cpu, puerto_interrupt);
    log_info(logger_cpu, "Servidor interrupt abierto");

    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_operacion("CPU IS IN DA HOUSE", conexion_memoria, MENSAJE);

    cliente_fd_dispatch = esperar_cliente(server_dispatch, logger_cpu);
    int cliente_fd_interrupt = esperar_cliente(server_interrupt, logger_cpu);

    sem_init(&sem_contexto, 1, 1);
    sem_init(&sem_ejecucion, 1, 0);
    sem_init(&sem_interrupcion, 1, 0);
    sem_init(&sem_instruccion, 1, 0);
    sem_init(&sem_respuesta_memoria, 1, 0);
    sem_init(&sem_respuesta_marco, 1, 0);

    ArgsGestionarServidor args_dispatch = {logger_cpu, cliente_fd_dispatch};
    ArgsGestionarServidor args_interrupt = {logger_cpu, cliente_fd_interrupt};
    ArgsGestionarServidor args_memoria = {logger_cpu, conexion_memoria};

    pthread_create(&hilo_id[0], NULL, gestionar_llegada_kernel, &args_dispatch);
    pthread_create(&hilo_id[1], NULL, gestionar_llegada_kernel, &args_interrupt);
    pthread_create(&hilo_id[2], NULL, gestionar_llegada_memoria, &args_memoria);

    for (i = 0; i < 5; i++)
    {
        pthread_join(hilo_id[i], NULL);
    }

    sem_destroy(&sem_ejecucion);
    sem_destroy(&sem_interrupcion);
    sem_destroy(&sem_respuesta_memoria);
    liberar_conexion(conexion_memoria);
    terminar_programa(logger_cpu, config);

    return 0;
}

void Execute(RESPONSE *response, cont_exec *contexto)
{
    if (response != NULL)
    {
        for (int i = 0; instructions[i].command != NULL; i++)
        {
            if (strcmp(instructions[i].command, response->command) == 0)
            {
                instructions[i].function(response->params);

                free(response->command);
                response->command = NULL;
                string_array_destroy(response->params);
                free(response);
                response = NULL;
                return;
            }
        }
    }
}

RESPONSE *Decode(char *instruccion)
{
    RESPONSE *response;
    response = parse_command(instruccion);
    int index = 0;
    char* direccion_fisica;

    //Encontrar comando
    if (response != NULL)
    {
        for (int i = 0; instructions[i].command != NULL; i++)
        {
            if (strcmp(instructions[i].command, response->command) == 0)
            {
                index = instructions[i].posicion_direccion_logica;
            }
        }

        // Traducir una direccion logica a fisica (solo para funciones que la requieren)
        int cant_commands = sizeof(instrucciones_logicas) / sizeof(instrucciones_logicas[0]);
        for(int i = 0; i < cant_commands; i++) {
            if(!strcmp(response->command, instrucciones_logicas[i])) {
                REGISTER* registro_direccion = find_register(response->params[index]);
                
                DIRECCION_LOGICA direccion;

                if (registro_direccion->type == TYPE_UINT32) {
                    direccion = obtener_pagina_y_offset(*(uint32_t*)registro_direccion->registro);
                } else {
                    direccion = obtener_pagina_y_offset(*(uint8_t*)registro_direccion->registro);
                }

                pagina_aux = direccion.nro_pagina;

                if(cant_ent_tlb > 0){
                    int index_marco = chequear_en_tlb(contexto->PID, direccion.pagina);

                    if(index_marco != -1) {
                        log_info(logger_cpu, "PID: %d - TLB HIT - Pagina: %d", contexto->PID, direccion.pagina);
                        direccion_fisica = malloc(strlen(string_itoa(index_marco))+1+strlen(string_itoa(direccion.offset))+3);
                        strcpy(direccion_fisica, string_itoa(index_marco));
                        strcat(direccion_fisica, " ");
                        strcat(direccion_fisica, string_itoa(direccion.offset));
                        response->params[index] = direccion_fisica;
                    } else {
                        log_info(logger_cpu, "PID: %d - TLB MISS - Pagina: %d", contexto->PID, direccion.pagina);
                    
                        direccion_fisica = mmu(direccion);

                        response->params[index] = direccion_fisica;
                        pthread_mutex_lock(&mutex_tlb);
                        agregar_en_tlb(contexto->PID, direccion.pagina, atoi(memoria_marco_response));
                        pthread_mutex_unlock(&mutex_tlb);
                    }    
                }else{
                    direccion_fisica = mmu(direccion);

                    response->params[index] = direccion_fisica;
                }
                break;
            }
        }
    }

    return response;
}

void Fetch(cont_exec *contexto)
{
    t_instruccion* fetch = malloc(sizeof(t_instruccion));
    fetch->pc = strdup(string_itoa(contexto->registros->PC));
    fetch->pid = strdup(string_itoa(contexto->PID));

    paquete_solicitud_instruccion(conexion_memoria, fetch); // Enviamos instruccion para mandarle la instruccion que debe mandarnos

    log_debug(logger_cpu, "Se solicito a memoria el paso de la instruccion n°%d", contexto->registros->PC);
    
    sem_wait(&sem_instruccion);

    free(fetch->pc);
    fetch->pc = NULL;
    free(fetch->pid);
    fetch->pid = NULL;
    free(fetch);
    fetch = NULL;
}

void procesar_contexto(cont_exec* contexto)
{
    while (flag_ejecucion)
    { 
        RESPONSE *response;
        Fetch(contexto);
    
        log_debug(logger_cpu, "El decode recibio %s", instruccion_a_ejecutar);

        response = Decode(instruccion_a_ejecutar);

        if (es_motivo_de_salida(response->command))
        {
            contexto->registros->PC++;
            Execute(response, contexto);
            sem_post(&sem_contexto);
            return;
        }

        contexto->registros->PC++;
        Execute(response, contexto);
    }

    enviar_contexto_pcb(cliente_fd_dispatch, contexto, determinar_op(interrupcion));
    
    pthread_mutex_lock(&mutex_ejecucion);
    log_info(logger_cpu, "Desalojando registro. MOTIVO: %s\n", interrupcion);
    pthread_mutex_unlock(&mutex_ejecucion);
    
    sem_post(&sem_contexto);
}

void *gestionar_llegada_kernel(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case MENSAJE:
            recibir_mensaje(args_entrada->cliente_fd, logger_cpu, MENSAJE);
            break;
        case INTERRUPCION:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            pthread_mutex_lock(&mutex_ejecucion);
            flag_ejecucion = false;
            interrupcion = list_get(lista, 0);
            pthread_mutex_unlock(&mutex_ejecucion);
            list_destroy(lista);
            break;
        case CONTEXTO:
            sem_wait(&sem_contexto);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            contexto = list_get(lista, 0);
            contexto->registros = list_get(lista, 1);
            log_debug(logger_cpu, "Recibi un contexto PID: %d", contexto->PID);
            log_debug(logger_cpu, "PC del CONTEXTO: %d", contexto->registros->PC);
            flag_ejecucion = true;
            procesar_contexto(contexto);
            list_destroy(lista);
            break;
        case -1:
            log_error(logger_cpu, "el cliente se desconecto. Terminando servidor");
            return (void*)EXIT_FAILURE;
        default:
            log_warning(logger_cpu, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

void *gestionar_llegada_memoria(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case MENSAJE:
            lista = recibir_paquete(args_entrada->cliente_fd, args_entrada->logger);
            tam_pagina = atoi((char*)list_get(lista, 0));
            list_destroy(lista);
            break;
        case RESPUESTA_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            instruccion_a_ejecutar = list_get(lista, 0);
            log_info(logger_cpu, "PID: %d - FETCH - Program Counter: %d", contexto->PID, contexto->registros->PC);

            sem_post(&sem_instruccion);
            list_destroy(lista);
            break;
        case RESPUESTA_LEER_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            memoria_response = list_get(lista, 0);
            sem_post(&sem_respuesta_memoria);
            list_destroy(lista);
            break;
        case RESPUESTA_ESCRIBIR_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            log_debug(logger_cpu, "Se escribio correctamente en memoria!");
            sem_post(&sem_respuesta_memoria);
            list_destroy(lista);
            break;
        case OUT_OF_MEMORY:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            pthread_mutex_lock(&mutex_ejecucion);
            flag_ejecucion = false;
            interrupcion = list_get(lista, 0);
            pthread_mutex_unlock(&mutex_ejecucion);
            sem_post(&sem_respuesta_memoria);
            list_destroy(lista);
            break;
        case RESIZE:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            char* mensaje = list_get(lista, 0);
            log_info(logger_cpu, "-%s-", mensaje);
            sem_post(&sem_respuesta_memoria);
            list_destroy(lista);
            break;
        case CAMBIO_TLB:
            pthread_mutex_lock(&mutex_tlb);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            PAQUETE_TLB* paquete = list_get(lista, 0);
            log_info(logger_cpu, "Se solicito cambiar el marco del PID: %d a %d referenciado por la pagina %d", paquete->pid, paquete->marco, paquete->pagina);
            agregar_en_tlb(paquete->pid, pagina_aux, paquete->marco);
            pthread_mutex_unlock(&mutex_tlb);
            free(paquete);
            paquete = NULL;
            list_destroy(lista);
            flag_escritura = true;
            sem_post(&sem_respuesta_memoria);
            break;
        case ACCEDER_MARCO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            memoria_marco_response = list_get(lista, 0);
            sem_post(&sem_respuesta_marco);
            list_destroy(lista);
            break;
        case -1:
            log_error(logger_cpu, "el cliente se desconecto. Terminando servidor");
            return (void*)EXIT_FAILURE;
        default:
            log_warning(logger_cpu, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

// Función para inicializar el mapeo de registros
void upload_register_map()
{
    register_map[0] = (REGISTER){"PC", (uint32_t *)&contexto->registros->PC, TYPE_UINT32};
    register_map[1] = (REGISTER){"AX", (uint8_t *)&contexto->registros->AX, TYPE_UINT8};
    register_map[2] = (REGISTER){"BX", (uint8_t *)&contexto->registros->BX, TYPE_UINT8};
    register_map[3] = (REGISTER){"CX", (uint8_t *)&contexto->registros->CX, TYPE_UINT8};
    register_map[4] = (REGISTER){"DX", (uint8_t *)&contexto->registros->DX, TYPE_UINT8};
    register_map[5] = (REGISTER){"EAX", (uint32_t *)&contexto->registros->EAX, TYPE_UINT32};
    register_map[6] = (REGISTER){"EBX", (uint32_t *)&contexto->registros->EBX, TYPE_UINT32};
    register_map[7] = (REGISTER){"ECX", (uint32_t *)&contexto->registros->ECX, TYPE_UINT32};
    register_map[8] = (REGISTER){"EDX", (uint32_t *)&contexto->registros->EDX, TYPE_UINT32};
    register_map[9] = (REGISTER){"SI", (uint32_t *)&contexto->registros->SI, TYPE_UINT32};
    register_map[10] = (REGISTER){"DI", (uint32_t *)&contexto->registros->DI, TYPE_UINT32};
}

REGISTER *find_register(const char *name)
{
    upload_register_map();
    for (int i = 0; i < num_register; i++)
    {
        if (!strcmp(register_map[i].name, name))
        {
            return &register_map[i];
        }
    }
    return NULL;
}

void set(char **params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: SET - %s %s", contexto->PID, params[0], params[1]);

    const char *register_name = params[0];
    int new_register_value = atoi(params[1]);

    REGISTER *found_register = find_register(register_name);
    if (found_register->type == TYPE_UINT32){
        *(uint32_t *)found_register->registro = (uint32_t)new_register_value;
        log_debug(logger_cpu, "Nuevo valor registro: %d", *(uint32_t *)found_register->registro);
    }
    else if (found_register->type == TYPE_UINT8){
        *(uint8_t *)found_register->registro = (uint8_t)new_register_value;
        log_debug(logger_cpu, "Nuevo valor registro: %d", *(uint8_t *)found_register->registro);
    }
    else{
        printf("Registro desconocido: %s\n", register_name);
    }
 }

// primer parametro: destino (TARGET), segundo parametro: origen (ORIGIN)
void sum(char **params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: SUM - %s %s", contexto->PID, params[0], params[1]);

    char* first_register = params[0];
    char* second_register = params[1];
    eliminarEspaciosBlanco(second_register);

    REGISTER *register_target = find_register(first_register);
    REGISTER *register_origin = find_register(second_register);

   if (register_target->type == TYPE_UINT32 && register_origin->type == TYPE_UINT32){
        *(uint32_t *)register_target->registro += *(uint32_t *)register_origin->registro;
        log_debug(logger_cpu, "Nuevo valor registro destino: %d", *(uint32_t *)register_target->registro);
    }
    else if (register_target->type == TYPE_UINT8 && register_origin->type == TYPE_UINT8){
        *(uint8_t *)register_target->registro += *(uint8_t *)register_origin->registro;
        log_debug(logger_cpu, "Nuevo valor registro destino: %d", *(uint32_t *)register_target->registro);
    }
    else if (register_target->type == TYPE_UINT8 && register_origin->type == TYPE_UINT32){
        *(uint8_t *)register_target->registro += *(uint32_t *)register_origin->registro;
        log_debug(logger_cpu, "Nuevo valor registro destino: %d", *(uint32_t *)register_target->registro);
    }else{
        *(uint32_t *)register_target->registro += *(uint8_t *)register_origin->registro;
        log_debug(logger_cpu, "Nuevo valor registro destino: %d", *(uint32_t *)register_target->registro);
    }
}

void sub(char **params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: SUB - %s %s", contexto->PID, params[0], params[1]);
    char* first_register = params[0];
    char* second_register = params[1];
    eliminarEspaciosBlanco(second_register);

    REGISTER *register_target = find_register(first_register);
    REGISTER *register_origin = find_register(second_register);

     if (register_target->type == TYPE_UINT32 && register_origin->type == TYPE_UINT32){
        *(uint32_t *)register_target->registro -= *(uint32_t *)register_origin->registro;
        log_debug(logger_cpu, "Nuevo valor registro destino: %d", *(uint32_t *)register_target->registro);
    }
    else if (register_target->type == TYPE_UINT8 && register_origin->type == TYPE_UINT8){
        *(uint8_t *)register_target->registro -= *(uint8_t *)register_origin->registro;
        log_debug(logger_cpu, "Nuevo valor registro destino: %d", *(uint8_t *)register_target->registro);
    }
    else if (register_target->type == TYPE_UINT8 && register_origin->type == TYPE_UINT32){
        *(uint8_t *)register_target->registro -= *(uint32_t *)register_origin->registro;
        log_debug(logger_cpu, "Nuevo valor registro destino: %d", *(uint32_t *)register_target->registro);
    }else{
        *(uint32_t *)register_target->registro -= *(uint8_t *)register_origin->registro;
        log_debug(logger_cpu, "Nuevo valor registro destino: %d", *(uint32_t *)register_target->registro);
    }
}

void jnz(char **params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: JNZ - %s %s", contexto->PID, params[0], params[1]);

    const char *register_name = params[0];
    const int next_instruction = atoi(params[1]);

    REGISTER *found_register = find_register(register_name);

    if (found_register != NULL)
    {
        if (found_register->registro != 0)
        {
            contexto->registros->PC = next_instruction;
        }
    }
    else
    {
        log_error(logger_cpu, "Registro no encontrado o puntero nulo\n");
    }
}

void resize(char **tamanio_a_modificar)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: RESIZE - %s", contexto->PID, tamanio_a_modificar[0]);
    t_resize* info_rsz = malloc(sizeof(t_resize));
    info_rsz->tamanio = strdup(tamanio_a_modificar[0]);
    info_rsz->pid = contexto->PID;

    log_debug(logger_cpu, "-RESIZE: Cambiar tamanio del proceso a %d\n", atoi(info_rsz->tamanio));
    paquete_resize(conexion_memoria, info_rsz);
    sem_wait(&sem_respuesta_memoria);

    free(info_rsz->tamanio);
    info_rsz->tamanio = NULL;
    free(info_rsz);
    info_rsz = NULL;
}

void copy_string(char **params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: COPY STRING - %s", contexto->PID, params[0]);
    int tamanio = atoi(params[0]);

    REGISTER* registro_SI = find_register("SI");
    REGISTER* registro_DI = find_register("DI");

    if(registro_SI == NULL || registro_DI == NULL) {
        log_error(logger_cpu, "No se encontro el registro");
        return;
    }

    PAQUETE_COPY_STRING* paquete = malloc(sizeof(PAQUETE_COPY_STRING));
    paquete->pid = strdup(string_itoa(contexto->PID));

    DIRECCION_LOGICA direccion_logica_SI = obtener_pagina_y_offset(*(uint32_t*)registro_SI->registro);
    DIRECCION_LOGICA direccion_logica_DI = obtener_pagina_y_offset(*(uint32_t*)registro_DI->registro);

    paquete->direccion_fisica_origen = strdup(mmu(direccion_logica_SI));
    paquete->direccion_fisica_destino = strdup(mmu(direccion_logica_DI));
    paquete->tamanio = string_itoa(tamanio);

    paquete_copy_string(conexion_memoria, paquete);

    sem_wait(&sem_respuesta_memoria);
    free(paquete->pid);
    paquete->pid = NULL;
    free(paquete->direccion_fisica_destino);
    paquete->direccion_fisica_destino = NULL;
    free(paquete->direccion_fisica_origen);
    paquete->direccion_fisica_origen = NULL;
    free(paquete->tamanio);
    paquete->tamanio = NULL;
}

void WAIT(char **params){
    log_info(logger_cpu, "PID: %d - Ejecutando: WAIT - %s", contexto->PID, params[0]);
    char* name_recurso = params[0];
    paqueteRecurso(cliente_fd_dispatch, contexto, name_recurso, O_WAIT);
}

void SIGNAL(char **params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: SIGNAL - %s", contexto->PID, params[0]);
    char* name_recurso = params[0];
    paqueteRecurso(cliente_fd_dispatch, contexto, name_recurso, O_SIGNAL);
}

void io_gen_sleep(char **params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: IO_GEN_SLEEP - %s %s", contexto->PID, params[0], params[1]);

    char *interfaz_name = params[0];
    char **args = string_array_new();
    string_array_push(&args, params[1]);
    solicitar_interfaz(interfaz_name, "IO_GEN_SLEEP", args);
}

void io_stdin_read(char ** params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: IO_STDIN_READ - %s %s %s\n", contexto->PID, params[0], params[1], params[2]);

    char *interfaz_name = params[0];
    char *registro_direccion = params[1];
    char *registro_tamanio = params[2];
    char **args = string_array_new();

    REGISTER* found_register = find_register(registro_tamanio);
    
    string_array_push(&args, registro_direccion);

    if(found_register->type == TYPE_UINT32){
        string_array_push(&args, string_itoa(*(uint32_t*)found_register->registro));
    }else{
        string_array_push(&args, string_itoa(*(uint8_t*)found_register->registro));
    }

    solicitar_interfaz(interfaz_name, "IO_STDIN_READ", args);
}

void mov_in(char **params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: MOV_IN - %s %s\n", contexto->PID, params[0], params[1]);
    char* registro_datos = params[0];
    char* direccion_fisica = params[1];

    REGISTER *found_register = find_register(registro_datos);

    if(found_register == NULL) {
        log_error(logger_cpu, "No se encontro el registro");
        return;
    }
    
    PAQUETE_LECTURA* paquete_lectura = malloc(sizeof(PAQUETE_LECTURA));
    paquete_lectura->direccion_fisica = direccion_fisica;
    paquete_lectura->pid = strdup(string_itoa(contexto->PID));
    if (found_register->type == TYPE_UINT32) {
        paquete_lectura->tamanio = strdup(string_itoa(sizeof(uint32_t)));
    } else {
        paquete_lectura->tamanio = strdup(string_itoa(sizeof(uint8_t)));
    }

    paquete_leer_memoria(conexion_memoria, paquete_lectura);

    sem_wait(&sem_respuesta_memoria);

    free(paquete_lectura->tamanio);
    paquete_lectura->tamanio = NULL;
    free(paquete_lectura->pid);
    paquete_lectura->pid = NULL;
    free(paquete_lectura);
    paquete_lectura = NULL;
}

void mov_out(char **params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: MOV_OUT - %s %s\n", contexto->PID, params[0], params[1]);
    char* direccion_fisica = params[0];
    char* registro_datos = params[1];

    REGISTER *found_register = find_register(registro_datos);

    if(found_register == NULL) {
        log_error(logger_cpu, "No se encontro el registro");
        return;
    }

    PAQUETE_ESCRITURA* paquete_escritura = malloc(sizeof(PAQUETE_ESCRITURA));
    paquete_escritura->pid = contexto->PID;
    paquete_escritura->direccion_fisica = direccion_fisica;
    paquete_escritura->dato = malloc(sizeof(t_dato));
    if (found_register->type == TYPE_UINT32) {
        paquete_escritura->dato->data = malloc(sizeof(uint32_t));
        paquete_escritura->dato->data = (uint32_t*)found_register->registro;
        paquete_escritura->dato->tamanio = 4;
    } else {
        paquete_escritura->dato->data = malloc(sizeof(u_int8_t));
        paquete_escritura->dato->data = (uint8_t*)found_register->registro;
        paquete_escritura->dato->tamanio = 1;
    }
    
    paquete_escribir_memoria(conexion_memoria, paquete_escritura);

    sem_wait(&sem_respuesta_memoria);

    if(flag_escritura){
        int marco = chequear_en_tlb(contexto->PID, pagina_aux);
        char** direccion = string_n_split(dir_fisica, 2, " ");
        
        char* nueva_direccion = malloc(strlen(string_itoa(marco)) + 3 + strlen(direccion[1]) + 1);
        strcpy(nueva_direccion, string_itoa(marco));
        strcat(nueva_direccion, " ");
        strcat(nueva_direccion, direccion[1]);
        
        paquete_escritura->direccion_fisica = nueva_direccion; 
        paquete_escribir_memoria(conexion_memoria, paquete_escritura);

        flag_escritura = false;
        sem_wait(&sem_respuesta_memoria);
    }


    free(paquete_escritura->nueva_direccion);
    paquete_escritura->nueva_direccion = NULL;
    free(paquete_escritura->dato);
    paquete_escritura->dato = NULL;
    free(paquete_escritura);
    paquete_escritura=NULL;
}


void io_stdout_write(char **params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: IO_STDOUT_WRITE - %s %s %s\n", contexto->PID, params[0], params[1], params[2]);

    char *interfaz_name = params[0];
    char *registro_direccion = params[1];
    char *registro_tamanio = params[2];

    REGISTER* found_register = find_register(registro_tamanio);

    char **args = string_array_new();
    string_array_push(&args, registro_direccion);

    if(found_register->type == TYPE_UINT32){
        string_array_push(&args, string_itoa(*(uint32_t*)found_register->registro));
    }else{
        string_array_push(&args, string_itoa(*(uint8_t*)found_register->registro));
    }

    solicitar_interfaz(interfaz_name, "IO_STDOUT_WRITE", args);
}

void io_fs_create(char** params){
    log_info(logger_cpu, "PID: %d - Ejecutando: IO_FS_CREATE - %s %s\n", contexto->PID, params[0], params[1]);

    char* interfaz_name = params[0];

    char **args = string_array_new();
    string_array_push(&args, params[1]);

    solicitar_interfaz(interfaz_name, "IO_FS_CREATE", args);
}

void io_fs_delete(char** params){
    log_info(logger_cpu, "PID: %d - Ejecutando: IO_FS_DELETE - %s %s\n", contexto->PID, params[0], params[1]);

    char* interfaz_name = params[0];

    char **args = string_array_new();
    string_array_push(&args, params[1]);

    solicitar_interfaz(interfaz_name, "IO_FS_DELETE", args);
}

void io_fs_trucate(char** params){
    log_info(logger_cpu, "PID: %d - Ejecutando: IO_FS_TRUNCATE - %s %s %s\n", contexto->PID, params[0], params[1], params[2]);
    char* interfaz = params[0];

    REGISTER *registro_tamanio = find_register(params[2]);

    char **args = string_array_new();
    string_array_push(&args, params[1]);

    if(registro_tamanio->type == TYPE_UINT32){
        string_array_push(&args, string_itoa(*(uint32_t*)registro_tamanio->registro));
    }else{
        string_array_push(&args, string_itoa(*(uint8_t*)registro_tamanio->registro));
    }

    solicitar_interfaz(interfaz,"IO_FS_TRUNCATE",args);
}

void io_fs_read(char** params){
    log_info(logger_cpu, "PID: %d - Ejecutando: IO_FS_READ - %s %s %s %s %s\n", contexto->PID, params[0], params[1], params[2], params[3], params[4]);
    char* interfaz= params[0];

    char ** args= string_array_new();
    string_array_push(&args,params[1]);
    string_array_push(&args,params[2]);
    
    REGISTER* registro_tamanio = find_register(params[3]);
    REGISTER* registro_puntero = find_register(params[4]);

    if(registro_tamanio->type == TYPE_UINT32){
        string_array_push(&args, string_itoa(*(uint32_t*)registro_tamanio->registro));
    }else{
        string_array_push(&args, string_itoa(*(uint8_t*)registro_tamanio->registro));
    }

    if(registro_puntero->type == TYPE_UINT32){
        string_array_push(&args, string_itoa(*(uint32_t*)registro_puntero->registro));
    }else{
        string_array_push(&args, string_itoa(*(uint8_t*)registro_puntero->registro));
    }
    solicitar_interfaz(interfaz,"IO_FS_READ",args);
}

void io_fs_write(char** params){
    log_info(logger_cpu, "PID: %d - Ejecutando: IO_FS_WRITE - %s %s %s %s %s\n", contexto->PID, params[0], params[1], params[2], params[3], params[4]);

    char* interfaz_name = params[0];
    char* file_name = params[1];
    char* registro_direccion = params[2];

    char **args = string_array_new();
    string_array_push(&args, file_name);
    string_array_push(&args, registro_direccion);
    
    REGISTER* registro_tamanio = find_register(params[3]);
    REGISTER* registro_puntero = find_register(params[4]);

    if(registro_tamanio->type == TYPE_UINT32){
        string_array_push(&args, string_itoa(*(uint32_t*)registro_tamanio->registro));
    }else{
        string_array_push(&args, string_itoa(*(uint8_t*)registro_tamanio->registro));
    }

    if(registro_puntero->type == TYPE_UINT32){
        string_array_push(&args, string_itoa(*(uint32_t*)registro_puntero->registro));
    }else{
        string_array_push(&args, string_itoa(*(uint8_t*)registro_puntero->registro));
    }

    solicitar_interfaz(interfaz_name, "IO_FS_WRITE", args);
}

void EXIT(char **params)
{
    log_info(logger_cpu, "PID: %d - Ejecutando: EXIT - SIN PARAMETROS", contexto->PID);
    enviar_contexto_pcb(cliente_fd_dispatch, contexto, CONTEXTO);
}

void solicitar_interfaz(char *interfaz_name, char *solicitud, char **argumentos)
{
    SOLICITUD_INTERFAZ* aux = malloc(sizeof(SOLICITUD_INTERFAZ));
    aux->nombre = strdup(interfaz_name);
    aux->solicitud = strdup(solicitud);
    aux->args = argumentos;

    paqueteIO(cliente_fd_dispatch, aux, contexto);

    free(aux->nombre);
    aux->nombre = NULL;
    free(aux->solicitud);
    aux->solicitud = NULL;
    string_array_destroy(argumentos);
    free(aux);
    aux = NULL;
}

const char *motivos_de_salida[11] = {"EXIT", "IO_GEN_SLEEP", "IO_STDIN_READ", "IO_STDOUT_WRITE", "WAIT", "SIGNAL", "IO_FS_CREATE", "IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE", "IO_FS_READ"};

bool es_motivo_de_salida(const char *command)
{
    int num_commands = sizeof(motivos_de_salida) / sizeof(motivos_de_salida[0]);
    for (int i = 0; i < num_commands; i++)
    {
        if (strcmp(motivos_de_salida[i], command) == 0)
        {
            flag_ejecucion = false;
            return true;
        }
    }
    return false;
}

char* traducirDireccionLogica(DIRECCION_LOGICA direccion_logica) {
    PAQUETE_MARCO *paquete = malloc(sizeof(PAQUETE_MARCO));
    paquete->pagina = direccion_logica.pagina;
    paquete->pid = contexto->PID;

    paquete_marco(conexion_memoria, paquete);

    free(paquete);
    paquete = NULL;
    
    // Espero la respuesta de memoria
    sem_wait(&sem_respuesta_marco);
    
    char *s2 = string_itoa(direccion_logica.offset);
    
    char* direccionFisica = malloc(strlen(memoria_marco_response) + strlen(s2) + 4);
    strcpy(direccionFisica, memoria_marco_response);
    strcat(direccionFisica, " ");
    strcat(direccionFisica, s2);

    return direccionFisica;
}

char* mmu (DIRECCION_LOGICA direccion_logica){
    // Direcciones de 12 bits -> 1 | 360
    return traducirDireccionLogica(direccion_logica);
}


TLB *inicializar_tlb(int entradas) {
    TLB *tlb = malloc(sizeof(TLB));
    tlb->entradas = list_create();
    return tlb;
}

bool es_pid_pag(int pid, int pag, void* data) {
    TLBEntry* a_buscar = (TLBEntry*)data;
    return (a_buscar->pid == pid && a_buscar->pagina == pag);
}

void agregar_en_tlb_fifo(int pid, int pagina, int marco) {
    // Prueba primero utilizando FIFO
    TLBEntry* tlb_entry_aux = malloc(sizeof(TLBEntry));
    tlb_entry_aux->marco = marco;
    tlb_entry_aux->pagina = pagina;
    tlb_entry_aux->pid = pid;
    tlb_entry_aux->last_access = NULL;

    
    if (list_size(tlb->entradas) < cant_ent_tlb) {
        list_add(tlb->entradas, tlb_entry_aux);
    } else {
        //Aca empleo el algoritmo de fifo
        list_remove_and_destroy_element(tlb->entradas, 0, free);
        list_add(tlb->entradas, tlb_entry_aux);
    }
}

void agregar_en_tlb_lru(int pid, int pagina, int marco) {
    TLBEntry* tlb_entry_aux = malloc(sizeof(TLBEntry));
    tlb_entry_aux->marco = marco;
    tlb_entry_aux->pagina = pagina;
    tlb_entry_aux->pid = pid;
    tlb_entry_aux->last_access = temporal_create(); // Inicializa el tiempo de acceso

    if (list_size(tlb->entradas) < cant_ent_tlb) {
        list_add(tlb->entradas, tlb_entry_aux);
    } else {
        // Algoritmo LRU
        int lru_index = 0;
        int64_t min_time = temporal_gettime(((TLBEntry*) list_get(tlb->entradas, 0))->last_access);

        for (int i = 1; i < list_size(tlb->entradas); i++) {
            int64_t entry_time = temporal_gettime(((TLBEntry*) list_get(tlb->entradas, i))->last_access);
            if (entry_time < min_time) {
                min_time = entry_time;
                lru_index = i;
            }
        }

        // Elimina la entrada menos recientemente usada
        TLBEntry* lru_entry = list_remove(tlb->entradas, lru_index);
        temporal_destroy(lru_entry->last_access); // Libera el tiempo de acceso
        free(lru_entry);

        list_add(tlb->entradas, tlb_entry_aux);
    }
}

void agregar_en_tlb(int pid, int pagina, int marco) {
    !strcmp(algoritmo_tlb, "FIFO") ? agregar_en_tlb_fifo(pid, pagina, marco) : agregar_en_tlb_lru(pid, pagina, marco);
}

int chequear_en_tlb(int pid, int pagina) {
    bool es_pid_pag_aux(void* data){
        return es_pid_pag(pid, pagina, data);
    };

    TLBEntry* tlbentry = list_find(tlb->entradas, es_pid_pag_aux);

    if(tlbentry != NULL) {
        return tlbentry->marco;
    }

    return -1;
}

op_code determinar_op(char* interrupcion){
    if(!strcmp(interrupcion, "OUT OF MEMORY")){
        return OUT_OF_MEMORY;
    }else if(!strcmp(interrupcion, "-Interrupcion por usuario-")){
        return USER_INTERRUPTED;
    } else {
        return INTERRUPCION;
    }
}

DIRECCION_LOGICA obtener_pagina_y_offset(int direccion_logica){
    DIRECCION_LOGICA dirr;
    
    dirr.pagina = floor(direccion_logica / tam_pagina);
    dirr.offset = direccion_logica - (dirr.pagina * tam_pagina);

    return dirr;
}

t_config* iniciar_configuracion(){
    printf("1. Cargar configuracion para pruebas 1, 2, 4, 5\n");
    printf("2. Cargar configuracion para pruebas 3\n");
    printf("3. Cargar configuracion para pruebas 6\n");
    char* opcion_en_string = readline("Seleccione una opción: ");
    int opcion = atoi(opcion_en_string);
    free(opcion_en_string);

    switch (opcion)
        {
        case 1:
            log_info(logger_cpu, "Se cargo la configuracion 1 2 4 5 correctamente");
            return iniciar_config("../cpu/configs/prueba_1_2_4_5.config");
        case 2:
            log_info(logger_cpu, "Se cargo la configuracion 3 correctamente");
            return iniciar_config("../cpu/configs/prueba_3.config");
        case 3:
            log_info(logger_cpu, "Se cargo la configuracion 6 correctamente");
            return iniciar_config("../cpu/configs/prueba_6.config");
        default:
            log_info(logger_cpu, "Se cargo la configuracion 1 2 4 5 correctamente");
            return iniciar_config("../cpu/configs/prueba_1_2_4_5.config");
        }
}
