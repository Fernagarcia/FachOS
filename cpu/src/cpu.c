#include <cpu.h>
#include <utils/parse.h>

int conexion_memoria;
int server_interrupt;
int server_dispatch;
int cliente_fd_dispatch;

char *instruccion_a_ejecutar;
char *interrupcion;

t_log *logger_cpu;
t_config *config;

cont_exec *contexto;

REGISTER register_map[11];
const int num_register = sizeof(register_map) / sizeof(REGISTER);

sem_t sem_ejecucion;
sem_t sem_interrupcion;

INSTRUCTION instructions[] = {
    {"SET", set, "Ejecutar SET"},
    {"MOV_IN", mov_in, "Ejecutar MOV_IN"},
    {"MOV_OUT", mov_out, "Ejecutar MOV_OUT"},
    {"SUM", sum, "Ejecutar SUM"},
    {"SUB", sub, "Ejecutar SUB"},
    {"JNZ", jnz, "Ejecutar JNZ"},
    {"MOV", mov, "Ejecutar MOV"},
    {"RESIZE", resize, "Ejecutar RESIZE"},
    {"COPY_STRING", copy_string, "Ejecutar COPY_STRING"},
    {"WAIT", wait, "Ejecutar WAIT"},
    {"SIGNAL", SIGNAL, "Ejecutar SIGNAL"},
    {"IO_GEN_SLEEP", io_gen_sleep, "Ejecutar IO_GEN_SLEEP"},
    {"IO_STDIN_READ", io_stdin_read, "Ejecutar IO_STDIN_READ"},
    {"EXIT", EXIT, "Syscall, devuelve el contexto a kernel"},
    {NULL, NULL, NULL}};

int main(int argc, char *argv[])
{
    int i;
    char *config_path = "../cpu/cpu.config";

    logger_cpu = iniciar_logger("../cpu/cpu.log", "cpu-log", LOG_LEVEL_INFO);
    log_info(logger_cpu, "logger para CPU creado exitosamente.");

    config = iniciar_config(config_path);

    pthread_t hilo_id[4];

    // Get info from cpu.config

    char *ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    char *puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    char *puerto_dispatch = config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH");
    char *puerto_interrupt = config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT");

    char *cant_ent_tlb = config_get_string_value(config, "CANTIDAD_ENTRADAS_TLB");
    char *algoritmo_tlb = config_get_string_value(config, "ALGORITMO_TLB");

    log_info(logger_cpu, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);

    // Abrir servidores

    server_dispatch = iniciar_servidor(logger_cpu, puerto_dispatch);
    log_info(logger_cpu, "Servidor dispatch abierto");
    server_interrupt = iniciar_servidor(logger_cpu, puerto_interrupt);
    log_info(logger_cpu, "Servidor interrupt abierto");

    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_operacion("CPU IS IN DA HOUSE", conexion_memoria, MENSAJE);

    cliente_fd_dispatch = esperar_cliente(server_dispatch, logger_cpu);
    int cliente_fd_interrupt = esperar_cliente(server_interrupt, logger_cpu);

    sem_init(&sem_ejecucion, 1, 0);
    sem_init(&sem_interrupcion, 1, 0);

    ArgsGestionarServidor args_dispatch = {logger_cpu, cliente_fd_dispatch};
    ArgsGestionarServidor args_interrupt = {logger_cpu, cliente_fd_interrupt};
    ArgsGestionarServidor args_memoria = {logger_cpu, conexion_memoria};

    pthread_create(&hilo_id[0], NULL, gestionar_llegada_cpu, &args_dispatch);
    pthread_create(&hilo_id[1], NULL, gestionar_llegada_cpu, &args_interrupt);
    pthread_create(&hilo_id[2], NULL, gestionar_llegada_cpu, &args_memoria);

    for (i = 0; i < 5; i++)
    {
        pthread_join(hilo_id[i], NULL);
    }

    sem_destroy(&sem_ejecucion);
    sem_destroy(&sem_interrupcion);
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
                return;
            }
        }
    }
}

RESPONSE *Decode(char *instruccion)
{
    // Decode primero reconoce
    RESPONSE *response;
    response = parse_command(instruccion);

    //printf("%s", response->command);

    /*if (response != NULL)
    {
        printf("COMMAND: %s\n", response->command);
        printf("PARAMS: \n");
        for (int i = 0; response->params[i] != NULL && i < response->params[i]; i++)
        {
            printf("Param[%d]: %s\n", i, response->params[i]);
        }
    }*/
    return response;
}

void Fetch(cont_exec *contexto)
{
    paqueteDeMensajes(conexion_memoria, string_itoa(contexto->registros->PC), INSTRUCCION); // Enviamos instruccion para mandarle la instruccion que debe mandarnos

    log_info(logger_cpu, "Se solicito a memoria el paso de la instruccion n°%d", contexto->registros->PC);
}

void procesar_contexto(cont_exec *contexto)
{
    while (1)
    {
        RESPONSE *response;
        Fetch(contexto);

        sem_wait(&sem_ejecucion);
        sem_trywait(&sem_interrupcion);

        log_info(logger_cpu, "El decode recibio %s", instruccion_a_ejecutar);

        response = Decode(instruccion_a_ejecutar);
        
        log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", contexto->PID, response->command, response->params[0], response->params[1]);

        if (es_motivo_de_salida(response->command))
        {
            contexto->registros->PC++;
            Execute(response, contexto);
            
            sem_trywait(&sem_interrupcion);
            break;
        }

        contexto->registros->PC++;
        Execute(response, contexto);

        if (sem_trywait(&sem_interrupcion) == 0)
        {
            log_info(logger_cpu, "Desalojando registro. MOTIVO: %s\n", interrupcion);
            enviar_contexto_pcb(cliente_fd_dispatch, contexto, INTERRUPCION);
            break;
        }
        else
        {
            log_info(logger_cpu, "No hubo interrupciones, prosiguiendo con la ejecucion");
        }
    }
}

void *gestionar_llegada_cpu(void *args)
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
            interrupcion = list_get(lista, 0);
            sem_post(&sem_interrupcion);
            break;
        case INSTRUCCION:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            instruccion_a_ejecutar = list_get(lista, 0);
            log_info(logger_cpu, "PID: %d - FETCH - Program Counter: %d", contexto->PID, contexto->registros->PC);
            sem_post(&sem_ejecucion);
            break;
        case CONTEXTO: // Se recibe el paquete del contexto del PCB
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            if (!list_is_empty(lista))
            {
                log_info(logger_cpu, "Recibi un contexto de ejecución desde Kernel");
                contexto = list_get(lista, 0);
                contexto->registros = list_get(lista, 1);
                log_info(logger_cpu, "PC del CONTEXTO: %d", contexto->registros->PC);
                procesar_contexto(contexto);
            }
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

void iterator_cpu(t_log *logger_cpu, char *value)
{
    log_info(logger_cpu, "%s", value);
}

// Función para inicializar el mapeo de registros
void upload_register_map()
{
    register_map[0] = (REGISTER){"PC", (uint32_t *)&contexto->registros->PC};
    register_map[1] = (REGISTER){"AX", (uint8_t *)&contexto->registros->AX};
    register_map[2] = (REGISTER){"BX", (uint8_t *)&contexto->registros->BX};
    register_map[3] = (REGISTER){"CX", (uint8_t *)&contexto->registros->CX};
    register_map[4] = (REGISTER){"DX", (uint8_t *)&contexto->registros->DX};
    register_map[5] = (REGISTER){"EAX", (uint32_t *)&contexto->registros->EAX};
    register_map[6] = (REGISTER){"EBX", (uint32_t *)&contexto->registros->EBX};
    register_map[7] = (REGISTER){"ECX", (uint32_t *)&contexto->registros->ECX};
    register_map[8] = (REGISTER){"EDX", (uint32_t *)&contexto->registros->EDX};
    register_map[9] = (REGISTER){"SI", (uint32_t *)&contexto->registros->SI};
    register_map[10] = (REGISTER){"DI", (uint32_t *)&contexto->registros->DI};
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
    printf("Ejecutando instruccion SET\n");
    printf("Me llegaron los parametros: %s, %s\n", params[0], params[1]);

    const char *register_name = params[0];
    int new_register_value = atoi(params[1]);

    REGISTER *found_register = find_register(register_name);
    if (found_register != NULL)
    {
        printf("ENCONTRE REGISTRO %s\n", found_register->name);
        *(int *)found_register->registro = new_register_value;
        printf("Valor del registro %s actualizado a %d\n", register_name, new_register_value);
    }
    else
    {
        printf("Registro desconocido: %s\n", register_name);
    }
}

void sum(char **params)
{
    printf("Ejecutando instruccion SUM\n");
    printf("Me llegaron los registros: %s, %s\n", params[0], params[1]);

    REGISTER *register_origin = find_register(params[0]);
    REGISTER *register_target = find_register(params[1]);

    if (register_origin != NULL && register_target != NULL)
    {
        if (register_origin->name == register_target->name)
        {
            *(int *)register_target->registro += *(int *)register_origin->registro;
            printf("Suma realizada y almacenada en %s\n", params[1]);
        }
        else
        {
            printf("Los registros no son del mismo tipo\n");
        }
    }
    else
    {
        printf("Alguno de los registros no fue encontrado\n");
    }
}

void sub(char **params)
{
    printf("Ejecutando instruccion SUB\n");
    printf("Me llegaron los registros: %s, %s\n", params[0], params[1]);

    REGISTER *register_origin = find_register(params[0]);
    REGISTER *register_target = find_register(params[1]);

    if (register_origin != NULL && register_target != NULL)
    {
        if (register_origin->name == register_target->name)
        {
            *(int *)register_target->registro -= *(int *)register_origin->registro;
            printf("Suma realizada y almacenada en %s\n", params[1]);
        }
        else
        {
            printf("Los registros no son del mismo tipo\n");
        }
    }
    else
    {
        printf("Alguno de los registros no fue encontrado\n");
    }
}

void jnz(char **params)
{
    printf("Ejecutando instruccion JNZ\n");
    printf("Me llegaron los parametros: %s\n", params[0]);

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
        printf("Registro no encontrado o puntero nulo\n");
    }
}

// TODO
void mov(char **)
{
}

void resize(char **)
{
}

void copy_string(char **)
{
}

void wait(char **)
{
}

void SIGNAL(char **)
{
}

void io_gen_sleep(char **params)
{
    printf("Ejecutando instruccion IO_GEN_SLEEP\n");
    printf("Me llegaron los parametros: %s, %s\n", params[0], params[1]);

    char *interfaz_name = params[0];
    char **tiempo_a_esperar = &params[1]; // el & es para q le pase la direccion y pueda asignarlo como char**, y asi usarlo en solicitar_interfaz (gpt dijo)
    // enviar a kernel la peticion de la interfaz con el argumento especificado, capaz no hace falta extraer cada char* de params, sino enviar todo params
    solicitar_interfaz(interfaz_name, "IO_GEN_SLEEP", tiempo_a_esperar);
}

void io_stdin_read(char **)
{
}

void mov_in(char **)
{
}

void mov_out(char **)
{
}

void EXIT(char **params)
{
    log_info(logger_cpu, "Se finalizo la ejecucion de las instrucciones. Devolviendo contexto a Kernel...\n");
    enviar_contexto_pcb(cliente_fd_dispatch, contexto, CONTEXTO);
}

void solicitar_interfaz(char *interfaz_name, char *solicitud, char **argumentos)
{
    SOLICITUD_INTERFAZ* aux = malloc(sizeof(SOLICITUD_INTERFAZ));
    aux->nombre = strdup(interfaz_name);
    aux->solicitud = strdup(solicitud);
    aux->args = malloc(sizeof(argumentos));

    int argumentos_a_copiar = sizeof(aux->args) / sizeof(aux->args[0]);  
    for (int i = 0; i < argumentos_a_copiar; i++)
    {
        aux->args[i] = strdup(argumentos[i]);
    }
    paqueteIO(cliente_fd_dispatch, aux, contexto);
}

int check_interrupt(char *interrupcion)
{
    return !strcmp(interrupcion, "Fin de Quantum");
}

const char *motivos_de_salida[9] = {"EXIT", "IO_GEN_SLEEP", "IO_STDIN_WRITE", "IO_STDOUT_READ", "IO_FS_CREATE", "IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE", "IO_FS_READ"};

bool es_motivo_de_salida(const char *command)
{
    int num_commands = sizeof(motivos_de_salida) / sizeof(motivos_de_salida[0]);
    for (int i = 0; i < num_commands; i++)
    {
        if (strcmp(motivos_de_salida[i], command) == 0)
        {
            return true;
        }
    }
    return false;
}