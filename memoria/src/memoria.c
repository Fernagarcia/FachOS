#include <memoria.h>

int cliente_fd_cpu;
int cliente_fd_kernel;
t_log* logger_memoria;
t_config* config_memoria; 
t_list* pseudocodigo;

sem_t instrucciones;

//TODO: Conseguir que se pase bien el path de las instrucciones del proceso

int enlistar_pseudocodigo(char* path, t_log* logger){
    pseudocodigo = list_create();

    char instruccion[50];

    FILE* f = fopen(path, "rb");

    if (f == NULL) {
        log_info(logger_memoria, "No se pudo abrir el archivo de %s\n", path);
        return EXIT_FAILURE;
    }

    while(!feof(f)){
        int i = 0;
        char* linea_instruccion = fgets(instruccion, sizeof(instruccion), f);
        log_info(logger_memoria, "INSTRUCCION n°%d: %s", i, linea_instruccion);
        list_add(pseudocodigo, linea_instruccion);
        i++;
    }

    log_info(logger_memoria, "INSTRUCCIONES CARGADAS CORRECTAMENTE.\n");
    
    fclose(f);

    return EXIT_SUCCESS;
}

void enviar_instrucciones_a_cpu(char* program_counter){
    int pc = atoi(program_counter);

    if (!list_is_empty(pseudocodigo)) { // Verificar que el iterador se haya creado correctamente  
        char* instruccion = list_get(pseudocodigo, pc);
        log_info(logger_memoria, "Enviaste la instruccion n°%d: %s a CPU exitosamente", pc, instruccion);
        paqueteDeMensajes(cliente_fd_cpu, instruccion, INSTRUCCION);
    }
}

int main(int argc, char* argv[]) {
    int i, server_memoria;
    
    char* path_config = "../memoria/memoria.config";
    char* puerto_escucha;

    // CREAMOS LOG Y CONFIG

    logger_memoria = iniciar_logger("memoria.log", "memoria-log", LOG_LEVEL_INFO);
    log_info(logger_memoria, "Logger Creado.");

    config_memoria = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config_memoria, "PUERTO_ESCUCHA");

    sem_init(&instrucciones, 0, 0);

    pthread_t hilo[3];
    server_memoria = iniciar_servidor(logger_memoria, puerto_escucha);
    log_info(logger_memoria, "Servidor a la espera de clientes");

    cliente_fd_cpu = esperar_cliente(server_memoria, logger_memoria);
    cliente_fd_kernel = esperar_cliente(server_memoria, logger_memoria);
    //int cliente_fd_tres = esperar_cliente(server_memoria, logger_memoria);
    
    ArgsGestionarServidor args_sv1 = {logger_memoria, cliente_fd_cpu};
    ArgsGestionarServidor args_sv2 = {logger_memoria, cliente_fd_kernel};
    //ArgsGestionarServidor args_sv3 = {logger_memoria, cliente_fd_tres};

    pthread_create(&hilo[0], NULL, gestionar_llegada_memoria, &args_sv1);
    pthread_create(&hilo[1], NULL, gestionar_llegada_memoria, &args_sv2);
    //pthread_create(&hilo[2], NULL, gestionar_llegada, &args_sv3);

    for(i = 0; i<3; i++){
        pthread_join(hilo[i], NULL);
    }    
   
    return 0;
}

void* gestionar_llegada_memoria(void* args){
	ArgsGestionarServidor* args_entrada = (ArgsGestionarServidor*)args;

	void iterator_adapter(void* a) {
		iterator(logger_memoria, (char*)a);
	};

	t_list* lista;
	while (1) {
		log_info(logger_memoria, "Esperando operacion...");
		int cod_op = recibir_operacion(args_entrada->cliente_fd);
		switch (cod_op) {
		case MENSAJE:
            char* mensaje = (char*)recibir_mensaje(args_entrada->cliente_fd, logger_memoria, MENSAJE);
            free(mensaje);
            break;
        case INSTRUCCION:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_memoria);
            char* program_counter = list_get(lista, 0);
            enviar_instrucciones_a_cpu(program_counter);
            break;
        case PATH: 
            lista = recibir_paquete(args_entrada->cliente_fd, logger_memoria);
            char* path_recibido = list_get(lista, 0);
            log_info(logger_memoria, "PATH RECIBIDO: %s", path_recibido);
            enlistar_pseudocodigo(path_recibido, logger_memoria);
            free(path_recibido);
			break;
		case PAQUETE:
			lista = recibir_paquete(args_entrada->cliente_fd, logger_memoria);
			log_info(logger_memoria, "Me llegaron los siguientes valores:\n");
			list_iterate(lista, iterator_adapter);
			break;
		case -1:
			log_error(logger_memoria, "el cliente se desconecto. Terminando servidor");
			return EXIT_FAILURE;
		default:
			log_warning(logger_memoria,"Operacion desconocida. No quieras meter la pata");
			break;
		}
	}
}

void iterator_memoria(void* a){
	log_info(logger_memoria,"%s", (char*)a);
}