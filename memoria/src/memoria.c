#include <memoria.h>
#include <unistd.h>

int cliente_fd_cpu;
int cliente_fd_kernel;
t_log* logger_memoria;
t_config* config_memoria; 
t_list* pseudocodigo;

sem_t instrucciones;

char* path;
char* path_instructions;

//TODO: Conseguir que se pase bien el path de las instrucciones del proceso

int enlistar_pseudocodigo(char* path_instructions, char* path, t_log* logger, t_list* pseudocodigo){
    char instruccion[50];
    char* linea_instruccion = string_new();
    char* full_path = string_new();

    strcat(full_path, path_instructions);
    strcat(full_path, path);

    pseudocodigo = list_create();

    FILE* f = fopen(full_path, "rb");

    if (f == NULL) {
        log_error(logger_memoria, "No se pudo abrir el archivo de %s (ERROR: %s)", full_path, strerror(errno));
        return EXIT_FAILURE;
    }

    while(!feof(f)){
        linea_instruccion = fgets(instruccion, sizeof(instruccion), f);
        char* inst_a_lista = strdup(linea_instruccion);
        log_info(logger_memoria, "INSTRUCCION: %s", linea_instruccion);
        list_add(pseudocodigo, inst_a_lista);
    }

    iterar_lista_e_imprimir(pseudocodigo);

    log_info(logger_memoria, "INSTRUCCIONES CARGADAS CORRECTAMENTE.\n");
    fclose(f); 

    return EXIT_SUCCESS;
}

void borrar_lista(t_list* lista){
    list_destroy_and_destroy_elements(lista, destruir_instrucciones);
}

void destruir_instrucciones(void* data){
    char* instruccion = (char*)data;
    free(instruccion);
}

void enviar_instrucciones_a_cpu(char* program_counter){
    sem_wait(&instrucciones);

    int pc = atoi(program_counter);

    if (!list_is_empty(pseudocodigo)) { // Verificar que el iterador se haya creado correctamente  
        char* instruccion = list_get(pseudocodigo, pc);
        log_info(logger_memoria, "Enviaste la instruccion n°%d: %s a CPU exitosamente", pc, instruccion);
        paqueteDeMensajes(cliente_fd_cpu, instruccion, INSTRUCCION);
    }
}

void iterar_lista_e_imprimir(t_list* lista) {
    t_list_iterator* lista_a_iterar = list_iterator_create(lista);
    if (lista_a_iterar != NULL) { // Verificar que el iterador se haya creado correctamente
        printf(" Lista de instrucciones : [ ");
        while (list_iterator_has_next(lista_a_iterar)) {
            char* elemento_actual = list_iterator_next(lista_a_iterar); // Convertir el puntero genérico a pcb*
            
            if(list_iterator_has_next(lista_a_iterar)){
                printf("%s <- ", elemento_actual);
            }else{
                printf("%s", elemento_actual);
            }
        }
        printf(" ]\tElementos totales: %d\n", list_size(lista));
    }
    list_iterator_destroy(lista_a_iterar);
}


int main(int argc, char* argv[]) {
    int i, server_memoria;
    
    char* path_config = "../memoria/memoria.config";
    char* puerto_escucha;

    sem_init(&instrucciones, 1, 0);

    // CREAMOS LOG Y CONFIG

    logger_memoria = iniciar_logger("memoria.log", "memoria-log", LOG_LEVEL_INFO);
    log_info(logger_memoria, "Logger Creado.");

    config_memoria = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config_memoria, "PUERTO_ESCUCHA");

     // path_instructions
    path_instructions = config_get_string_value(config_memoria, "PATH_INSTRUCCIONES");
    log_info(logger_memoria, "Utilizando el path the instrucciones: %s", path_instructions);

    sem_init(&instrucciones, 0, 0);

    pseudocodigo = list_create();

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
            log_info(logger_memoria, "Me solicitaron la instruccion n°%s", program_counter);
            sem_post(&instrucciones);
            enviar_instrucciones_a_cpu(program_counter);
            break;
        case PATH: 
            lista = recibir_paquete(args_entrada->cliente_fd, logger_memoria);
            char* path = list_get(lista, 0);
            log_info(logger_memoria, "PATH RECIBIDO: %s", path);
            if(!list_is_empty(pseudocodigo)){
                log_info(logger_memoria, "BORRANDO LISTA...\n");
                borrar_lista(pseudocodigo);
                enlistar_pseudocodigo(path_instructions, path, logger_memoria, pseudocodigo);
            }else{
                enlistar_pseudocodigo(path_instructions, path, logger_memoria, pseudocodigo);
            }
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


