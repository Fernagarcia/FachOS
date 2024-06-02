#include <stdio.h>
#include <kernel.h>

int conexion_memoria;
int conexion_cpu_dispatch;
int conexion_cpu_interrupt;
int quantum_krn;
int grado_multiprogramacion;
int procesos_en_ram;
int idProceso = 0;
int cliente_fd;
char* tipo_de_planificacion;

t_list *interfaces;

t_queue *cola_new;
t_queue *cola_ready;
t_queue *cola_ready_prioridad;
t_queue *cola_running;
t_queue *cola_blocked;
t_queue *cola_exit;

t_log *logger_kernel;
t_log *logger_kernel_planif;
t_log *logger_kernel_mov_colas;
t_config *config_kernel;

pcb* proceso_creado;
cont_exec *contexto_recibido;
SOLICITUD_INTERFAZ *interfaz_solicitada;

pthread_t planificacion;
pthread_t interrupcion;
sem_t sem_planif;
sem_t recep_contexto;
sem_t creacion_proceso;
sem_t finalizacion_proceso;


void *FIFO()
{
    while (queue_size(cola_ready) > 0)
    {
        sem_wait(&sem_planif);
        if (queue_is_empty(cola_running))
        {
            pcb *a_ejecutar = queue_peek(cola_ready);

            cambiar_de_ready_a_execute(a_ejecutar);

            // Enviamos mensaje para mandarle el path que debe abrir
            char *path_a_mandar = a_ejecutar->path_instrucciones;
            log_info(logger_kernel, "\n-INFO PROCESO EN EJECUCION-\nPID: %d\nPATH: %s\nEST. ACTUAL: %s\n", a_ejecutar->contexto->PID, a_ejecutar->path_instrucciones, a_ejecutar->estadoActual);
            paqueteDeMensajes(conexion_memoria, path_a_mandar, PATH);

            // Enviamos el pcb a CPU
            sleep(1);
            enviar_contexto_pcb(conexion_cpu_dispatch, a_ejecutar->contexto, CONTEXTO);

            // Recibimos el contexto denuevo del CPU

            sem_wait(&recep_contexto);

            a_ejecutar->contexto = contexto_recibido;

            log_info(logger_kernel_planif, "\n-PC del proceso ejecutado: %d-", a_ejecutar->contexto->registros->PC);

            switch (a_ejecutar->contexto->motivo)
            {
            case FIN_INSTRUCCION:
                cambiar_de_execute_a_exit(a_ejecutar);
                break;
            default:
                 if (lista_seek_interfaces(interfaz_solicitada->nombre))
                {
                    INTERFAZ *interfaz = interfaz_encontrada(interfaz_solicitada->nombre);
                    if (lista_validacion_interfaces(interfaz, interfaz_solicitada->solicitud))
                    {
                        log_info(logger_kernel_mov_colas, "Operacion correcta. Enseguida se realizara la petición.");
                        interfaz_solicitada->pid = string_itoa(a_ejecutar->contexto->PID);
                        cambiar_de_execute_a_blocked(a_ejecutar);
                        checkear_estado_interfaz(interfaz);
                    }
                    else
                    {
                        log_warning(logger_kernel_mov_colas, "Operación desconocida... Dirigiendose a la salida.");
                        cambiar_de_execute_a_exit(a_ejecutar);
                    }
                }
                else
                {
                    log_error(logger_kernel_mov_colas, "Interfaz sin conexión... Dirigiendose a la salida.");
                    cambiar_de_execute_a_exit(a_ejecutar);
                }
                break;
            }
        }
        sem_post(&sem_planif);
    }
    return NULL;
}

void *RR()
{
    while (queue_size(cola_ready) > 0)
    {
        sem_wait(&sem_planif);
        if (queue_is_empty(cola_running))
        {
            pcb *a_ejecutar = queue_peek(cola_ready);

            cambiar_de_ready_a_execute(a_ejecutar);

            // Enviamos mensaje para mandarle el path que debe abrir
            char *path_a_mandar = a_ejecutar->path_instrucciones;
            log_info(logger_kernel_planif, "\n-INFO PROCESO EN EJECUCION-\nPID: %d\nQUANTUM: %d\nPATH: %s\nEST. ACTUAL: %s\n", a_ejecutar->contexto->PID, a_ejecutar->contexto->quantum, a_ejecutar->path_instrucciones, a_ejecutar->estadoActual);
            paqueteDeMensajes(conexion_memoria, path_a_mandar, PATH);

            // Enviamos el pcb a CPU
            sleep(1);
            enviar_contexto_pcb(conexion_cpu_dispatch, a_ejecutar->contexto, CONTEXTO);

            // Esperamos a que pasen los segundos de quantum

            abrir_hilo_interrupcion(quantum_krn);            

            // Recibimos el contexto denuevo del CPU

            sem_wait(&recep_contexto);

            a_ejecutar->contexto = contexto_recibido;

            log_info(logger_kernel_planif, "\n-PC del proceso ejecutado: %d-", a_ejecutar->contexto->registros->PC);

            switch (a_ejecutar->contexto->motivo)
            {
            case FIN_INSTRUCCION:
                cambiar_de_execute_a_exit(a_ejecutar);
                break;
            case QUANTUM:
                log_info(logger_kernel_planif, "PID: %d - Desalojado por fin de quantum", a_ejecutar->contexto->PID);
                cambiar_de_execute_a_ready(a_ejecutar);
                break;
            default:
                if (lista_seek_interfaces(interfaz_solicitada->nombre))
                {
                    INTERFAZ *interfaz = interfaz_encontrada(interfaz_solicitada->nombre);
                    if (lista_validacion_interfaces(interfaz, interfaz_solicitada->solicitud))
                    {
                        log_info(logger_kernel_mov_colas, "Operacion correcta. Enseguida se realizara la petición.");
                        interfaz_solicitada->pid = string_itoa(a_ejecutar->contexto->PID);
                        cambiar_de_execute_a_blocked(a_ejecutar);
                        checkear_estado_interfaz(interfaz);                    }
                    else
                    {
                        log_warning(logger_kernel_mov_colas, "Operación desconocida... Dirigiendose a la salida.");
                        cambiar_de_execute_a_exit(a_ejecutar);
                    }
                }
                else
                {
                    log_error(logger_kernel_mov_colas, "Interfaz sin conexión... Dirigiendose a la salida.");
                    cambiar_de_execute_a_exit(a_ejecutar);
                }
                break;
            }
        }
        sem_post(&sem_planif);
    }
    return NULL;
}

void *VRR()
{
    //TODO

    while ((queue_size(cola_ready) + queue_size(cola_ready_prioridad)) > 0)
    {
        sem_wait(&sem_planif);
        if (queue_is_empty(cola_running))
        {
            pcb *a_ejecutar;

            if(!queue_is_empty(cola_ready_prioridad)){
                a_ejecutar = queue_peek(cola_ready_prioridad);
            }else{
                a_ejecutar = queue_peek(cola_ready);
            }
        
            cambiar_de_ready_a_execute(a_ejecutar);

            // Enviamos mensaje para mandarle el path que debe abrir
            char *path_a_mandar = a_ejecutar->path_instrucciones;
            log_info(logger_kernel_planif, "\n-INFO PROCESO EN EJECUCION-\nPID: %d\nQUANTUM: %d\nPATH: %s\nEST. ACTUAL: %s\n", a_ejecutar->contexto->PID, a_ejecutar->contexto->quantum, a_ejecutar->path_instrucciones, a_ejecutar->estadoActual);
            paqueteDeMensajes(conexion_memoria, path_a_mandar, PATH);

            // Enviamos el pcb a CPU
            sleep(1);
            enviar_contexto_pcb(conexion_cpu_dispatch, a_ejecutar->contexto, CONTEXTO);

            // Esperamos a que pasen los segundos de quantum

            abrir_hilo_interrupcion(a_ejecutar->contexto->quantum);            

            // Recibimos el contexto denuevo del CPU

            sem_wait(&recep_contexto);

            a_ejecutar->contexto = contexto_recibido;

            log_info(logger_kernel_planif, "\n-PC del proceso ejecutado: %d-\n-Quantum actual: %d\n", a_ejecutar->contexto->registros->PC, a_ejecutar->contexto->quantum);

            switch (a_ejecutar->contexto->motivo)
            {
            case FIN_INSTRUCCION:
                cambiar_de_execute_a_exit(a_ejecutar);
                break;
            case QUANTUM:
                log_info(logger_kernel_planif, "PID: %d - Desalojado por fin de quantum", a_ejecutar->contexto->PID);
                cambiar_de_execute_a_ready(a_ejecutar);
                break;
            default:
                if (lista_seek_interfaces(interfaz_solicitada->nombre))
                {
                    INTERFAZ *interfaz = interfaz_encontrada(interfaz_solicitada->nombre);
                    if (lista_validacion_interfaces(interfaz, interfaz_solicitada->solicitud))
                    {
                        log_info(logger_kernel_mov_colas, "Operacion correcta. Enseguida se realizara la petición.");
                        interfaz_solicitada->pid = string_itoa(a_ejecutar->contexto->PID);
                        cambiar_de_execute_a_blocked(a_ejecutar);
                        checkear_estado_interfaz(interfaz);
                    }
                    else
                    {
                        log_warning(logger_kernel_mov_colas, "Operación desconocida... Dirigiendose a la salida.");
                        cambiar_de_execute_a_exit(a_ejecutar);
                    }
                }
                else
                {
                    log_error(logger_kernel_mov_colas, "Interfaz sin conexión... Dirigiendose a la salida.");
                    cambiar_de_execute_a_exit(a_ejecutar);
                }
                break;
            }
        }
        sem_post(&sem_planif);
    }
    return NULL;
}

void *leer_consola()
{
    log_info(logger_kernel, "CONSOLA INTERACTIVA DE KERNEL\n Ingrese comando a ejecutar...");
    char *leido, *s;
    while (1)
    {
        leido = readline("> ");

        if (strncmp(leido, "EXIT", 4) == 0)
        {
            free(leido);
            log_info(logger_kernel, "GRACIAS, VUELVA PRONTOS\n");
            break;
        }
        else
        {
            s = stripwhite(leido);
            if (*s)
            {
                add_history(s);
                execute_line(s, logger_kernel);
            }
            free(leido);
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int i;

    cola_new = queue_create();
    cola_ready = queue_create();
    cola_ready_prioridad = queue_create();
    cola_running = queue_create();
    cola_blocked = queue_create();
    cola_exit = queue_create();

    interfaces = list_create();

    char *ip_cpu, *ip_memoria;
    char *puerto_cpu_dispatch, *puerto_cpu_interrupt, *puerto_memoria;

    char *path_config = "../kernel/kernel.config";
    char *puerto_escucha;

    pthread_t id_hilo[3];
    
    sem_init(&sem_planif, 1, 0);
    sem_init(&recep_contexto, 1, 0);
    sem_init(&creacion_proceso, 1, 0);
    sem_init(&finalizacion_proceso, 1, 0);

    // CREAMOS LOG Y CONFIG
    logger_kernel = iniciar_logger("kernel.log", "kernel-log", LOG_LEVEL_INFO);
    logger_kernel_mov_colas = iniciar_logger("kernel_colas.log", "kernel_colas-log", LOG_LEVEL_INFO);
    logger_kernel_planif = iniciar_logger("kernel_planif.log", "kernel_planificacion-log", LOG_LEVEL_INFO);
    log_info(logger_kernel, "\n \t\t\t-INICIO LOGGER GENERAL- \n");
    log_info(logger_kernel_planif, "\n \t\t\t-INICIO LOGGER DE PLANIFICACION- \n");
    log_info(logger_kernel_mov_colas, "\n \t\t\t-INICIO LOGGER DE PROCESOS- \n");

    config_kernel = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config_kernel, "PUERTO_ESCUCHA");
    ip_cpu = config_get_string_value(config_kernel, "IP_CPU");
    puerto_cpu_dispatch = config_get_string_value(config_kernel, "PUERTO_CPU_DISPATCH");
    puerto_cpu_interrupt = config_get_string_value(config_kernel, "PUERTO_CPU_INTERRUPT");
    quantum_krn = config_get_int_value(config_kernel, "QUANTUM");
    ip_memoria = config_get_string_value(config_kernel, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config_kernel, "PUERTO_MEMORIA");
    grado_multiprogramacion = config_get_int_value(config_kernel, "GRADO_MULTIPROGRAMACION");
    tipo_de_planificacion = config_get_string_value(config_kernel, "ALGORITMO_PLANIFICACION");

    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE CPU", ip_cpu, puerto_cpu_dispatch);
    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);

    int server_kernel = iniciar_servidor(logger_kernel, puerto_escucha);
    log_info(logger_kernel, "Servidor listo para recibir al cliente");

    // CONEXIONES
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_operacion("KERNEL LLEGO A LA CASA MAMIIII", conexion_memoria, MENSAJE);
    conexion_cpu_dispatch = crear_conexion(ip_cpu, puerto_cpu_dispatch);
    enviar_operacion("KERNEL LLEGO A LA CASA MAMIIII", conexion_cpu_dispatch, MENSAJE);
    conexion_cpu_interrupt = crear_conexion(ip_cpu, puerto_cpu_interrupt);
    enviar_operacion("KERNEL LLEGO A LA CASA MAMIIII", conexion_cpu_interrupt, MENSAJE);
    cliente_fd = esperar_cliente(server_kernel, logger_kernel);

    log_info(logger_kernel, "Conexiones con modulos establecidas");

    ArgsGestionarServidor args_sv_cpu = {logger_kernel, conexion_cpu_dispatch};
    pthread_create(&id_hilo[0], NULL, gestionar_llegada_kernel_cpu, (void *)&args_sv_cpu);

    ArgsGestionarServidor args_sv_memoria = {logger_kernel, conexion_memoria};
    pthread_create(&id_hilo[0], NULL, gestionar_llegada_kernel_memoria, (void *)&args_sv_memoria);

    ArgsGestionarServidor args_sv_io = {logger_kernel, cliente_fd};
    pthread_create(&id_hilo[1], NULL, gestionar_llegada_io_kernel, (void *)&args_sv_io);

    sleep(2);

    pthread_create(&id_hilo[2], NULL, leer_consola, NULL);

    for (i = 0; i < 3; i++)
    {
        pthread_join(id_hilo[i], NULL);
    }
    pthread_join(planificacion, NULL);
    terminar_programa(logger_kernel, config_kernel);
    liberar_conexion(conexion_cpu_interrupt);
    liberar_conexion(conexion_cpu_dispatch);
    liberar_conexion(conexion_memoria);

    return 0;
}

int ejecutar_script(char *path_inst_kernel)
{
    char comando[266];

    FILE *f = fopen(path_inst_kernel, "rb");

    if (f == NULL)
    {
        log_error(logger_kernel, "No se pudo abrir el archivo de %s\n", path_inst_kernel);
        return 1;
    }

    while (!feof(f))
    {
        char *comando_a_ejecutar = fgets(comando, sizeof(comando), f);
        execute_line(comando_a_ejecutar, logger_kernel);
    }

    fclose(f);
    return 0;
}

int iniciar_proceso(char *path)
{
    paqueteDeMensajes(conexion_memoria, path, CREAR_PROCESO);

    sem_wait(&creacion_proceso);
    proceso_creado->contexto->PID = idProceso;
    proceso_creado->contexto->quantum = quantum_krn;
    proceso_creado->estadoActual = "NEW";
    proceso_creado->contexto->registros->PC = 0;

    queue_push(cola_new, proceso_creado);

    log_info(logger_kernel_mov_colas, "Se creo el proceso n° %d en NEW", proceso_creado->contexto->PID);

    if (procesos_en_ram < grado_multiprogramacion)
    {
        cambiar_de_new_a_ready(proceso_creado);
    }
    idProceso++;
    return 0;
}

int finalizar_proceso(char *PID)
{
    int pid = atoi(PID);

    if (buscar_pcb_en_cola(cola_new, pid) != NULL)
    {
        cambiar_de_new_a_exit(buscar_pcb_en_cola(cola_new, pid));
    }
    else if (buscar_pcb_en_cola(cola_ready, pid) != NULL)
    {
        cambiar_de_ready_a_exit(buscar_pcb_en_cola(cola_ready, pid));
    }
    else if (buscar_pcb_en_cola(cola_running, pid) != NULL)
    {
        paqueteDeMensajes(conexion_cpu_interrupt, "-Interrupcion por usuario-", INTERRUPCION);

        cambiar_de_execute_a_exit(buscar_pcb_en_cola(cola_running, pid));
    }
    else if (buscar_pcb_en_cola(cola_blocked, pid) != NULL)
    {
        cambiar_de_blocked_a_exit(buscar_pcb_en_cola(cola_blocked, pid));
    }
    else if (buscar_pcb_en_cola(cola_exit, pid) == NULL)
    {
        log_error(logger_kernel, "El PCB con PID n°%d no existe", pid);
        return (void*)EXIT_FAILURE;
    }

    if (procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_new))
    {
        cambiar_de_new_a_ready((pcb *)queue_peek(cola_new));
    }

    liberar_recursos(pid, INTERRUPTED);

    return 0;
}

int iniciar_planificacion()
{
    sem_post(&sem_planif);
    
    switch (determinar_planificacion(tipo_de_planificacion))
    {
    case ALG_FIFO:
        log_warning(logger_kernel_planif, "\t-INICIANDO PLANIFICACION CON FIFO-\n");
        pthread_create(&planificacion, NULL, FIFO, NULL);
        break;
    case ALG_RR:
        log_warning(logger_kernel_planif, "\t-INICIANDO PLANIFICACION CON RR-\n");
        pthread_create(&planificacion, NULL, RR, NULL);
        break;
    case ALG_VRR:
        log_warning(logger_kernel_planif, "\t-INICIANDO PLANIFICACION CON VRR-\n");
        pthread_create(&planificacion, NULL, VRR, NULL);
        break;
    default:
        log_error(logger_kernel, "\t\t\t--Algoritmo invalido--\n");
        break;
    }    
    return 0;
}

int detener_planificacion()
{
    sem_wait(&sem_planif);
    log_warning(logger_kernel, "-Deteniendo planificacion-\n...");
    paqueteDeMensajes(conexion_cpu_interrupt, "Detencion de la planificacion", INTERRUPCION);
    log_warning(logger_kernel, "-Planificacion detenida-\n...");
    return 0;
}

int multiprogramacion(char *multiprogramacion)
{
    if (procesos_en_ram < atoi(multiprogramacion))
    {
        grado_multiprogramacion = atoi(multiprogramacion);
        log_info(logger_kernel, "Se ha establecido el grado de multiprogramacion en %d", grado_multiprogramacion);
        config_set_value(config_kernel, "GRADO_MULTIPROGRAMACION", multiprogramacion);
    }
    else
    {
        log_error(logger_kernel, "Desaloje elementos de la cola antes de cambiar el grado de multiprogramacion");
    }
    return 0;
}

int proceso_estado()
{
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

int interfaces_conectadas()
{
    printf("INTERFACES CONECTADAS.\n");
    iterar_lista_e_imprimir(interfaces);
    return 0;
}

void iterar_cola_e_imprimir(t_queue *cola)
{
    t_list_iterator *lista_a_iterar = list_iterator_create(cola->elements);
    printf("%d\n", list_size(cola->elements));

    if (lista_a_iterar != NULL)
    {
        printf("\t PIDs : [ ");
        while (list_iterator_has_next(lista_a_iterar))
        {
            pcb *elemento_actual = list_iterator_next(lista_a_iterar);

            if (list_iterator_has_next(lista_a_iterar))
            {
                printf("%d <- ", elemento_actual->contexto->PID);
            }
            else
            {
                printf("%d", elemento_actual->contexto->PID);
            }
        }
        printf(" ]\n");
    }
    list_iterator_destroy(lista_a_iterar);
}

void iterar_lista_e_imprimir(t_list *lista)
{
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
        printf(" ]\tElementos totales: %d\n", list_size(lista));
    }
    list_iterator_destroy(lista_a_iterar);
}

// FUNCIONES DE BUSCAR Y ELIMINAR

pcb *buscar_pcb_en_cola(t_queue *cola, int PID)
{
    pcb *elemento_a_encontrar;

    bool es_igual_a_aux(void *data)
    {
        return es_igual_a(PID, data);
    };

    if (!list_is_empty(cola->elements))
    {
        elemento_a_encontrar = list_find(cola->elements, es_igual_a_aux);
        return elemento_a_encontrar;
    }
    else
    {
        return NULL;
    }
}

int liberar_recursos(int PID, MOTIVO_SALIDA motivo)
{   
    bool es_igual_a_aux(void *data)
    {
        return es_igual_a(PID, data);
    };

    pcb* a_eliminar = list_remove_by_condition(cola_exit->elements, es_igual_a_aux);

    switch (motivo)
    {
    case FIN_INSTRUCCION:
        log_info(logger_kernel_mov_colas, "Finaliza el proceso n°%d - Motivo: SUCCESS", PID);
        break;
    case INTERRUPTED:
        log_info(logger_kernel_mov_colas, "Finaliza el proceso n°%d - Motivo: INTERRUMPED BY USER", PID);
        break;
    default:
        log_info(logger_kernel_mov_colas, "Finaliza el proceso n°%d - Motivo: INVALID_INTERFACE ", PID);
        break;
    }

    peticion_de_eliminacion_espacio_para_pcb(conexion_memoria, a_eliminar, FINALIZAR_PROCESO);
    sem_wait(&finalizacion_proceso);

    return EXIT_SUCCESS;
}

bool es_igual_a(int id_proceso, void *data)
{
    pcb *elemento = (pcb *)data;
    return (elemento->contexto->PID == id_proceso);
}

// CAMBIAR DE COLA

void cambiar_de_new_a_ready(pcb *pcb)
{
    queue_push(cola_ready, (void *)pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "NEW";
    queue_pop(cola_new);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = queue_size(cola_ready) + queue_size(cola_blocked) + queue_size(cola_running);
}

void cambiar_de_ready_a_execute(pcb *pcb)
{
    queue_push(cola_running, (void *)pcb);
    pcb->estadoActual = "EXECUTE";
    pcb->estadoAnterior = "READY";
    queue_pop(cola_ready);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);

    if (procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_new))
    {
        cambiar_de_new_a_ready(queue_peek(cola_new));
    }
}

void cambiar_de_execute_a_ready(pcb *pcb)
{
    queue_push(cola_ready, (void *)pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "EXECUTE";
    queue_pop(cola_running);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_execute_a_blocked(pcb *pcb)
{
    queue_push(cola_blocked, (void *)pcb);
    pcb->estadoActual = "BLOCKED";
    pcb->estadoAnterior = "EXECUTE";
    queue_pop(cola_running);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_blocked_a_ready(pcb *pcb)
{
    queue_push(cola_ready, (void *)pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "BLOCKED";
    queue_pop(cola_blocked);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

// PARA ELIMINACION DE PROCESOS

void cambiar_de_execute_a_exit(pcb *PCB)
{
    queue_push(cola_exit, (void *)PCB);
    PCB->estadoActual = "EXIT";
    PCB->estadoAnterior = "EXECUTE";
    queue_pop(cola_running);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", PCB->contexto->PID, PCB->estadoAnterior, PCB->estadoActual);
    procesos_en_ram = queue_size(cola_ready) + queue_size(cola_blocked) + queue_size(cola_running);
    
    if(procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_new)){
        pcb *en_cola_new = queue_peek(cola_new);
        cambiar_de_new_a_ready(en_cola_new);
    }

    liberar_recursos(PCB->contexto->PID, PCB->contexto->motivo);
}

void cambiar_de_ready_a_exit(pcb *pcb)
{
    queue_push(cola_exit, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "READY";
    list_remove_element(cola_ready->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = queue_size(cola_ready) + queue_size(cola_blocked) + queue_size(cola_running);

    if (procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_new))
    {
        cambiar_de_new_a_ready(queue_peek(cola_new));
    }
}

void cambiar_de_blocked_a_exit(pcb *pcb)
{
    queue_push(cola_exit, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "BLOCKED";
    list_remove_element(cola_blocked->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = queue_size(cola_ready) + queue_size(cola_blocked) + queue_size(cola_running);

    if (procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_new))
    {
        cambiar_de_new_a_ready(queue_peek(cola_new));
    }
}

void cambiar_de_new_a_exit(pcb *pcb)
{
    queue_push(cola_exit, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "NEW";
    list_remove_element(cola_new->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = queue_size(cola_ready) + queue_size(cola_blocked) + queue_size(cola_running);

    if (procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_new))
    {
        cambiar_de_new_a_ready(queue_peek(cola_new));
    }
}

bool lista_validacion_interfaces(INTERFAZ *interfaz, char *solicitud)
{
    int operaciones = sizeof(interfaz->datos->operaciones) / sizeof(interfaz->datos->operaciones[0]);
    for (int i = 0; i < operaciones; i++)
    {
        if (!strcmp(interfaz->datos->operaciones[i], solicitud))
        {
            return true;
        }
    }
    return false;
}

INTERFAZ *interfaz_encontrada(char *nombre)
{
    bool es_nombre_de_interfaz_aux(void *data)
    {
        return es_nombre_de_interfaz(nombre, data);
    };

    return (INTERFAZ *)list_find(interfaces, es_nombre_de_interfaz_aux);
}

bool lista_seek_interfaces(char *nombre)
{
    bool es_nombre_de_interfaz_aux(void *data)
    {
        return es_nombre_de_interfaz(nombre, data);
    };

    if (!list_is_empty(interfaces) && list_find(interfaces, es_nombre_de_interfaz_aux) != NULL)
    {
        return true;
    }else{
        return false;
    }
}

op_code determinar_operacion_io(INTERFAZ* io){
    if(io->datos->tipo == 0){
        return IO_GENERICA;
    }else if(io->datos->tipo == 1){
        return IO_STDIN;
    }else if(io->datos->tipo == 2){
        return IO_STDOUT;
    }else{
        return IO_DIALFS;
    }
}

INTERFAZ* asignar_espacio_a_io(t_list* lista){
    INTERFAZ* nueva_interfaz = malloc(sizeof(INTERFAZ));
    nueva_interfaz = list_get(lista, 0);
    nueva_interfaz->datos = malloc(sizeof(DATOS_INTERFAZ));
    nueva_interfaz->datos = list_get(lista, 1);
    nueva_interfaz->datos->nombre = list_get(lista, 2);
    nueva_interfaz->datos->operaciones = list_get(lista, 3);
    int j = 0;
    for (int i = 4; i < list_size(lista); i++){
        nueva_interfaz->datos->operaciones[j] = strdup((char*)list_get(lista, i));
        j++;
    }

    nueva_interfaz->estado = LIBRE;
    return nueva_interfaz;
}

void *gestionar_llegada_kernel_cpu(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
            free(mensaje);
            break;
        case INTERRUPCION:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            contexto_recibido = list_get(lista, 0);
            contexto_recibido->registros = list_get(lista, 1);
            contexto_recibido->motivo = QUANTUM;
            sem_post(&recep_contexto);
            break;
        case CONTEXTO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            contexto_recibido = list_get(lista, 0);
            contexto_recibido->registros = list_get(lista, 1);
            contexto_recibido->motivo = FIN_INSTRUCCION;
            sem_post(&recep_contexto);
            break;
        case SOLICITUD_IO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            contexto_recibido = list_get(lista, 0);
            contexto_recibido->registros = list_get(lista, 1);
            interfaz_solicitada = list_get(lista, 2);
            interfaz_solicitada->nombre = list_get(lista, 3);
            interfaz_solicitada->solicitud = list_get(lista, 4);
            interfaz_solicitada->args = list_get(lista, 5);

            int j = 0;
            for (int i = 6; i < list_size(lista); i++)
            {
                strcpy(interfaz_solicitada->args[j], list_get(lista, i));
                j++;
            }

            contexto_recibido->motivo = IO;
            sem_post(&recep_contexto);
            break;
        case -1:
            log_error(args_entrada->logger, "el cliente se desconecto. Terminando servidor");
            return (void *)EXIT_FAILURE;
        default:
            log_warning(args_entrada->logger, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

void *gestionar_llegada_io_kernel(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case MENSAJE:
            char *mensaje = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
            free(mensaje);
            break;
        case INSTRUCCION:
            char *instruccion = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, INSTRUCCION);
            free(instruccion);
            break;
        case NUEVA_IO:
            sleep(1);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            INTERFAZ* nueva_interfaz = asignar_espacio_a_io(lista);
            list_add(interfaces, nueva_interfaz);
            log_info(logger_kernel, "\n%s se ha conectado.\n", nueva_interfaz->datos->nombre);
            break;
        case DESCONECTAR_IO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            char* interfaz_a_desconectar = list_get(lista, 0);
            buscar_y_desconectar(interfaz_a_desconectar, interfaces, logger_kernel);
            break;
        case DESCONECTAR_TODO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            char* mensaje_de_desconexion = list_get(lista, 0);
            log_warning(logger_kernel, "%s", mensaje_de_desconexion);
            list_clean_and_destroy_elements(interfaces, destruir_interfaz);
            break;
        case DESBLOQUEAR_PID:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            desbloquear_io *solicitud_entrante = list_get(lista, 0);
            solicitud_entrante->pid = list_get(lista, 1);
            solicitud_entrante->nombre = list_get(lista, 2);
            int id_proceso = atoi(solicitud_entrante->pid);

            pcb* pcb = buscar_pcb_en_cola(cola_blocked, id_proceso);
            cambiar_de_blocked_a_ready(pcb);

            desocupar_io(solicitud_entrante);
            eliminar_io_solicitada(interfaz_solicitada);
            break;
        case -1:
            log_error(args_entrada->logger, "el cliente se desconecto. Terminando servidor");
            return (void *)EXIT_FAILURE;
        default:
            log_warning(args_entrada->logger, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

void *gestionar_llegada_kernel_memoria(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case CREAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            proceso_creado = list_get(lista, 0);
            proceso_creado->path_instrucciones = list_get(lista, 1);
            proceso_creado->contexto = list_get(lista, 2);
            proceso_creado->contexto->registros = list_get(lista, 3);
            sem_post(&creacion_proceso);
            break;
        case FINALIZAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            sem_post(&finalizacion_proceso);
            break;
        case -1:
            log_error(args_entrada->logger, "el cliente se desconecto. Terminando servidor");
            return (void *)EXIT_FAILURE;
        default:
            log_warning(args_entrada->logger, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

void abrir_hilo_interrupcion(int quantum_proceso){
    int quantum_en_seg = (quantum_proceso / 1000);

    args_hilo_interrupcion args = {quantum_en_seg};

    pthread_create(&interrupcion, NULL, interrumpir_por_quantum, (void*)&args);

    pthread_join(interrupcion, NULL);
}

void* interrumpir_por_quantum(void* args){
    args_hilo_interrupcion *args_del_hilo = (args_hilo_interrupcion*)args;

    sleep(args_del_hilo->tiempo_a_esperar);

    paqueteDeMensajes(conexion_cpu_interrupt, "Fin de Quantum", INTERRUPCION);

    log_warning(logger_kernel, "SYSCALL INCOMING...");

    return NULL;
}

void checkear_estado_interfaz(INTERFAZ* interfaz){
    switch (interfaz->estado)
    {
    case OCUPADA:
        log_error(logger_kernel, "-INTERFAZ BLOQUEADA-\n");
        
        //TODO: Logica de si esta ocupada la IO

        break;
    case LIBRE:
        log_info(logger_kernel, "Bloqueando interfaz...\n");
        interfaz->solicitud = interfaz_solicitada;
        enviar_solicitud_io(cliente_fd, interfaz->solicitud, determinar_operacion_io(interfaz));
        interfaz->estado = OCUPADA;
        break;
    default:
        break;
    }
}

void desocupar_io(desbloquear_io *solicitud){
    INTERFAZ* io_a_desbloquear = interfaz_encontrada(solicitud->nombre); 

    io_a_desbloquear->estado = LIBRE;

    log_info(logger_kernel, "Se desbloqueo la interfaz %s.\n", io_a_desbloquear->datos->nombre);

    liberar_solicitud_de_desbloqueo(solicitud);
}

void liberar_solicitud_de_desbloqueo(desbloquear_io *solicitud){
    free(solicitud->nombre);
    solicitud->nombre = NULL;
    free(solicitud->pid);
    solicitud->pid = NULL;
    free(solicitud);
    solicitud = NULL;
}

ALG_PLANIFICACION determinar_planificacion(char* tipo){
    if(!strcmp(tipo, "FIFO")){
        return ALG_FIFO;
    }else if(!strcmp(tipo, "RR")){
        return ALG_RR;
    }else if(!strcmp(tipo, "VRR")){
        return ALG_VRR;
    }
    return ERROR;
}