#include <kernel.h>

int conexion_memoria;
int conexion_cpu_dispatch;
int conexion_cpu_interrupt;
int grado_multiprogramacion;
int idProceso=0;
int pid;

t_queue* cola_new;
t_queue* cola_ready;
t_queue* cola_running;
t_queue* cola_blocked;
t_queue* cola_exit;

t_log* logger_kernel;
t_config* config_kernel;


char tipo[4]="FIFO";//FIFO O RR

void FIFO(){
    paqueteDePCB(conexion_cpu_dispatch,queue_peek(cola_ready));
    queue_push(cola_running,(void*) queue_peek(cola_ready));
    queue_pop(cola_ready);
            
}
/*    void RR(pcb proceso){
        paquetePCB(queue_pop(colaReady)->contextoDeEjecucion);
        if(proceso.quantum=NULL){//ni idea cual seria el tiempo de ejecucuion o rafaga que deberia comparar

        }
    }*/

// TODO se podria hacer mas simple pero es para salir del paso <3 (por ejemplo que directamente se pase la funcion)
void planificadorCortoPlazo(){
    if(strcmp(tipo, "FIFO")){
        FIFO();
    }
        /*else if(tipo=="RR"){
            RR(proceso,quantum);
        }*/
}



void* leer_consola(){
    log_info(logger_kernel, "CONSOLA INTERACTIVA DE KERNEL\n Ingrese comando a ejecutar...");

    char* leido, *s;

	while (1)
	{
		leido = readline("> ");

        if (strncmp(leido, "EXIT", 4) == 0) {
            free(leido);
            log_info(logger_kernel, "GRACIAS, VUELVA PRONTOS\n");
            break;
        }else{
            s = stripwhite(leido);
            if(*s)
            {
                add_history(s);
                execute_line(s, logger_kernel);
            }
            log_info(logger_kernel, leido);
            free (leido);
        }	
	}
}

int main(int argc, char* argv[]) {
    int i;

    cola_new=queue_create();
    cola_ready=queue_create();
    cola_running=queue_create();
    cola_blocked=queue_create();
    cola_exit=queue_create();

    char* ip_cpu, *ip_memoria;
    char* puerto_cpu_dispatch, *puerto_cpu_interrupt, *puerto_memoria;

    char* path_config = "../kernel/kernel.config";
    char* puerto_escucha;

    pthread_t id_hilo[2];

    // CREAMOS LOG Y CONFIG
    logger_kernel = iniciar_logger("kernel.log", "kernel-log", LOG_LEVEL_INFO);
    log_info(logger_kernel, "Logger Creado.");

    config_kernel = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config_kernel, "PUERTO_ESCUCHA");
    ip_cpu = config_get_string_value(config_kernel, "IP_CPU");
    puerto_cpu_dispatch = config_get_string_value(config_kernel, "PUERTO_CPU_DISPATCH");
    puerto_cpu_interrupt = config_get_string_value(config_kernel, "PUERTO_CPU_INTERRUPT");
    ip_memoria = config_get_string_value(config_kernel, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config_kernel, "PUERTO_MEMORIA");
    grado_multiprogramacion = config_get_int_value(config_kernel, "GRADO_MULTIPROGRAMACION");

    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE CPU", ip_cpu, puerto_cpu_dispatch);
    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);

    int server_kernel = iniciar_servidor(logger_kernel, puerto_escucha);
    log_info(logger_kernel, "Servidor listo para recibir al cliente");
    
    //CONEXIONES
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_mensaje("KERNEL LLEGO A LA CASA MAMIIII", conexion_memoria);
    conexion_cpu_dispatch = crear_conexion(ip_cpu, puerto_cpu_dispatch);
    enviar_mensaje("KERNEL LLEGO A LA CASA MAMIIII", conexion_cpu_dispatch);
    conexion_cpu_interrupt = crear_conexion(ip_cpu, puerto_cpu_interrupt);
    enviar_mensaje("KERNEL LLEGO A LA CASA MAMIIII", conexion_cpu_interrupt);
	int cliente_fd = esperar_cliente(server_kernel, logger_kernel);

    log_info(logger_kernel, "Conexiones con modulos establecidas");

    ArgsGestionarServidor args_sv = {logger_kernel, cliente_fd};
    pthread_create(&id_hilo[0], NULL, gestionar_llegada, (void*)&args_sv);
    
    pthread_create(&id_hilo[1], NULL, leer_consola, NULL);

    for(i = 0; i<3; i++){
        pthread_join(id_hilo[i], NULL);
    }

    terminar_programa(logger_kernel, config_kernel);
    liberar_conexion(conexion_cpu_interrupt);
    liberar_conexion(conexion_cpu_dispatch);
    liberar_conexion(conexion_memoria);

    return 0;
}

//TODO Desarrollar las funciones 

int ejecutar_script(char* param){
    printf("%s\n", param);
    return 0;
}
void iniciar_proceso(char* path_instrucciones){
    //enviar_mensaje(path_instrucciones, conexion_memoria);

    pcb* pcb_nuevo = malloc(sizeof(pcb));
    pcb_nuevo->PID = idProceso;
    pcb_nuevo->contexto.PID = idProceso;
    pcb_nuevo->estadoActual = "NEW";
    pcb_nuevo->contexto.registro.PC = 0;
    
    queue_push(cola_new,(void*)pcb_nuevo);
    
    log_info(logger_kernel,"Se creo el proceso n° %d en NEW", pcb_nuevo->PID);
    
    if(queue_size(cola_ready) < grado_multiprogramacion){
        pcb_nuevo->estadoActual = "READY";
        pcb_nuevo->estadoAnterior = "NEW";
        cambiar_pcb_de_cola(cola_new, cola_ready, pcb_nuevo);
        log_info(logger_kernel, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb_nuevo->PID, pcb_nuevo->estadoAnterior, pcb_nuevo->estadoActual);
    }
    idProceso++;
}

int finalizar_proceso(char* PID){
    int pid = atoi(PID);
    
    buscar_y_borrar_pcb_en_cola(cola_new, pid);
    buscar_y_borrar_pcb_en_cola(cola_ready, pid);
    buscar_y_borrar_pcb_en_cola(cola_running, pid);
    buscar_y_borrar_pcb_en_cola(cola_blocked, pid);
    buscar_y_borrar_pcb_en_cola(cola_exit, pid);

    return EXIT_SUCCESS;
}

int iniciar_planificacion(){
    printf("Hola mundo");
    return 0;
}

int detener_planificacion(){
    printf("Hola mundo");
    return 0;
}

void multiprogramacion(char* multiprogramacion){
    if(queue_size(cola_ready) < atoi(multiprogramacion)-1){
        grado_multiprogramacion = atoi(multiprogramacion);
        log_info(logger_kernel, "Se ha establecido el grado de multiprogramacion en %d", grado_multiprogramacion);
        config_set_value(config_kernel, "GRADO_MULTIPROGRAMACION", multiprogramacion);
    }else{
        log_error(logger_kernel, "Desaloje elementos de la cola antes de cambiar el grado de multiprogramacion");
    }
}

void proceso_estado(){
    printf("Procesos en NEW:\t");
    iterar_cola_e_imprimir(cola_new);
    printf("Procesos en READY:\t");
    iterar_cola_e_imprimir(cola_ready);
    printf("Procesos en EXECUTE:\t");
    iterar_cola_e_imprimir(cola_running);
    printf("Procesos en BLOCKED:\t");
    iterar_cola_e_imprimir(cola_blocked);
    printf("Procesos en EXIT:\t");
    iterar_cola_e_imprimir(cola_exit);
}

void iterar_cola_e_imprimir(t_queue* cola) {
    t_list_iterator* lista_a_iterar = list_iterator_create(cola->elements);
    printf("%d\n", list_size(cola->elements));
    
    if (lista_a_iterar != NULL) { // Verificar que el iterador se haya creado correctamente
        while (list_iterator_has_next(lista_a_iterar)) {
            pcb* elemento_actual = list_iterator_next(lista_a_iterar); // Convertir el puntero genérico a pcb*
            printf("\tPID: %d\n", elemento_actual->PID);
        }
    }
    list_iterator_destroy(lista_a_iterar);
}

void cambiar_pcb_de_cola(t_queue* cola_actual, t_queue* nueva_cola, pcb* pcb){
    queue_push(cola_ready, (void*)pcb); //SOLO CASTEAR A VOID* EL PCB, NO PASARLO COMO DIRECCION DE MEMORIA
    queue_pop(cola_new);    
}

int buscar_y_borrar_pcb_en_cola(t_queue* cola, int PID){
    pcb* elemento_a_borrar;
    pid = PID;

    bool es_igual_a_aux(void* data) {
        return es_igual_a(PID, data);
    };

    if (!list_is_empty(cola->elements)) {
        list_remove_and_destroy_by_condition(cola->elements, 
        es_igual_a_aux,
        destruir_pcb);
        if(elemento_a_borrar != NULL){
            return EXIT_FAILURE;
        }else{
            log_info(logger_kernel, "Finaliza el proceso n°%d - Motivo: le pego dura la falopa", PID);
            return EXIT_SUCCESS;
        }
    }
    return 0;  
}   

bool es_igual_a(void *data){
    pcb* elemento = (pcb*) data;
    return (elemento->PID == pid);
}

void destruir_pcb(void* data){
    pcb* elemento = (pcb*) data;
    free(elemento);
}
