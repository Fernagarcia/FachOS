#include <memoria.h>

int cliente_fd_cpu;
int cliente_fd_kernel;
t_log* logger_memoria;
t_config* config_memoria; 
t_list* pseudocodigo;

void encolarPseudocodigo(char* path, t_log* logger){
    pseudocodigo = list_create();

    char instruccion[50];

    FILE* f = fopen(path, "rb"); // Se recibe la ruta del archivo y se abre en memoria

    while(!feof(f)){
        char* linea_instruccion = fgets(instruccion, strlen(instruccion) + 1, f);
        list_add(pseudocodigo,linea_instruccion);
    }
    log_info(logger, "Se termino de leer el archivo de instrucciones");
    
    fclose(f);
}

void enviar_instrucciones_a_cpu(char* programCounter){
    int pc = atoi(programCounter);
    char* instruccion = list_get(pseudocodigo,pc);
    enviar_operacion(instruccion, cliente_fd_cpu, INSTRUCCION);
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
    /*while(1){
    
    }*/
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
            recibir_mensaje(args_entrada->cliente_fd, logger_memoria);
        case PATH:   
            char* path;
			path = recibir_instruccion(args_entrada->cliente_fd, logger_memoria);
            encolarPseudocodigo(path, logger_memoria);
			break;
        case INSTRUCCION:
            char* pc;
            pc = recibir_instruccion(args_entrada->cliente_fd, logger_memoria);
            enviar_instrucciones_a_cpu(pc);
		case PAQUETE:
			lista = recibir_paquete(args_entrada->cliente_fd);
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

void iterator_memoria(t_log* logger, char* value){
	log_info(logger,"%s", value);
}