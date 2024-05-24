#include <cpu.h>
#include <utils/parse.h>

int conexion_memoria;
int server_interrupt;
int server_dispatch;
int cliente_fd_dispatch;

char* instruccion_a_ejecutar;
char* interrupcion;

t_log* logger_cpu;
t_config* config;

cont_exec* contexto;

sem_t sem_ejecucion;

INSTRUCTION instructions[] = {
    { "SET", set, "Ejecutar SET" },
    { "MOV_IN", mov_in, "Ejecutar MOV_IN"},
    { "MOV_OUT", mov_out, "Ejecutar MOV_OUT"},
    { "SUM", sum, "Ejecutar SUM"},
    { "SUB", sub, "Ejecutar SUB"},
    { "JNZ", jnz, "Ejecutar JNZ"},
    { "MOV", mov, "Ejecutar MOV"},
    { "RESIZE", resize, "Ejecutar RESIZE"},
    { "COPY_STRING", copy_string, "Ejecutar COPY_STRING"},
    { "WAIT", wait, "Ejecutar WAIT"},
    { "SIGNAL", SIGNAL, "Ejecutar SIGNAL"},
    { "IO_GEN_SLEEP", io_gen_sleep, "Ejecutar IO_GEN_SLEEP"},
    { "IO_STDIN_READ", io_stdin_read, "Ejecutar IO_STDIN_READ"},
    { "EXIT" , EXIT, "Syscall, devuelve el contexto a kernel" },
    { NULL, NULL, NULL }
};

int main(int argc, char* argv[]) {   
    int i;
    char* config_path = "../cpu/cpu.config";
    
    logger_cpu = iniciar_logger("../cpu/cpu.log", "cpu-log", LOG_LEVEL_INFO);
    log_info(logger_cpu, "logger para CPU creado exitosamente.");

    config = iniciar_config(config_path);

    pthread_t hilo_id[4];

    // Get info from cpu.config
    
    char* ip_memoria = config_get_string_value(config,"IP_MEMORIA");
    char* puerto_memoria = config_get_string_value(config,"PUERTO_MEMORIA");
    char* puerto_dispatch = config_get_string_value(config,"PUERTO_ESCUCHA_DISPATCH");
    char* puerto_interrupt = config_get_string_value(config,"PUERTO_ESCUCHA_INTERRUPT");
    
    char* cant_ent_tlb = config_get_string_value(config,"CANTIDAD_ENTRADAS_TLB");
    char* algoritmo_tlb = config_get_string_value(config,"ALGORITMO_TLB");
    
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

    ArgsGestionarServidor args_dispatch = {logger_cpu, cliente_fd_dispatch};
    ArgsGestionarServidor args_interrupt = {logger_cpu, cliente_fd_interrupt};
    ArgsGestionarServidor args_memoria = {logger_cpu, conexion_memoria};

    pthread_create(&hilo_id[0], NULL, gestionar_llegada_cpu, &args_dispatch);
    pthread_create(&hilo_id[1], NULL, gestionar_llegada_cpu, &args_interrupt);
    pthread_create(&hilo_id[2], NULL, gestionar_llegada_cpu, &args_memoria);

    for(i = 0; i<5; i++){
        pthread_join(hilo_id[i], NULL);
    }
    
    liberar_conexion(conexion_memoria);
    terminar_programa(logger_cpu, config);
    
    return 0;
}

void Execute(RESPONSE* response, cont_exec* contexto) {
    if (response != NULL) {
        for(int i = 0; instructions[i].command != NULL; i++) {
            if (strcmp(instructions[i].command, response->command) == 0) {
                instructions[i].function(response->params, contexto);
                return; 
            }
        }
    }
}

RESPONSE* Decode(char* instruccion) {
    // Decode primero reconoce 
    RESPONSE* response;
    INSTRUCTION* instructions;
    response = parse_command(instruccion);

    printf("%s", response->command);

    if (response != NULL) {
        printf("COMMAND: %s\n", response->command);
        printf("PARAMS: \n");
        for(int i = 0; response->params[i] != NULL && i < response->params[i]; i++) {
            printf("Param[%d]: %s\n", i, response->params[i]);
        }
    }
    return response;
}

void Fetch(cont_exec* contexto) {

  paqueteDeMensajes(conexion_memoria, string_itoa(contexto->registros->PC), INSTRUCCION); // Enviamos instruccion para mandarle la instruccion que debe mandarnos

  log_info(logger_cpu, "Se solicito a memoria el paso de la instruccion n°%d", contexto->registros->PC);

}

void procesar_contexto(cont_exec* contexto){
  while(1){
    RESPONSE * response;
    Fetch(contexto);

    sem_wait(&sem_ejecucion);

    log_info(logger_cpu, "El decode recibio %s", instruccion_a_ejecutar);
    
    // Decoding instruction
    response = Decode(instruccion_a_ejecutar);
    // Executing instruction
    if(es_motivo_de_salida(response->command)){
      contexto->registros->PC++;
      Execute(response, contexto);
      break;
    }

    Execute(response, contexto);
    contexto->registros->PC++;

    // Checkeamos interrupcion

    // Para salvar por condicion de carrera.
    if(interrupcion != NULL) {
        if(check_interrupt(interrupcion)){
        log_info(logger_cpu, "Desalojando registro. MOTIVO: %s\n", interrupcion);
        enviar_contexto_pcb(cliente_fd_dispatch, contexto);
        break;
        }else{
        log_info(logger_cpu, "No hubo interrupciones, prosiguiendo con la ejecucion");
        }
    }
  }
}

void* gestionar_llegada_cpu(void* args){
	ArgsGestionarServidor* args_entrada = (ArgsGestionarServidor*)args;

  t_list* lista;
	while (1) {
		log_info(logger_cpu, "Esperando operacion...");
		int cod_op = recibir_operacion(args_entrada->cliente_fd);
		switch (cod_op) {
      case MENSAJE:
        recibir_mensaje(args_entrada->cliente_fd, logger_cpu, MENSAJE);
        break;
      case INTERRUPCION:
        lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
        interrupcion = list_get(lista, 0);
        printf("Hola soy el valor de interrupcion: %s", interrupcion);
        break;
      case INSTRUCCION:
        lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
        instruccion_a_ejecutar = list_get(lista, 0);
        log_info(logger_cpu, "Instruccion recibida de memoria: %s", instruccion_a_ejecutar);
        sem_post(&sem_ejecucion);
        break;
      case CONTEXTO:   // Se recibe el paquete del contexto del PCB
        lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
        if(!list_is_empty(lista)){
          log_info(logger_cpu, "Recibi un contexto de ejecución desde Kernel");
          contexto = list_get(lista, 0);
          contexto->registros = list_get(lista, 1);
          printf("%d", contexto->registros->PC);
          log_info(logger_cpu, "PC del CONTEXTO: %d", contexto->registros->PC);
          procesar_contexto(contexto);
        }
        break;
      case -1:
        log_error(logger_cpu, "el cliente se desconecto. Terminando servidor");
        return EXIT_FAILURE;
      default:
        log_warning(logger_cpu,"Operacion desconocida. No quieras meter la pata");
        break;
      }
	}
}

void iterator_cpu(t_log* logger_cpu, char* value){
	log_info(logger_cpu,"%s", value);
}

REGISTER* find_register(const char *name, cont_exec* contexto) {
    // Mapping
    REGISTER register_map[] = {
        {"PC", &contexto->registros->PC, "b"},
        {"AX", &contexto->registros->AX, "a"},
        {"BX", &contexto->registros->BX, "a"},
        {"CX", &contexto->registros->CX, "a"},
        {"DX", &contexto->registros->DX, "a"},
        {"EAX", &contexto->registros->EAX, "b"},
        {"EBX", &contexto->registros->EBX, "b"},
        {"ECX", &contexto->registros->ECX, "b"},
        {"EDX", &contexto->registros->EDX, "b"},
        {"SI", &contexto->registros->SI, "b"},
        {"DI", &contexto->registros->DI, "b"}
    };

    const int num_register = sizeof(register_map) / sizeof(REGISTER);

    for (int i = 0; i < num_register; ++i) {
        if (strcmp(register_map[i].name, name) == 0) {
            return &register_map[i];
        }
    }
    return NULL;
}

void set(char **params, cont_exec *contexto) {
  printf("Ejecutando instruccion set\n");
  printf("Me llegaron los parametros: %s, %s\n", params[0], params[1]);

    const char* register_name = params[0];
    int new_register_value = atoi(params[1]);


    REGISTER* found_register = find_register(register_name, contexto);
    if (found_register != NULL) {
        if (strcmp(found_register->type, "a")) {
            *(uint8_t*)found_register = new_register_value; // Si el registro es de tipo 'a'
        } else {
            *(uint32_t*)found_register = new_register_value; // Si el registro es de tipo 'b'
        }
        printf("Valor del registro %s actualizado a %d\n", register_name, new_register_value);
    } else {
        printf("Registro desconocido: %s\n", register_name);
    }
}

void sum(char **params, cont_exec *contexto) {
    printf("Ejecutando instruccion sum");
    printf("Me llegaron los registros: %s, %s\n", params[0], params[1]);

    REGISTER* register_origin = find_register(params[0], contexto);
    REGISTER* register_target = find_register(params[1], contexto);


    if (register_origin != NULL && register_target != NULL) {
        if (register_origin->type == register_target->type) {
            if (strcmp(register_origin->type, "a")) {
                *(uint8_t*)register_target->ptr += *(uint8_t*)register_origin->ptr;
            } else {
                *(uint32_t*)register_target->ptr += *(uint32_t*)register_origin->ptr;
            }
            printf("Suma realizada y almacenada en %s\n", params[1]);
        } else {
            printf("Los registros no son del mismo tipo\n");
        }
    } else {
        printf("Alguno de los registros no fue encontrado\n");
    }
}

void sub(char **params, cont_exec *contexto) {
    printf("Ejecutando instruccion sub");
    printf("Me llegaron los registros: %s, %s\n", params[0], params[1]);

    REGISTER* register_origin = find_register(params[0], contexto);
    REGISTER* register_target = find_register(params[1], contexto);


    if (register_origin != NULL && register_target != NULL) {
        if (register_origin->type == register_target->type) {
            if (strcmp(register_origin->type, "a")) {
                *(uint8_t*)register_target->ptr -= *(uint8_t*)register_origin->ptr;
            } else {
                *(uint32_t*)register_target->ptr -= *(uint32_t*)register_origin->ptr;
            }
            printf("Suma realizada y almacenada en %s\n", params[1]);
        } else {
            printf("Los registros no son del mismo tipo\n");
        }
    } else {
        printf("Alguno de los registros no fue encontrado\n");
    }
}

void jnz(char **params, cont_exec *contexto) {
    printf("Ejecutando instruccion set\n");
    printf("Me llegaron los parametros: %s\n", params[0]);

    const char* register_name = params[0];
    const int next_instruction = atoi(params[1]);

    REGISTER* found_register_name = find_register(register_name, contexto);

    if (found_register_name != NULL && found_register_name->ptr != NULL) {
        if (found_register_name->ptr != 0) {
            contexto->registros->PC = next_instruction;
        }
    } else {
        printf("Registro no encontrado o puntero nulo\n");
    }
}

    //TODO
void mov(char**, cont_exec* contexto){ 

}

void resize(char**, cont_exec* contexto){

}

void copy_string(char**, cont_exec* contexto){

}

void wait(char**, cont_exec* contexto){

}

void SIGNAL(char**, cont_exec* contexto){

}

void io_gen_sleep(char** params, cont_exec* contexto){
    char* interfaz= params[0];
    char** tiempo_a_esperar= &params[1];  // el & es para q le pase la direccion y pueda asignarlo como char**, y asi usarlo en solicitar_interfaz (gpt dijo)
    // enviar a kernel la peticion de la interfaz con el argumento especificado, capaz no hace falta extraer cada char* de params, sino enviar todo params
    solicitar_interfaz(interfaz, "IO_GEN_SLEEP", tiempo_a_esperar);
}

void io_stdin_read(char**, cont_exec* contexto){

}

void mov_in(char**, cont_exec* contexto){

}

void mov_out(char**, cont_exec* contexto){
    
}

void EXIT(char **params, cont_exec *contexto){
    log_info(logger_cpu, "Se finalizo la ejecucion de las instrucciones. Devolviendo contexto a Kernel...");
    enviar_contexto_pcb(cliente_fd_dispatch, contexto);
}

//TODO funcion que le manda a kernel una solicitud para una interfaz
void solicitar_interfaz(char* interfaz,char* solicitud,char** args){
  SOLICITUD_INTERFAZ* aux= malloc(sizeof(SOLICITUD_INTERFAZ));
  aux->nombre= interfaz;
  aux->solicitud= solicitud;
  aux->args= args;
  // Para enviar la solicitud a kernel la meto en un paquete y la mando x el dispatch
  paqueteIO(server_dispatch, aux);
}

int check_interrupt(char* interrupcion){
    return !strcmp(interrupcion, "Fin de Quantum");
}

const char *motivos_de_salida[9] = {"EXIT", "IO_GEN_SLEEP", "IO_STDIN_WRITE", "IO_STDOUT_READ", "IO_FS_CREATE", "IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE", "IO_FS_READ"};

bool es_motivo_de_salida(const char *command) {
    int num_commands = sizeof(motivos_de_salida) / sizeof(motivos_de_salida[0]);
    for (int i = 0; i < num_commands; i++) {
        if (strcmp(motivos_de_salida[i], command) == 0) {
            return true;
        }
    }
    return false;
}