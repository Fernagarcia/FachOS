#include <stdio.h>
#include <kernel.h>

int conexion_memoria;
int conexion_cpu_dispatch;
int conexion_cpu_interrupt;
int quantum_krn;
int grado_multiprogramacion;
int procesos_en_ram;
int idProceso=0;
int pid;

t_queue* cola_new;
t_queue* cola_ready;
t_queue* cola_running;
t_queue* cola_blocked;
t_queue* cola_exit;


t_log* logger_kernel;
t_config* config_kernel;

pthread_t planificacion;
sem_t sem_planif;  // Se va a encargar de la ejecucion y pausa de la planificacion

void* FIFO(){
    while(1){
        sem_wait(&sem_planif);
        if(queue_is_empty(cola_running)){
            pcb* a_ejecutar = queue_peek(cola_ready);

            cambiar_de_ready_a_execute(a_ejecutar);

            // Enviamos mensaje para mandarle el path que debe abrir
            log_info(logger_kernel, "\n-INFO PROCESO EN EJECUCION-\nPID: %d\nQUANTUM: %d\nPATH: %s\nEST. ACTUAL: %s\n", a_ejecutar->PID, a_ejecutar->quantum, a_ejecutar->path_instrucciones, a_ejecutar->estadoActual);
            paqueteDeMensajes(conexion_memoria, a_ejecutar->path_instrucciones, PATH); 



            // Enviamos el pcb a CPU
            paqueteDePCB(conexion_cpu_dispatch, a_ejecutar);

            if(procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_blocked)){
                cambiar_de_blocked_a_ready(queue_peek(cola_blocked));
            }else if(procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_new)){
                cambiar_de_new_a_ready(queue_peek(cola_new));
            }
            
            // Recibimos el contexto denuevo del CPU

            t_list* lista_de_contexto;
            lista_de_contexto = recibir_paquete(conexion_cpu_dispatch, logger_kernel);
            a_ejecutar->contexto = list_get(lista_de_contexto, 0);

            log_info(logger_kernel, "PC del PCB: %d", a_ejecutar->contexto->PC);

            cambiar_de_execute_a_blocked(a_ejecutar);
        }
        sem_post(&sem_planif);
    }
}
/*    void RR(pcb proceso){
        paquetePCB(queue_pop(colaReady)->contextoDeEjecucion);
        if(proceso.quantum=NULL){//ni idea cual seria el tiempo de ejecucuion o rafaga que deberia comparar

        }
    }*/

// TODO se podria hacer mas simple pero es para salir del paso <3 (por ejemplo que directamente se pase la funcion)

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
    quantum_krn = config_get_int_value(config_kernel, "QUANTUM");
    ip_memoria = config_get_string_value(config_kernel, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config_kernel, "PUERTO_MEMORIA");
    grado_multiprogramacion = config_get_int_value(config_kernel, "GRADO_MULTIPROGRAMACION");

    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE CPU", ip_cpu, puerto_cpu_dispatch);
    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);

    int server_kernel = iniciar_servidor(logger_kernel, puerto_escucha);
    log_info(logger_kernel, "Servidor listo para recibir al cliente");
    
    //CONEXIONES
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_operacion("KERNEL LLEGO A LA CASA MAMIIII", conexion_memoria, MENSAJE);
    conexion_cpu_dispatch = crear_conexion(ip_cpu, puerto_cpu_dispatch);
    enviar_operacion("KERNEL LLEGO A LA CASA MAMIIII", conexion_cpu_dispatch, MENSAJE);
    conexion_cpu_interrupt = crear_conexion(ip_cpu, puerto_cpu_interrupt);
    enviar_operacion("KERNEL LLEGO A LA CASA MAMIIII", conexion_cpu_interrupt, MENSAJE);
	int cliente_fd = esperar_cliente(server_kernel, logger_kernel);

    log_info(logger_kernel, "Conexiones con modulos establecidas");

    ArgsGestionarServidor args_sv = {logger_kernel, cliente_fd};
    pthread_create(&id_hilo[0], NULL, gestionar_llegada, (void*)&args_sv);

    sleep(2);
    
    pthread_create(&id_hilo[1], NULL, leer_consola, NULL);

    for(i = 0; i<3; i++){
        pthread_join(id_hilo[i], NULL);
    }
    pthread_join(planificacion, NULL);
    terminar_programa(logger_kernel, config_kernel);
    liberar_conexion(conexion_cpu_interrupt);
    liberar_conexion(conexion_cpu_dispatch);
    liberar_conexion(conexion_memoria);

    return 0;
}

//TODO Desarrollar las funciones 

int ejecutar_script(char* path_inst_kernel){
    char comando[130];

    FILE *f = fopen(path_inst_kernel, "rb");

    if (f == NULL) {
        log_info(logger_kernel, "No se pudo abrir el archivo de %s\n", path_inst_kernel);
        return 1;
    }

    while(!feof(f)){
        char* comando_a_ejecutar = fgets(comando, sizeof(comando), f);
        execute_line(comando_a_ejecutar, logger_kernel);
    }

    fclose(f);
    return 0;
}

int iniciar_proceso(char* path){
    pcb* pcb_nuevo = malloc(sizeof(pcb));
    pcb_nuevo->PID = idProceso;
    pcb_nuevo->quantum = quantum_krn;
    pcb_nuevo->path_instrucciones = strdup(path);
    pcb_nuevo->estadoActual = "NEW";
    pcb_nuevo->contexto = malloc(sizeof(regCPU));
    pcb_nuevo->contexto->PC = 0;
    
    queue_push(cola_new, pcb_nuevo);
    
    log_info(logger_kernel, "Se creo el proceso n° %d en NEW", pcb_nuevo->PID);
    
    if(procesos_en_ram < grado_multiprogramacion){
        cambiar_de_new_a_ready(pcb_nuevo);
    }
    idProceso++;
    return 0;
}

int finalizar_proceso(char* PID){
    int pid = atoi(PID);

    if(buscar_pcb_en_cola(cola_new, pid) != NULL){
        cambiar_de_new_a_exit(buscar_pcb_en_cola(cola_new, pid));
    }else if(buscar_pcb_en_cola(cola_ready, pid) != NULL){
        cambiar_de_ready_a_exit(buscar_pcb_en_cola(cola_ready, pid));
    }else if(buscar_pcb_en_cola(cola_running, pid) != NULL){
        cambiar_de_execute_a_exit(buscar_pcb_en_cola(cola_running, pid));
    }else if(buscar_pcb_en_cola(cola_blocked, pid) != NULL){
        cambiar_de_blocked_a_exit(buscar_pcb_en_cola(cola_blocked, pid));
    }else if(buscar_pcb_en_cola(cola_exit, pid) == NULL){
        log_error(logger_kernel, "El PCB con PID n°%d no existe", pid);
        return EXIT_FAILURE;
    }

    //TODO: fijarse como pasarle el motivo de eliminacion del pcb

    if(procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_blocked)){
        cambiar_de_blocked_a_ready((pcb*)queue_peek(cola_blocked));
    }else if(procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_new)){
        cambiar_de_new_a_ready((pcb*)queue_peek(cola_new));
    }

    liberar_recursos(pid);
    log_info(logger_kernel, "Finaliza el proceso n°%d - Motivo: NO SE", pid);
    
    return 0;
}

int iniciar_planificacion(){
    sem_init(&sem_planif, 0, 1);
    pthread_create(&planificacion, NULL, FIFO, NULL);
    return 0;
}

int detener_planificacion(){
    sem_close(&sem_planif);
    return 0;
}

int multiprogramacion(char* multiprogramacion){
    if(procesos_en_ram < atoi(multiprogramacion)){
        grado_multiprogramacion = atoi(multiprogramacion);
        log_info(logger_kernel, "Se ha establecido el grado de multiprogramacion en %d", grado_multiprogramacion);
        config_set_value(config_kernel, "GRADO_MULTIPROGRAMACION", multiprogramacion);
    }else{
        log_error(logger_kernel, "Desaloje elementos de la cola antes de cambiar el grado de multiprogramacion");
    }
    return 0;
}

int proceso_estado(){
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
    return 0;
}

void iterar_cola_e_imprimir(t_queue* cola) {
    t_list_iterator* lista_a_iterar = list_iterator_create(cola->elements);
    printf("%d\n", list_size(cola->elements));
    
    if (lista_a_iterar != NULL) { // Verificar que el iterador se haya creado correctamente
        printf("\t PIDs : [ ");
        while (list_iterator_has_next(lista_a_iterar)) {
            pcb* elemento_actual = list_iterator_next(lista_a_iterar); // Convertir el puntero genérico a pcb*
            
            if(list_iterator_has_next(lista_a_iterar)){
                printf("%d <- ", elemento_actual->PID);
            }else{
                printf("%d", elemento_actual->PID);
            }
        }
        printf(" ]\n");
    }
    list_iterator_destroy(lista_a_iterar);
}

// FUNCIONES DE BUSCAR Y ELIMINAR

pcb* buscar_pcb_en_cola(t_queue* cola, int PID){
    pcb* elemento_a_encontrar;
    pid = PID;

    bool es_igual_a_aux(void* data) {
        return es_igual_a(PID, data);
    };

    if(!list_is_empty(cola->elements)){
        elemento_a_encontrar = list_find(cola->elements, es_igual_a_aux);
        return elemento_a_encontrar;
    }else{
        return NULL;
    }
}

int liberar_recursos(int PID){
    pid = PID;

    bool es_igual_a_aux(void* data) {
        return es_igual_a(PID, data);
    };

    list_remove_and_destroy_by_condition(cola_exit->elements, es_igual_a_aux, destruir_pcb);
    
    return EXIT_SUCCESS; // Devolver adecuadamente el resultado de la operación  
}  

bool es_igual_a(int PID, void* data){
    pcb* elemento = (pcb*) data;
    return (elemento->PID == pid);
}

void destruir_pcb(void* data){
    pcb* elemento = (pcb*) data;
    free(elemento->path_instrucciones);
    free(elemento->contexto);
    free(elemento);
}

// CAMBIAR DE COLA

void cambiar_de_new_a_ready(pcb* pcb){
    queue_push(cola_ready, (void*)pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "NEW"; 
    queue_pop(cola_new);   
    log_info(logger_kernel, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = queue_size(cola_ready) + queue_size(cola_blocked) + queue_size(cola_running);
}

void cambiar_de_ready_a_execute(pcb* pcb){
    queue_push(cola_running, (void*)pcb); 
    pcb->estadoActual = "EXECUTE";
    pcb->estadoAnterior = "READY"; 
    queue_pop(cola_ready);   
    log_info(logger_kernel, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_execute_a_blocked(pcb* pcb){
    queue_push(cola_blocked, (void*)pcb); 
    pcb->estadoActual = "BLOCKED";
    pcb->estadoAnterior = "EXECUTE"; 
    queue_pop(cola_running);   
    log_info(logger_kernel, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_blocked_a_ready(pcb* pcb){
    queue_push(cola_ready, (void*)pcb); 
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "BLOCKED"; 
    queue_pop(cola_blocked);   
    log_info(logger_kernel, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->PID, pcb->estadoAnterior, pcb->estadoActual);
}

// PARA ELIMINACION DE PROCESOS

void cambiar_de_execute_a_exit(pcb* pcb){
    queue_push(cola_exit, (void*)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "EXECUTE"; 
    queue_pop(cola_running);  
    log_info(logger_kernel, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = queue_size(cola_ready) + queue_size(cola_blocked) + queue_size(cola_running);
}

void cambiar_de_ready_a_exit(pcb* pcb){
    queue_push(cola_exit, (void*)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "READY"; 
    list_remove_element(cola_ready->elements, (void*)pcb);    
    log_info(logger_kernel, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = queue_size(cola_ready) + queue_size(cola_blocked) + queue_size(cola_running);
}

void cambiar_de_blocked_a_exit(pcb* pcb){
    queue_push(cola_exit, (void*)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "BLOCKED"; 
    list_remove_element(cola_blocked->elements, (void*)pcb);      
    log_info(logger_kernel, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = queue_size(cola_ready) + queue_size(cola_blocked) + queue_size(cola_running);
}

void cambiar_de_new_a_exit(pcb* pcb){
    queue_push(cola_exit, (void*)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "NEW"; 
    list_remove_element(cola_new->elements, (void*)pcb);     
    log_info(logger_kernel, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = queue_size(cola_ready) + queue_size(cola_blocked) + queue_size(cola_running);
}