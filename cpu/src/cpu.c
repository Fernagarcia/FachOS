#include <cpu.h>
#include <cpu_instructions.h>
#include <utils/parse.h>

int conexion_memoria;

char* instruccion_a_ejecutar;

t_log* logger_cpu;
t_config* config;

void Execute(RESPONSE* response, regCPU* registers) {
    if (response != NULL) {
        for(int i = 0; instructions[i].command != NULL; i++) {
            if (strcmp(instructions[i].command, response->command) == 0) {
                instructions[i].function(response->params, registers);
                return; 
            }
        }
    }
}

RESPONSE* Decode(char* instruccion) {
    // Decode primero reconoce 
    RESPONSE* response;
    INSTRUCTION* instructions;
    response = parse_command(instruccion, instructions);

    printf("%s", response->command);

    if (response != NULL) {
        printf("COMMAND: %s\n", response->command);
        printf("PARAMS: \n");
        for(int i = 0; i < response->params[i] != NULL; i++) {
            printf("Param[%d]: %s\n", i, response->params[i]);
        }
    }
    return response;
}

void Fetch(regCPU* registros) {

  paqueteDeMensajes(conexion_memoria, string_itoa(registros->PC), INSTRUCCION); // Enviamos instruccion para mandarle la instruccion que debe mandarnos

  log_info(logger_cpu, "Se solicito a memoria el paso de la instruccion n°%d", registros->PC);

}

void procesar_contexto(regCPU* registros){
    RESPONSE * response;
    Fetch(registros);

    sem_wait(&sem_ejecucion);

    log_info(logger_cpu, "El decode recibio %s", instruccion_a_ejecutar);
    // Decoding instruction
    response = Decode(instruccion_a_ejecutar);
    // Executing instruction
    //Execute(response, registros);
    registros->PC++;
}

int main(int argc, char* argv[]) {   
    int i;
    char* config_path = "../cpu/cpu.config";
    
    logger_cpu = iniciar_logger("../cpu/cpu.log", "cpu-log", LOG_LEVEL_INFO);
    log_info(logger_cpu, "logger para CPU creado exitosamente.");

    config = iniciar_config(config_path);

    // TEST DECODE
    char *instruction = "SET 24 30";
    RESPONSE* response;
    //response = Decode(instruction);
    
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
    
    int server_dispatch = iniciar_servidor(logger_cpu, puerto_dispatch);
    log_info(logger_cpu, "Servidor dispatch abierto");
    int server_interrupt = iniciar_servidor(logger_cpu, puerto_interrupt);
    log_info(logger_cpu, "Servidor interrupt abierto");
  
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_operacion("CPU IS IN DA HOUSE", conexion_memoria, MENSAJE);

    int cliente_fd_dispatch = esperar_cliente(server_dispatch, logger_cpu);
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
      case INSTRUCCION:
        lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
        instruccion_a_ejecutar = list_get(lista, 0);
        log_info(logger_cpu, "Instruccion recibida de memoria: %s", instruccion_a_ejecutar);
        sem_post(&sem_ejecucion);
        break;
      case PAQUETE:   // Se recibe el paquete del contexto del PCB
        regCPU* registros;
        lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
        if(!list_is_empty(lista)){
          log_info(logger_cpu, "Recibi un contexto de ejecución desde Kernel");
          registros = list_get(lista, 0);
          log_info(logger_cpu, "PC del CONTEXTO: %d", registros->PC);
          procesar_contexto(registros);
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
