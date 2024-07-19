#include <stdio.h>
#include <kernel.h>

int cliente_fd;
int conexion_memoria;
int conexion_cpu_dispatch;
int conexion_cpu_interrupt;
int quantum_krn;
int grado_multiprogramacion;
int coef_interrupcion;
int procesos_en_ram = 0;
int idProceso = 0;


bool llego_contexto;
bool flag_interrupcion;
bool flag_pasaje_ready;

char* tipo_de_planificacion;
int server_kernel;
char* name_recurso;

t_list *interfaces;
t_list *recursos;
t_list *solicitudes;

// COLAS DE ESTADO

t_queue *cola_new;
t_queue *cola_ready;
t_queue *cola_ready_prioridad;
t_queue *cola_running;
t_queue *cola_blocked;
t_queue *cola_exit;


t_log *logger_kernel;
t_log *logger_interfaces;
t_log *logger_kernel_planif;
t_log *logger_kernel_mov_colas;

t_config *config_kernel;

pcb* proceso_creado;
cont_exec *contexto_recibido;
SOLICITUD_INTERFAZ *interfaz_solicitada;

pthread_mutex_t mutex_cola_new = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_ready_prioridad = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_blocked = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_eliminacion = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_recursos = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_contexto = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_intefaces = PTHREAD_MUTEX_INITIALIZER;

pthread_t planificacion;
pthread_t interrupcion;

sem_t sem_planif;
sem_t recep_contexto;
sem_t creacion_proceso;
sem_t finalizacion_proceso;
sem_t sem_permiso_memoria;
sem_t sem_pasaje_a_ready;

void *FIFO(){
    
    while (1)
    {
        sem_wait(&sem_planif);
        if (queue_is_empty(cola_running) && queue_size(cola_ready) > 0)
        {
            pthread_mutex_lock(&mutex_cola_ready);
            pcb *a_ejecutar = queue_peek(cola_ready);
            cambiar_de_ready_a_execute(a_ejecutar);
            pthread_mutex_unlock(&mutex_cola_ready);

            // Enviamos mensaje para mandarle el path que debe abrir
            log_info(logger_kernel, "\n------------------------------------------------------------\n\t\t\t-Partio proceso: %d-\nPC: %d\n------------------------------------------------------------", a_ejecutar->contexto->PID, a_ejecutar->contexto->registros->PC);

            // Enviamos el pcb a CPU
            enviar_contexto_pcb(conexion_cpu_dispatch, a_ejecutar->contexto, CONTEXTO);

            // Recibimos el contexto denuevo del CPU

            sem_wait(&recep_contexto);

            a_ejecutar->contexto = contexto_recibido;

            log_info(logger_kernel_planif, "\n------------------------------------------------------------\n\t\t\t-Llego proceso %d-\nPC: %d\n------------------------------------------------------------", a_ejecutar->contexto->PID, a_ejecutar->contexto->registros->PC);

            switch (a_ejecutar->contexto->motivo)
            {
            case FIN_INSTRUCCION:
                cambiar_de_execute_a_exit(a_ejecutar);
                break;
            case T_WAIT:
                log_info(logger_kernel_planif, "PID: %d - Solicito recurso %s", a_ejecutar->contexto->PID, name_recurso);
                cambiar_de_execute_a_blocked(a_ejecutar);
                pthread_mutex_lock(&mutex_recursos);
                asignar_instancia_recurso(a_ejecutar, name_recurso);
                pthread_mutex_unlock(&mutex_recursos);
                break;
            case T_SIGNAL:
                log_info(logger_kernel_planif, "PID: %d - Libero recurso %s", a_ejecutar->contexto->PID, name_recurso);
                cambiar_de_execute_a_blocked(a_ejecutar);
                pthread_mutex_lock(&mutex_recursos);
                liberar_instancia_recurso(a_ejecutar, name_recurso);
                pthread_mutex_unlock(&mutex_recursos);
                break;
            case SIN_MEMORIA:
                log_info(logger_kernel_planif, "PID: %d - Sin memoria disponible", a_ejecutar->contexto->PID);
                cambiar_de_execute_a_exit(a_ejecutar);
                break;
            case INTERRUPTED:
                log_info(logger_kernel_planif, "PID: %d - Interrumpido por usuario", a_ejecutar->contexto->PID);
                cambiar_de_execute_a_exit(a_ejecutar);
                break;
            default:
                 if (lista_seek_interfaces(interfaz_solicitada->nombre))
                {
                    INTERFAZ *interfaz = interfaz_encontrada(interfaz_solicitada->nombre);
                    if (lista_validacion_interfaces(interfaz, interfaz_solicitada->solicitud))
                    {
                        log_info(logger_kernel_mov_colas, "Operacion correcta. Enseguida se realizara la petición.");
                        interfaz_solicitada ->pid = string_itoa(a_ejecutar->contexto->PID);
                        checkear_estado_interfaz(interfaz, a_ejecutar);
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
        
        if(flag_interrupcion){
            flag_interrupcion = false;
            return NULL;
        }
        
        sem_post(&sem_planif);
    }
    return NULL;
}

void *RR(){

    while (1)
    {
        sem_wait(&sem_planif);
        if (queue_is_empty(cola_running) && queue_size(cola_ready) > 0)
        {
            pthread_mutex_lock(&mutex_cola_ready);
            pcb *a_ejecutar = queue_peek(cola_ready);
            cambiar_de_ready_a_execute(a_ejecutar);
            pthread_mutex_unlock(&mutex_cola_ready);

            // Enviamos mensaje para mandarle el path que debe abrir
            log_info(logger_kernel_planif, "\n------------------------------------------------------------\n\t\t\t-Partio proceso %d-\nPC: %d\nQuantum: %d\n------------------------------------------------------------", a_ejecutar->contexto->PID, a_ejecutar->contexto->registros->PC, a_ejecutar->contexto->quantum);

            // Enviamos el pcb a CPU
            enviar_contexto_pcb(conexion_cpu_dispatch, a_ejecutar->contexto, CONTEXTO);

            // Esperamos a que pasen los segundos de quantum

            abrir_hilo_interrupcion(quantum_krn);            

            // Recibimos el contexto denuevo del CPU

            sem_wait(&recep_contexto);

            a_ejecutar->contexto = contexto_recibido;

            log_info(logger_kernel_planif, "\n------------------------------------------------------------\n\t\t\t-Llego proceso %d-\nPC: %d\nQuantum: %d\n------------------------------------------------------------", a_ejecutar->contexto->PID, a_ejecutar->contexto->registros->PC, a_ejecutar->contexto->quantum);

            switch (a_ejecutar->contexto->motivo)
            {
            case FIN_INSTRUCCION:
                cambiar_de_execute_a_exit(a_ejecutar);
                break;
            case QUANTUM:
                log_info(logger_kernel_planif, "PID: %d - Desalojado por fin de quantum", a_ejecutar->contexto->PID);
                a_ejecutar->contexto->quantum = quantum_krn;
                cambiar_de_execute_a_ready(a_ejecutar);
                break;
            case T_WAIT:
                log_info(logger_kernel_planif, "PID: %d - Solicito recurso %s", a_ejecutar->contexto->PID, name_recurso);
                cambiar_de_execute_a_blocked(a_ejecutar);
                pthread_mutex_lock(&mutex_recursos);
                asignar_instancia_recurso(a_ejecutar, name_recurso);
                pthread_mutex_unlock(&mutex_recursos);
                break;
            case T_SIGNAL:
                log_info(logger_kernel_planif, "PID: %d - Libero recurso %s", a_ejecutar->contexto->PID, name_recurso);
                cambiar_de_execute_a_blocked(a_ejecutar);
                pthread_mutex_lock(&mutex_recursos);
                liberar_instancia_recurso(a_ejecutar, name_recurso);
                pthread_mutex_unlock(&mutex_recursos);
                break;
            case SIN_MEMORIA:
                log_info(logger_kernel_planif, "PID: %d - Sin memoria disponible", a_ejecutar->contexto->PID);
                cambiar_de_execute_a_exit(a_ejecutar);
                break;
            case INTERRUPTED:
                log_info(logger_kernel_planif, "PID: %d - Interrumpido por usuario", a_ejecutar->contexto->PID);
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
                        checkear_estado_interfaz(interfaz, a_ejecutar);                   
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
        
        if(flag_interrupcion){
            flag_interrupcion = false;
            return NULL;
        }

        sem_post(&sem_planif);
    }
    return NULL;
}

void *VRR(){

    while (1)
    {
        sem_wait(&sem_planif);
        if (queue_is_empty(cola_running) && (queue_size(cola_ready) + queue_size(cola_ready_prioridad)) > 0)
        {
            pcb *a_ejecutar;

            if(!queue_is_empty(cola_ready_prioridad)){
                pthread_mutex_lock(&mutex_cola_ready_prioridad);
                a_ejecutar = queue_peek(cola_ready_prioridad);
                cambiar_de_ready_prioridad_a_execute(a_ejecutar);
                pthread_mutex_unlock(&mutex_cola_ready_prioridad);
            }else{
                pthread_mutex_lock(&mutex_cola_ready);
                a_ejecutar = queue_peek(cola_ready);
                cambiar_de_ready_a_execute(a_ejecutar);
                pthread_mutex_unlock(&mutex_cola_ready);
            }
        
            // Enviamos mensaje para mandarle el path que debe abrir
            log_info(logger_kernel_planif, "\n------------------------------------------------------------\n\t\t\t-Partio proceso %d-\nPC: %d\nQuantum: %d\n------------------------------------------------------------", a_ejecutar->contexto->PID, a_ejecutar->contexto->registros->PC, a_ejecutar->contexto->quantum);

            // Enviamos el pcb a CPU
            enviar_contexto_pcb(conexion_cpu_dispatch, a_ejecutar->contexto, CONTEXTO);

            t_temporal* tiempo_de_ejecucion = temporal_create();

            // Esperamos a que pasen los segundos de quantum
            abrir_hilo_interrupcion(a_ejecutar->contexto->quantum);            

            // Recibimos el contexto denuevo del CPU
            sem_wait(&recep_contexto);

            temporal_stop(tiempo_de_ejecucion);

            int64_t tiempo_transcurrido = temporal_gettime(tiempo_de_ejecucion);

            temporal_destroy(tiempo_de_ejecucion);
    
            a_ejecutar->contexto = contexto_recibido;

            a_ejecutar->contexto->quantum -= tiempo_transcurrido;

            a_ejecutar->contexto->quantum = redondear_quantum(a_ejecutar->contexto->quantum);

            log_info(logger_kernel_planif, "\n------------------------------------------------------------\n\t\t\t-Llego proceso %d-\nPC: %d\nQuantum: %d\nTiempo transcurrido: %ld\n------------------------------------------------------------", a_ejecutar->contexto->PID, a_ejecutar->contexto->registros->PC, a_ejecutar->contexto->quantum, tiempo_transcurrido);

            switch (a_ejecutar->contexto->motivo)
            {
            case FIN_INSTRUCCION:
                cambiar_de_execute_a_exit(a_ejecutar);
                break;
            case QUANTUM:
                log_info(logger_kernel_planif, "PID: %d - Desalojado por fin de quantum", a_ejecutar->contexto->PID);
                a_ejecutar->contexto->quantum = quantum_krn;
                cambiar_de_execute_a_ready(a_ejecutar);
                break;
            case T_WAIT:
                log_info(logger_kernel_planif, "PID: %d - Solicito recurso %s", a_ejecutar->contexto->PID, name_recurso);
                cambiar_de_execute_a_blocked(a_ejecutar);
                pthread_mutex_lock(&mutex_recursos);
                asignar_instancia_recurso(a_ejecutar, name_recurso);
                pthread_mutex_unlock(&mutex_recursos);
                break;
            case T_SIGNAL:
                log_info(logger_kernel_planif, "PID: %d - Liberara recurso %s", a_ejecutar->contexto->PID, name_recurso);
                cambiar_de_execute_a_blocked(a_ejecutar);
                pthread_mutex_lock(&mutex_recursos);
                liberar_instancia_recurso(a_ejecutar, name_recurso);
                pthread_mutex_unlock(&mutex_recursos);
                break;
            case SIN_MEMORIA:
                log_info(logger_kernel_planif, "PID: %d - Sin memoria disponible", a_ejecutar->contexto->PID);
                cambiar_de_execute_a_exit(a_ejecutar);
                break;
            case INTERRUPTED:
                log_info(logger_kernel_planif, "PID: %d - Interrumpido por usuario", a_ejecutar->contexto->PID);
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
                        checkear_estado_interfaz(interfaz, a_ejecutar);
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

        if(flag_interrupcion){
            flag_interrupcion = false;
            return NULL;
        }

        sem_post(&sem_planif);
    }
}

void *leer_consola(){
    log_info(logger_kernel, "\n\t\t-CONSOLA INTERACTIVA DE KERNEL-\n");
    printf("- PARA EJECUTAR_SCRIPT c-comenta-pruebas: /scripts_kernel/(Nombre script) -\n");
    printf("- PARA INICIAR_PROCESO c-comenta-pruebas: /scripts_memoria/(Nombre instrucciones) -\n");
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
                usleep(250000);
            }
            free(leido);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]){
    cola_new = queue_create();
    cola_ready = queue_create();
    cola_ready_prioridad = queue_create();
    cola_running = queue_create();
    cola_blocked = queue_create();
    cola_exit = queue_create();

    interfaces = list_create();
    recursos = list_create();
    solicitudes = list_create();

    pthread_t id_hilo[4];
    
    sem_init(&sem_planif, 1, 0);
    sem_init(&recep_contexto, 1, 0);
    sem_init(&creacion_proceso, 1, 0);
    sem_init(&finalizacion_proceso, 1, 0);
    sem_init(&sem_permiso_memoria, 1, 0);
    sem_init(&sem_pasaje_a_ready, 1, 0);

    logger_kernel = iniciar_logger("kernel.log", "kernel-log", LOG_LEVEL_INFO);
    logger_interfaces = iniciar_logger("interfaces-kernel.log", "interfaces-kernel-log", LOG_LEVEL_INFO);
    logger_kernel_mov_colas = iniciar_logger("kernel_colas.log", "kernel_colas-log", LOG_LEVEL_INFO);
    logger_kernel_planif = iniciar_logger("kernel_planif.log", "kernel_planificacion-log", LOG_LEVEL_INFO);
    log_info(logger_kernel, "\n \t\t\t-INICIO LOGGER GENERAL- \n");
    log_info(logger_interfaces, "\n \t\t\t-INICIO LOGGER RECEPCION DE INTERFACES- \n");
    log_info(logger_kernel_planif, "\n \t\t\t-INICIO LOGGER DE PLANIFICACION- \n");
    log_info(logger_kernel_mov_colas, "\n \t\t\t-INICIO LOGGER DE PROCESOS- \n");

    config_kernel = iniciar_configuracion();

    char* puerto_escucha = config_get_string_value(config_kernel, "PUERTO_ESCUCHA");
    char *ip_cpu = config_get_string_value(config_kernel, "IP_CPU");
    char *puerto_cpu_dispatch = config_get_string_value(config_kernel, "PUERTO_CPU_DISPATCH");
    char* puerto_cpu_interrupt = config_get_string_value(config_kernel, "PUERTO_CPU_INTERRUPT");
    quantum_krn = config_get_int_value(config_kernel, "QUANTUM");
    char *ip_memoria = config_get_string_value(config_kernel, "IP_MEMORIA");
    char* puerto_memoria = config_get_string_value(config_kernel, "PUERTO_MEMORIA");
    grado_multiprogramacion = config_get_int_value(config_kernel, "GRADO_MULTIPROGRAMACION");
    tipo_de_planificacion = config_get_string_value(config_kernel, "ALGORITMO_PLANIFICACION");
    char** nombres_recursos = config_get_array_value(config_kernel, "RECURSOS");
    char** instancias_recursos = config_get_array_value(config_kernel, "INSTANCIAS_RECURSOS");

    llenar_lista_de_recursos(nombres_recursos, instancias_recursos, recursos);

    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE CPU", ip_cpu, puerto_cpu_dispatch);
    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);

    server_kernel = iniciar_servidor(logger_kernel, puerto_escucha);
    log_info(logger_kernel, "Servidor listo para recibir al cliente");

    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_operacion("KERNEL LLEGO A LA CASA MAMIIII", conexion_memoria, MENSAJE);
    conexion_cpu_dispatch = crear_conexion(ip_cpu, puerto_cpu_dispatch);
    enviar_operacion("KERNEL LLEGO A LA CASA MAMIIII", conexion_cpu_dispatch, MENSAJE);
    conexion_cpu_interrupt = crear_conexion(ip_cpu, puerto_cpu_interrupt);
    enviar_operacion("KERNEL LLEGO A LA CASA MAMIIII", conexion_cpu_interrupt, MENSAJE);

    log_info(logger_kernel, "Conexiones con modulos establecidas");

    ArgsGestionarServidor args_sv_cpu = {logger_kernel, conexion_cpu_dispatch};
    pthread_create(&id_hilo[0], NULL, gestionar_llegada_kernel_cpu, (void *)&args_sv_cpu);

    ArgsGestionarServidor args_sv_memoria = {logger_kernel, conexion_memoria};
    pthread_create(&id_hilo[1], NULL, gestionar_llegada_kernel_memoria, (void *)&args_sv_memoria);

    pthread_create(&id_hilo[2],NULL, esperar_nuevo_io, NULL);

    sleep(1);

    pthread_create(&id_hilo[3], NULL, leer_consola, NULL);

    pthread_join(id_hilo[3], NULL);

    for (int i = 0; i < 4; i++)
    {
        pthread_join(id_hilo[i], NULL);
    }
    pthread_join(planificacion, NULL);
    
    list_destroy_and_destroy_elements(recursos, eliminar_recursos);
    
    queue_destroy(cola_new);
    queue_destroy(cola_ready);
    queue_destroy(cola_ready_prioridad);
    queue_destroy(cola_blocked);
    queue_destroy(cola_running);
    queue_destroy(cola_exit);
    
    sem_destroy(&sem_planif);
    sem_destroy(&recep_contexto);
    sem_destroy(&creacion_proceso);
    sem_destroy(&finalizacion_proceso);
    sem_destroy(&sem_pasaje_a_ready);
    sem_destroy(&sem_permiso_memoria);

    pthread_mutex_destroy(&mutex_cola_blocked);
    pthread_mutex_destroy(&mutex_cola_eliminacion);
    pthread_mutex_destroy(&mutex_cola_ready);
    pthread_mutex_destroy(&mutex_cola_ready_prioridad);
    pthread_mutex_destroy(&mutex_recursos);

    terminar_programa(logger_kernel, config_kernel);
    log_destroy(logger_interfaces);
    log_destroy(logger_kernel_mov_colas);
    log_destroy(logger_kernel_planif);
    
    liberar_conexion(conexion_cpu_interrupt);
    liberar_conexion(conexion_cpu_dispatch);
    liberar_conexion(conexion_memoria);

    return 0;
}

t_config* iniciar_configuracion(){
    printf("1. Cargar configuracion de Prueba planificacion\n");
    printf("2. Cargar configuracion de Prueba deadlock\n");
    printf("3. Cargar configuracion de Prueba memoria-tlb\n");
    printf("4. Cargar configuracion de Prueba IO\n");
    printf("5. Cargar configuracion de Prueba FS\n");
    printf("6. Cargar configuracion de Prueba salvations edge\n");
    char* opcion_en_string = readline("Seleccione una opción: ");
    int opcion = atoi(opcion_en_string);
    free(opcion_en_string);

    switch (opcion)
        {
        case 6:
            log_info(logger_kernel, "Se cargo la configuracion SVE correctamente");
            return iniciar_config("../kernel/configs/prueba_salvations_edge.config");
        case 1:
            log_info(logger_kernel, "Se cargo la configuracion PLN correctamente");
            return iniciar_config("../kernel/configs/prueba_planificacion.config");
        case 2:
            log_info(logger_kernel, "Se cargo la configuracion DLK correctamente");
            return iniciar_config("../kernel/configs/prueba_deadlock.config");
        case 3:            
            log_info(logger_kernel, "Se cargo la configuracion MTLB correctamente");
            return iniciar_config("../kernel/configs/prueba_memoria_tlb.config");
        case 4:            
            log_info(logger_kernel, "Se cargo la configuracion IO correctamente");
            return iniciar_config("../kernel/configs/prueba_io.config");
        case 5:            
            log_info(logger_kernel, "Se cargo la configuracion FS correctamente");
            return iniciar_config("../kernel/configs/prueba_fs.config");
        default:
            log_info(logger_kernel, "Se cargo la configuracion PLN correctamente");
            return iniciar_config("../kernel/configs/prueba_planificacion.config");
        }
}

int ejecutar_script(char *path_inst_kernel){
    char comando[100];

    char* path_instructions = "/home/utnso/c-comenta-pruebas";

    char* cabeza_path = malloc(strlen(path_instructions) + 1 + strlen(path_inst_kernel) + 1);
    strcpy(cabeza_path, path_instructions);
    strcat(cabeza_path, path_inst_kernel);

    FILE *f = fopen(cabeza_path, "r");

    if (f == NULL)
    {
        log_error(logger_kernel, "No se pudo abrir el archivo de %s\n", path_inst_kernel);
        return 1;
    }

    while (!feof(f))
    {
        char *comando_a_ejecutar = fgets(comando, sizeof(comando), f);
        execute_line(comando_a_ejecutar, logger_kernel);
        //sleep(1);
    }

    fclose(f);

    free(cabeza_path);
    cabeza_path = NULL;

    return 0;
}

int iniciar_proceso(char *path){
    c_proceso_data* data_memoria = malloc(sizeof(c_proceso_data));
    data_memoria->path = strdup(path);
    data_memoria->id_proceso = idProceso;
    paquete_creacion_proceso(conexion_memoria, data_memoria);

    sem_wait(&creacion_proceso);
    proceso_creado->contexto->PID = idProceso;
    proceso_creado->contexto->quantum = quantum_krn;
    proceso_creado->estadoActual = "NEW";
    proceso_creado->contexto->registros->PC = 0;

    free(data_memoria->path);
    data_memoria->path = NULL;
    free(data_memoria);
    data_memoria = NULL;

    pthread_mutex_lock(&mutex_cola_new);
    queue_push(cola_new, proceso_creado);

    log_info(logger_kernel_mov_colas, "Se creo el proceso n° %d en NEW", proceso_creado->contexto->PID);

    if (procesos_en_ram < grado_multiprogramacion)
    {
        paquete_guardar_en_memoria(conexion_memoria, proceso_creado);
        sem_wait(&sem_permiso_memoria);
        if(flag_pasaje_ready){
            cambiar_de_new_a_ready(proceso_creado);
            flag_pasaje_ready = false;
        }
    }
    idProceso++;
    pthread_mutex_unlock(&mutex_cola_new);
    return 0;
}

int finalizar_proceso(char *PID){
    int pid = atoi(PID);

    pcb* pcb;

    if (buscar_pcb_en_cola(cola_new, pid) != NULL)
    {
        pthread_mutex_lock(&mutex_cola_new);
        cambiar_de_new_a_exit(buscar_pcb_en_cola(cola_new, pid));
        pthread_mutex_unlock(&mutex_cola_new);
    }
    else if (buscar_pcb_en_cola(cola_ready, pid) != NULL)
    {
        pthread_mutex_lock(&mutex_cola_ready);
        cambiar_de_ready_a_exit(buscar_pcb_en_cola(cola_ready, pid));
        pthread_mutex_unlock(&mutex_cola_ready);
    }
    else if (buscar_pcb_en_cola(cola_running, pid) != NULL)
    {
        paqueteDeMensajes(conexion_cpu_interrupt, "-Interrupcion por usuario-", INTERRUPCION);
        return EXIT_SUCCESS;
    }
    else if (buscar_pcb_en_cola(cola_blocked, pid) != NULL)
    {   
        pthread_mutex_lock(&mutex_cola_blocked);
        cambiar_de_blocked_a_exit(buscar_pcb_en_cola(cola_blocked, pid));
        pthread_mutex_unlock(&mutex_cola_blocked);
    }
    else if (buscar_pcb_en_cola(cola_blocked, pid) == NULL){
        for(int i = 0; i < list_size(recursos); i++){
            t_recurso* recurso = list_get(recursos, i);
            pcb = buscar_pcb_en_cola(recurso->procesos_bloqueados, pid);
            if(pcb != NULL){
                cambiar_de_resourse_blocked_a_exit(pcb, recurso->nombre);
                liberar_recursos(pid, INTERRUPTED);
                return 0;
            }
        }
    }else{
        for(int j = 0; j < list_size(interfaces); j++){
            INTERFAZ* io = list_get(interfaces, j);
            pcb = buscar_pcb_en_cola(io->procesos_bloqueados, pid);
            if(pcb != NULL){
                cambiar_de_blocked_io_a_exit(pcb, io);
                liberar_recursos(pid, INTERRUPTED);
                return 0;
            }
        }
    }
    
    if(pcb == NULL){
        log_error(logger_kernel, "El PCB con PID n°%d no existe", pid);
        return EXIT_FAILURE;
    }
    
    liberar_recursos(pid, INTERRUPTED);

    return 0;
}

int iniciar_planificacion(){    
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

int detener_planificacion(){
    log_warning(logger_kernel, "-Stopping planning-\nWait a second...");
    paqueteDeMensajes(conexion_cpu_interrupt, "detencion de la planificacion", INTERRUPCION);
    flag_interrupcion = true;
    pthread_join(planificacion, NULL);
    log_warning(logger_kernel, "-Planning stopped-\n");
    return 0;
}

int algoritmo_planificacion(char* algoritmo){
    eliminarEspaciosBlanco(algoritmo);
    switch (determinar_planificacion(algoritmo))
        {
        case ALG_FIFO:
            log_info(logger_kernel, "Se cambio la planificacion a FIFO");
            config_set_value(config_kernel, "ALGORITMO_PLANIFICACION", "FIFO");
            break;
        case ALG_RR:
            log_info(logger_kernel, "Se cambio la planificacion a RR");
            config_set_value(config_kernel, "ALGORITMO_PLANIFICACION", "RR");
            break;
        case ALG_VRR:
            log_info(logger_kernel, "Se cambio la planificacion a VRR");
            config_set_value(config_kernel, "ALGORITMO_PLANIFICACION", "VRR");
            break;
        default:
            log_error(logger_kernel, "Planificacion invalida. Vuelva a ingresar por favor.");
            break;
    }
    return 0;
}

int multiprogramacion(char *g_multiprogramacion){
    grado_multiprogramacion = atoi(g_multiprogramacion);
    log_info(logger_kernel, "Multiprogramming level set to %d", grado_multiprogramacion);
    config_set_value(config_kernel, "GRADO_MULTIPROGRAMACION", g_multiprogramacion);

    while (grado_multiprogramacion > procesos_en_ram && !queue_is_empty(cola_new)){
        pcb* proceso_a_cambiar = queue_peek(cola_new);
        paquete_guardar_en_memoria(conexion_memoria, proceso_a_cambiar);
        sem_wait(&sem_permiso_memoria);
        if(flag_pasaje_ready){
            cambiar_de_new_a_ready(proceso_a_cambiar);
            flag_pasaje_ready = false;
        }
    }
    return 0;
}

int proceso_estado(){
    printf("NEW Queue:\t");
    iterar_cola_e_imprimir(cola_new);
    printf("READY Queue:\t");
    iterar_cola_e_imprimir(cola_ready);

    if(determinar_planificacion(tipo_de_planificacion) == ALG_VRR){
        printf("PRIORITY READY Queue:\t");
        iterar_cola_e_imprimir(cola_ready_prioridad);
    }

    printf("EXECUTE Queue:\t");
    iterar_cola_e_imprimir(cola_running);
    printf("BLOCKED Queue:\t");
    iterar_cola_e_imprimir(cola_blocked);

    for(int i = 0; i < list_size(recursos); i++){
        t_recurso* recurso = list_get(recursos, i);
        printf("RESOURSE <%s> BLOCKED Queue:\t", recurso->nombre);
        iterar_cola_e_imprimir(recurso->procesos_bloqueados);
    }

    for(int j = 0; j < list_size(interfaces); j++){
        INTERFAZ* io = list_get(interfaces, j);
        printf("INTERFAZ <%s> BLOCKED Queue:\t", io->sockets->nombre);
        iterar_cola_e_imprimir(io->procesos_bloqueados);
    }

    printf("EXIT Queue:\t");
    iterar_cola_e_imprimir(cola_exit);
    return 0;
}

int interfaces_conectadas(){
    printf("CONNECTED IOs.\n");
    iterar_lista_interfaces_e_imprimir(interfaces);
    return 0;
}

int recursos_actuales(){
    printf("-SYSTEM RESOURSES-\n");
    iterar_lista_recursos_e_imprimir(recursos);
    return 0;
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

void iterar_cola_e_imprimir(t_queue *cola){
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

void iterar_lista_interfaces_e_imprimir(t_list *lista){
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
                printf("%s - ", interfaz->sockets->nombre);
            }
            else
            {
                printf("%s", interfaz->sockets->nombre);
            }
        }
        printf(" ]\tInterfaces conectadas: %d\n", list_size(lista));
    }
    list_iterator_destroy(lista_a_iterar);
}

void iterar_lista_recursos_e_imprimir(t_list *lista){
    t_recurso *recurso;
    t_list_iterator *lista_a_iterar = list_iterator_create(lista);
    if (lista_a_iterar != NULL)
    { // Verificar que el iterador se haya creado correctamente
        printf(" [ ");
        while (list_iterator_has_next(lista_a_iterar))
        {
            recurso = list_iterator_next(lista_a_iterar); // Convertir el puntero genérico a pcb*

            if (list_iterator_has_next(lista_a_iterar))
            {
                printf("%s : %d - ", recurso->nombre, recurso->instancia);
            }
            else
            {
                printf("%s : %d", recurso->nombre, recurso->instancia);
            }
        }
        printf(" ]\tRecursos actuales: %d\n", list_size(lista));
    }
    list_iterator_destroy(lista_a_iterar);
}

// ---------------------------------------- FUNCIONES DE BUSCAR Y ELIMINAR ---------------------------------------- 

pcb *buscar_pcb_en_cola(t_queue *cola, int PID){
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

int liberar_recursos(int PID, MOTIVO_SALIDA motivo){   
    pthread_mutex_lock(&mutex_cola_eliminacion);
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
    case T_WAIT:
        log_info(logger_kernel_mov_colas, "Finaliza el proceso n°%d - Motivo: INVALID_RESOURSE", PID);
        break;
    case T_SIGNAL:
        log_info(logger_kernel_mov_colas, "Finaliza el proceso n°%d - Motivo: INVALID_RESOURSE", PID);
        break;
    case SIN_MEMORIA:
        log_info(logger_kernel_mov_colas, "Finaliza el proceso n°%d - Motivo: OUT_OF_MEMORY", PID);
        break;
    default:
        log_info(logger_kernel_mov_colas, "Finaliza el proceso n°%d - Motivo: INVALID_INTERFACE", PID);
        break;
    }

    if(!list_is_empty(a_eliminar->recursos_adquiridos)){
        
        liberar_todos_recursos_asignados(a_eliminar);

        list_destroy(a_eliminar->recursos_adquiridos);
    }

    peticion_de_eliminacion_espacio_para_pcb(conexion_memoria, a_eliminar, FINALIZAR_PROCESO);
    pthread_mutex_unlock(&mutex_cola_eliminacion);
    sem_wait(&finalizacion_proceso);

    return EXIT_SUCCESS;
}

bool es_igual_a(int id_proceso, void *data){
    pcb *elemento = (pcb *)data;
    return (elemento->contexto->PID == id_proceso);
}

// ---------------------------------------- CAMBIO DE COLA ----------------------------------------

void cambiar_de_new_a_ready(pcb *pcb){
    queue_push(cola_ready, (void *)pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "NEW";
    queue_pop(cola_new);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    
    pthread_mutex_lock(&mutex_cola_eliminacion);
    procesos_en_ram++;
    pthread_mutex_unlock(&mutex_cola_eliminacion);
}

void cambiar_de_ready_a_execute(pcb *pcb){
    queue_push(cola_running, (void *)pcb);
    pcb->estadoActual = "EXECUTE";
    pcb->estadoAnterior = "READY";
    queue_pop(cola_ready);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);

    checkear_pasaje_a_ready();
}

void cambiar_de_ready_prioridad_a_execute(pcb *pcb){
    queue_push(cola_running, (void *)pcb);
    pcb->estadoActual = "EXECUTE";
    pcb->estadoAnterior = "READY_PRIORIDAD";
    queue_pop(cola_ready_prioridad);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);

    checkear_pasaje_a_ready();
}

void cambiar_de_execute_a_ready(pcb *pcb){
    queue_push(cola_ready, (void *)pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "EXECUTE";
    queue_pop(cola_running);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_execute_a_blocked(pcb *pcb){
    queue_push(cola_blocked, (void *)pcb);
    pcb->estadoActual = "BLOCKED";
    pcb->estadoAnterior = "EXECUTE";
    queue_pop(cola_running);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_execute_a_blocked_io(pcb* pcb, INTERFAZ* io){
    queue_push(io->procesos_bloqueados, (void *)pcb);
    pcb->estadoActual = "BLOCKED_IO";
    pcb->estadoAnterior = "EXECUTE";
    queue_pop(cola_running);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_blocked_io_a_ready(pcb* pcb, INTERFAZ* io){
    queue_push(cola_ready, (void *)pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "BLOCKED_IO";
    list_remove_element(io->procesos_bloqueados->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);

    desocupar_io(io);
}

void cambiar_de_blocked_io_a_ready_prioridad(pcb* pcb, INTERFAZ* io){
    queue_push(cola_ready_prioridad, (void *)pcb);
    pcb->estadoActual = "READY_PRIORIDAD";
    pcb->estadoAnterior = "BLOCKED_IO";
    list_remove_element(io->procesos_bloqueados->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);

    desocupar_io(io);
}

void cambiar_de_blocked_io_a_exit(pcb* pcb, INTERFAZ* io){
    queue_push(cola_exit, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = strcat("BLOCKED_IO: ", io->sockets->nombre);
    list_remove_element(io->procesos_bloqueados->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);

    if(io->proceso_asignado == pcb->contexto->PID)
        desocupar_io(io);
}

void cambiar_de_blocked_a_ready(pcb *pcb){
    queue_push(cola_ready, (void *)pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "BLOCKED";
    queue_pop(cola_blocked);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);

    pcb->contexto->quantum = quantum_krn;
}

void cambiar_de_blocked_a_ready_prioridad(pcb *pcb){
    queue_push(cola_ready_prioridad, (void *)pcb);
    pcb->estadoActual = "READY_PRIORIDAD";
    pcb->estadoAnterior = "BLOCKED";
    queue_pop(cola_blocked);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_execute_a_exit(pcb *PCB){
    queue_push(cola_exit, (void *)PCB);
    PCB->estadoActual = "EXIT";
    PCB->estadoAnterior = "EXECUTE";
    queue_pop(cola_running);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", PCB->contexto->PID, PCB->estadoAnterior, PCB->estadoActual);
    
    pthread_mutex_lock(&mutex_cola_eliminacion);
    procesos_en_ram--;
    pthread_mutex_unlock(&mutex_cola_eliminacion);
    
    checkear_pasaje_a_ready();

    liberar_recursos(PCB->contexto->PID, PCB->contexto->motivo);
}

void cambiar_de_ready_a_exit(pcb *pcb){
    queue_push(cola_exit, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "READY";
    list_remove_element(cola_ready->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);

    pthread_mutex_lock(&mutex_cola_eliminacion);
    procesos_en_ram--;
    pthread_mutex_unlock(&mutex_cola_eliminacion);

    checkear_pasaje_a_ready();
}

void cambiar_de_blocked_a_exit(pcb *pcb){
    queue_push(cola_exit, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "BLOCKED";
    list_remove_element(cola_blocked->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    
    pthread_mutex_lock(&mutex_cola_eliminacion);
    procesos_en_ram--;
    pthread_mutex_unlock(&mutex_cola_eliminacion);

    checkear_pasaje_a_ready();

    liberar_recursos(pcb->contexto->PID, pcb->contexto->motivo);
}

void cambiar_de_new_a_exit(pcb *pcb){
    queue_push(cola_exit, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "NEW";
    list_remove_element(cola_new->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    
    pthread_mutex_lock(&mutex_cola_eliminacion);
    procesos_en_ram--;
    pthread_mutex_unlock(&mutex_cola_eliminacion);

    checkear_pasaje_a_ready();
}

void cambiar_de_blocked_a_resourse_blocked(pcb *pcb, char* name_recurso){
    bool es_t_recurso_buscado_aux (void *data){
        return es_t_recurso_buscado(name_recurso, data);
    };

    t_recurso* recurso = list_find(recursos, es_t_recurso_buscado_aux);

    queue_push(recurso->procesos_bloqueados, (void *)pcb);
    pcb->estadoActual = "RESOURSE_BLOCKED";
    pcb->estadoAnterior = "BLOCKED";
    list_remove_element(cola_blocked->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_resourse_blocked_a_ready_prioridad(pcb *pcb, char* name_recurso){
    bool es_t_recurso_buscado_aux (void *data){
        return es_t_recurso_buscado(name_recurso, data);
    };

    t_recurso* recurso = list_find(recursos, es_t_recurso_buscado_aux);

    queue_push(cola_ready_prioridad, (void *)pcb);
    pcb->estadoActual = "READY_PRIORIDAD";
    pcb->estadoAnterior = "RESOURSE_BLOCKED";
    queue_pop(recurso->procesos_bloqueados);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_resourse_blocked_a_ready(pcb *pcb, char* name_recurso){
    bool es_t_recurso_buscado_aux (void *data){
        return es_t_recurso_buscado(name_recurso, data);
    };

    t_recurso* recurso = list_find(recursos, es_t_recurso_buscado_aux);

    queue_push(cola_ready, (void *)pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "RESOURSE_BLOCKED";
    queue_pop(recurso->procesos_bloqueados);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);

    pcb->contexto->quantum = quantum_krn;
}

void cambiar_de_resourse_blocked_a_exit(pcb *pcb, char* name_recurso){
    bool es_t_recurso_buscado_aux (void *data){
        return es_t_recurso_buscado(name_recurso, data);
    };

    t_recurso* recurso = list_find(recursos, es_t_recurso_buscado_aux);

    queue_push(cola_exit, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "RESOURSE_BLOCKED";
    list_remove_element(recurso->procesos_bloqueados->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    
    pthread_mutex_lock(&mutex_cola_eliminacion);
    procesos_en_ram--;
    pthread_mutex_unlock(&mutex_cola_eliminacion);

    checkear_pasaje_a_ready();
}

void cambiar_de_blocked_a_ready_prioridad_first(pcb *pcb){
    list_add_in_index(cola_ready_prioridad->elements, 0, pcb);
    pcb->estadoActual = "READY_PRIORIDAD";
    pcb->estadoAnterior = "BLOCKED";
    list_remove_element(cola_blocked->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_blocked_a_ready_first(pcb *pcb ){
    list_add_in_index(cola_ready->elements, 0, pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "BLOCKED";
    list_remove_element(cola_blocked->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void checkear_pasaje_a_ready(){
    if (procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_new))
    {
        pthread_mutex_lock(&mutex_cola_new);
        
        paquete_guardar_en_memoria(conexion_memoria, proceso_creado);
        sem_wait(&sem_permiso_memoria);
        
        if(flag_pasaje_ready){
            cambiar_de_new_a_ready(proceso_creado);
            flag_pasaje_ready = false;
        }

        pthread_mutex_unlock(&mutex_cola_new);
    }
}

int procesos_bloqueados_en_recursos(){
    int procesos_bloqueados;

    for(int i = 0; i < list_size(recursos); i++){
        t_recurso* recurso = list_get(recursos, i);

        procesos_bloqueados += queue_size(recurso->procesos_bloqueados);
    }

    return procesos_bloqueados;
}

// ---------------------------------------- INTERFACES  ---------------------------------------

// VALIDA QUE COMPLUA LAS OPERACIONES
bool lista_validacion_interfaces(INTERFAZ *interfaz, char *solicitud){
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

INTERFAZ *interfaz_encontrada(char *nombre){
    bool es_nombre_de_interfaz_aux(void *data)
    {
        return es_nombre_de_interfaz(nombre, data);
    };

    return (INTERFAZ *)list_find(interfaces, es_nombre_de_interfaz_aux);
}

// VALIDA QUE EXISTA LA INTERFAZ
bool lista_seek_interfaces(char *nombre){
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
    nueva_interfaz->sockets = malloc(sizeof(DATOS_CONEXION));
    nueva_interfaz->sockets = list_get(lista, 1);
    nueva_interfaz->sockets->nombre = list_get(lista, 2);
    nueva_interfaz->datos = list_get(lista, 3);
    nueva_interfaz->datos->operaciones = list_get(lista, 4);
    

    nueva_interfaz->procesos_bloqueados = queue_create();
    
    int j = 0;
    for (int i = 5; i < list_size(lista); i++){
        nueva_interfaz->datos->operaciones[j] = strdup((char*)list_get(lista, i));
        j++;
    }

    nueva_interfaz->estado = LIBRE;
    return nueva_interfaz;
}

void checkear_estado_interfaz(INTERFAZ* interfaz, pcb* pcb){
    pthread_mutex_lock(&mutex_intefaces);
    switch (interfaz->estado)
    {
    case OCUPADA:
        log_error(logger_kernel, "-INTERFAZ BLOQUEADA-\n");
        cambiar_de_execute_a_blocked_io(pcb, interfaz);
        guardar_solicitud_a_io(interfaz_solicitada);
        break;
    case LIBRE:
        log_info(logger_kernel, "Bloqueando interfaz...\n");
        interfaz->estado = OCUPADA;
        interfaz->proceso_asignado = pcb->contexto->PID;
        cambiar_de_execute_a_blocked_io(pcb, interfaz);
        guardar_solicitud_a_io(interfaz_solicitada);
        enviar_solicitud_io(interfaz->sockets->cliente_fd, interfaz_solicitada, determinar_operacion_io(interfaz));
        break;
    }
    pthread_mutex_unlock(&mutex_intefaces);
}

void desocupar_io(INTERFAZ* io_a_desbloquear){
    pthread_mutex_lock(&mutex_intefaces);

    io_a_desbloquear->estado = LIBRE;

    log_info(logger_kernel, "Se desbloqueo la interfaz %s.\n", io_a_desbloquear->sockets->nombre);

    if(!queue_is_empty(io_a_desbloquear->procesos_bloqueados)){
        pcb* pcb = queue_peek(io_a_desbloquear->procesos_bloqueados);

        bool es_solicitud_de_pid_aux(void* data){
            return es_solicitud_de_pid(pcb->contexto->PID, data);
        };

        SOLICITUD_INTERFAZ* solicitud = list_find(solicitudes, es_solicitud_de_pid_aux);

        enviar_solicitud_io(io_a_desbloquear->sockets->cliente_fd, solicitud, determinar_operacion_io(io_a_desbloquear));
    }

    pthread_mutex_unlock(&mutex_intefaces);
}

bool es_solicitud_de_pid(int PID, void* data){
    SOLICITUD_INTERFAZ* a_realizar = (SOLICITUD_INTERFAZ*)data;
    return atoi(a_realizar->pid) == PID;
}

void liberar_solicitud_de_desbloqueo(desbloquear_io *solicitud){
    free(solicitud->nombre);
    solicitud->nombre = NULL;
    free(solicitud->pid);
    solicitud->pid = NULL;
    free(solicitud);
    solicitud = NULL;
}

// ---------------------------------------- GESTION LLEGADAS -----------------------------------------

void *gestionar_llegada_kernel_cpu(void *args){
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
        case USER_INTERRUPTED:
            pthread_mutex_lock(&mutex_contexto);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            contexto_recibido = list_get(lista, 0);
            contexto_recibido->registros = list_get(lista, 1);
            contexto_recibido->motivo = INTERRUPTED;
            pthread_mutex_unlock(&mutex_contexto);
            sem_post(&recep_contexto);
            break;
        case INTERRUPCION:
            pthread_mutex_lock(&mutex_contexto);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            contexto_recibido = list_get(lista, 0);
            contexto_recibido->registros = list_get(lista, 1);
            contexto_recibido->motivo = QUANTUM;
            pthread_mutex_unlock(&mutex_contexto);
            sem_post(&recep_contexto);
            break;
        case CONTEXTO:
            pthread_mutex_lock(&mutex_contexto);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            contexto_recibido = list_get(lista, 0);
            contexto_recibido->registros = list_get(lista, 1);
            contexto_recibido->motivo = FIN_INSTRUCCION;
            pthread_mutex_unlock(&mutex_contexto);
            sem_post(&recep_contexto);
            break;
        case SOLICITUD_IO:
            pthread_mutex_lock(&mutex_contexto);
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
            pthread_mutex_unlock(&mutex_contexto);
            sem_post(&recep_contexto);
            break;
        case O_WAIT:
            pthread_mutex_lock(&mutex_contexto);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            contexto_recibido = list_get(lista, 0);
            contexto_recibido->registros = list_get(lista, 1);
            name_recurso = list_get(lista, 2);
            contexto_recibido->motivo = T_WAIT;
            pthread_mutex_unlock(&mutex_contexto);
            sem_post(&recep_contexto);
            break;
        case O_SIGNAL:
            pthread_mutex_lock(&mutex_contexto);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            contexto_recibido = list_get(lista, 0);
            contexto_recibido->registros = list_get(lista, 1);
            name_recurso = list_get(lista, 2);
            contexto_recibido->motivo = T_SIGNAL;
            pthread_mutex_unlock(&mutex_contexto);
            sem_post(&recep_contexto);
            break;
        case OUT_OF_MEMORY:
            pthread_mutex_lock(&mutex_contexto);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            contexto_recibido = list_get(lista, 0);
            contexto_recibido->registros = list_get(lista, 1);
            contexto_recibido->motivo = SIN_MEMORIA;
            pthread_mutex_unlock(&mutex_contexto);
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

void *gestionar_llegada_io_kernel(void *args){

    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;

    while (1){

        int cod_op = recibir_operacion(args_entrada->cliente_fd);

        switch (cod_op){  
        case DESCONECTAR_IO:
            break;

        case DESBLOQUEAR_PID:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            desbloquear_io *solicitud_entrante = list_get(lista, 0);
            solicitud_entrante->pid = list_get(lista, 1);
            solicitud_entrante->nombre = list_get(lista, 2);

            INTERFAZ* io_a_desbloquear = interfaz_encontrada(solicitud_entrante->nombre); 
            
            int id_proceso = atoi(solicitud_entrante->pid);
            
            pcb* pcb = buscar_pcb_en_cola(io_a_desbloquear->procesos_bloqueados, id_proceso);

            bool es_solicitud_de_pid_aux(void* data){
                return es_solicitud_de_pid(pcb->contexto->PID, data);
            };

            list_remove_and_destroy_by_condition(solicitudes, es_solicitud_de_pid_aux, eliminar_io_solicitada);

            if(pcb->contexto->quantum > 0 && !strcmp(tipo_de_planificacion, "VRR")){
                pthread_mutex_lock(&mutex_cola_blocked);
                cambiar_de_blocked_io_a_ready_prioridad(pcb, io_a_desbloquear);
                pthread_mutex_unlock(&mutex_cola_blocked);
            }else{
                pcb->contexto->quantum = quantum_krn;
                pthread_mutex_lock(&mutex_cola_blocked);
                cambiar_de_blocked_io_a_ready(pcb, io_a_desbloquear);
                pthread_mutex_unlock(&mutex_cola_blocked);
            }

            liberar_solicitud_de_desbloqueo(solicitud_entrante);
            break;

        case -1:
            log_error(args_entrada->logger, "%s se desconecto. Terminando servidor", args_entrada->nombre);
            buscar_y_desconectar(args_entrada->nombre, interfaces, logger_kernel);
            return (void *)EXIT_FAILURE;

        default:
            log_warning(args_entrada->logger, "Operacion desconocida. No quieras meter la pata");
            break;

        }
    }
}

void *esperar_nuevo_io(){

    while(1){

        INTERFAZ* interfaz_a_agregar;
        t_list *lista;

        int socket_io = esperar_cliente(server_kernel, logger_kernel);     
        int cod_op = recibir_operacion(socket_io);

        if(cod_op != NUEVA_IO){ /* ERROR OPERACION INVALIDA */ exit(-32); }

        lista = recibir_paquete(socket_io, logger_kernel);

        interfaz_a_agregar = asignar_espacio_a_io(lista);
        interfaz_a_agregar->sockets->cliente_fd = socket_io;

        ArgsGestionarServidor args_gestionar_servidor = {logger_interfaces, interfaz_a_agregar->sockets->cliente_fd, interfaz_a_agregar->sockets->nombre};
        pthread_create(&interfaz_a_agregar->sockets->hilo_de_llegada_kernel, NULL, gestionar_llegada_io_kernel, (void*)&args_gestionar_servidor);

        list_add(interfaces, interfaz_a_agregar);
        log_warning(logger_kernel, "Un %s salvaje ha aparecido en el camino \n", interfaz_a_agregar->sockets->nombre);
    }
}

void *gestionar_llegada_kernel_memoria(void *args){
    
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;

    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {

        case MENSAJE:
            recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
            break;

        case CREAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            proceso_creado = list_get(lista, 0);
            proceso_creado->recursos_adquiridos = list_get(lista, 1);
            proceso_creado->contexto = list_get(lista, 2);
            proceso_creado->contexto->registros = list_get(lista, 3);
            proceso_creado->contexto->registros->PTBR = list_get(lista, 4);
            sem_post(&creacion_proceso);
            break;

        case FINALIZAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            log_info(logger_kernel, "%s", (char*)list_get(lista, 0));
            sem_post(&finalizacion_proceso);
            break;

        case MEMORIA_ASIGNADA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            char* respuesta = list_get(lista, 0);
            int response = atoi(respuesta);
            
            if (response == 1) {
                flag_pasaje_ready = true;
            }
            sem_post(&sem_permiso_memoria);
            break;

        case TIEMPO_RESPUESTA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            char* tiempo = list_get(lista, 0);
            coef_interrupcion = atoi(tiempo);
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

// ---------------------------------------- INTERRUPCION POR QUANTUM ----------------------------------------

void abrir_hilo_interrupcion(int quantum_proceso){
    args_hilo_interrupcion args = {quantum_proceso};

    pthread_create(&interrupcion, NULL, interrumpir_por_quantum, (void*)&args);

    pthread_join(interrupcion, NULL);
}

void* interrumpir_por_quantum(void* args){
    args_hilo_interrupcion *args_del_hilo = (args_hilo_interrupcion*)args;

    int i = 0;

    while(i < (args_del_hilo->tiempo_a_esperar - (coef_interrupcion / 2)) && !llego_contexto){
        usleep(250000);
        i += 250;
    }
    
    if(!llego_contexto){
        paqueteDeMensajes(conexion_cpu_interrupt, "Fin de Quantum", INTERRUPCION);

        log_warning(logger_kernel, "SYSCALL INCOMING...");
    }
        

    return NULL;
}

int redondear_quantum(int tiempo){
    if(tiempo <= 0){
        return tiempo = 0;
    }else{
        return tiempo;
    }
}

// ---------------------------------------- RECURSOS ----------------------------------------

void llenar_lista_de_recursos(char** nombres_recursos, char** instancias_recursos, t_list* recursos) {
    int i = 0;

    while (nombres_recursos[i] != NULL && instancias_recursos[i] != NULL) {
        t_recurso *recurso = malloc(sizeof(t_recurso));
        recurso->nombre = strdup(nombres_recursos[i]);
        recurso->instancia = atoi(instancias_recursos[i]);
        recurso->procesos_bloqueados = queue_create();
        list_add(recursos, recurso);
        i++;
    } 

    log_info(logger_kernel, "-Recursos cargados-");
}

void eliminar_recursos(void* data){
    t_recurso* elemento = (t_recurso*)data;
    queue_destroy(elemento->procesos_bloqueados);
    free(elemento->nombre);
    elemento->nombre = NULL;
    free(elemento);
    elemento = NULL;
}

bool es_t_recurso_buscado(char* name_recurso, void* data) {
    t_recurso* recurso = (t_recurso*)data;
    return !strcmp(recurso->nombre, name_recurso);
}

bool es_p_recurso_buscado(char* name_recurso, void* data) {
    p_recurso* recurso = (p_recurso*)data;
    return !strcmp(recurso->nombre, name_recurso);
}

void asignar_instancia_recurso(pcb* proceso, char* name_recurso) {
    eliminarEspaciosBlanco(name_recurso);

    bool es_p_recurso_buscado_aux (void *data){
        return es_p_recurso_buscado(name_recurso, data);
    };

    bool es_t_recurso_buscado_aux (void *data){
        return es_t_recurso_buscado(name_recurso, data);
    };

    t_recurso* recurso = (t_recurso*)list_find(recursos, es_t_recurso_buscado_aux);

    if (recurso == NULL) {
        log_error(logger_kernel, "ERROR 404: Not found resourse <%s>\n", name_recurso);
        pthread_mutex_lock(&mutex_cola_blocked);
        cambiar_de_blocked_a_exit(proceso);
        pthread_mutex_unlock(&mutex_cola_blocked);
        return;
    }

    if(recurso->instancia <= 0){
        log_warning(logger_kernel, "\t-SIN INSTANCIAS DE RECURSOS %s-\n", recurso->nombre);
        pthread_mutex_lock(&mutex_cola_blocked);
        cambiar_de_blocked_a_resourse_blocked(proceso, name_recurso);
        pthread_mutex_unlock(&mutex_cola_blocked);
        return;
    }else{
        log_info(logger_kernel, "Asignando recurso solicitado...");
        
        recurso->instancia -= 1;
        
        if(!list_is_empty(proceso->recursos_adquiridos)){
            if(proceso_posee_recurso(proceso, name_recurso)){
                p_recurso* recurso_encontrado = (p_recurso*)list_find(proceso->recursos_adquiridos, es_p_recurso_buscado_aux);
                recurso_encontrado->instancia += 1;
            }else{
                p_recurso* recurso_copia = malloc(sizeof(p_recurso));
                recurso_copia->nombre = strdup(recurso->nombre);
                recurso_copia->instancia = 1;
                list_add(proceso->recursos_adquiridos, recurso_copia);
            }
        }else{
            p_recurso* recurso_copia = malloc(sizeof(p_recurso));
            recurso_copia->nombre = strdup(recurso->nombre);
            recurso_copia->instancia = 1;
            list_add(proceso->recursos_adquiridos, recurso_copia);
        }

        if(determinar_planificacion(tipo_de_planificacion) == ALG_VRR && proceso->contexto->quantum > 0){
            if(buscar_pcb_en_cola(cola_blocked, proceso->contexto->PID) != NULL){
                pthread_mutex_lock(&mutex_cola_blocked);
                cambiar_de_blocked_a_ready_prioridad(proceso);
                pthread_mutex_unlock(&mutex_cola_blocked);
            }else{
                cambiar_de_resourse_blocked_a_ready_prioridad(proceso, name_recurso);
            }
        }else{
            if(buscar_pcb_en_cola(cola_blocked, proceso->contexto->PID) != NULL){
                pthread_mutex_lock(&mutex_cola_blocked);
                cambiar_de_blocked_a_ready(proceso);
                pthread_mutex_unlock(&mutex_cola_blocked);
            }else{
                cambiar_de_resourse_blocked_a_ready(proceso, name_recurso);
            }
        }
    }
}

void liberar_instancia_recurso(pcb* proceso, char* name_recurso) {
    eliminarEspaciosBlanco(name_recurso);

    bool es_p_recurso_buscado_aux (void *data){
        return es_p_recurso_buscado(name_recurso, data);
    };

    bool es_t_recurso_buscado_aux (void *data){
        return es_t_recurso_buscado(name_recurso, data);
    };

    t_recurso* recurso = (t_recurso*)list_find(recursos, es_t_recurso_buscado_aux);

    if (recurso == NULL || !proceso_posee_recurso(proceso, name_recurso)) {
        log_error(logger_kernel, "ERROR 404: Not found resourse <%s>", name_recurso);
        pthread_mutex_lock(&mutex_cola_blocked);
        cambiar_de_blocked_a_exit(proceso);
        pthread_mutex_unlock(&mutex_cola_blocked);
    }else{
        log_info(logger_kernel, "Liberando recurso solicitado...");
        
        p_recurso* recurso_encontrado = (p_recurso*)list_find(proceso->recursos_adquiridos, es_p_recurso_buscado_aux);
        
        recurso->instancia += 1;
        recurso_encontrado->instancia -= 1;
    
        if(recurso_encontrado->instancia == 0){
            list_remove_and_destroy_by_condition(proceso->recursos_adquiridos, es_p_recurso_buscado_aux, limpiar_recurso);
        }

        if(determinar_planificacion(tipo_de_planificacion) == ALG_VRR && proceso->contexto->quantum > 0){
            pthread_mutex_lock(&mutex_cola_blocked);
            cambiar_de_blocked_a_ready_prioridad_first(proceso);
            pthread_mutex_unlock(&mutex_cola_blocked);
        }else{ 
            pthread_mutex_lock(&mutex_cola_blocked);
            cambiar_de_blocked_a_ready(proceso);
            pthread_mutex_unlock(&mutex_cola_blocked);
        }

        log_info(logger_kernel, "-Recurso liberado correctamente-");

        if(!queue_is_empty(recurso->procesos_bloqueados)){
            pcb* a_desbloquear = queue_peek(recurso->procesos_bloqueados);
            
            asignar_instancia_recurso(a_desbloquear, name_recurso);
        }
    }
}

void liberar_todos_recursos_asignados(pcb* a_eliminar){
    if(!list_is_empty(a_eliminar->recursos_adquiridos)){
        for(int i = 0; i < list_size(a_eliminar->recursos_adquiridos); i++){
            p_recurso* recurso_encontrado = (p_recurso*)list_get(a_eliminar->recursos_adquiridos, i);

            bool es_t_recurso_buscado_aux (void *data){
                return es_t_recurso_buscado(recurso_encontrado->nombre, data);
            };

            bool es_p_recurso_buscado_aux (void *data){
                return es_p_recurso_buscado(recurso_encontrado->nombre, data);
            };
            
            t_recurso* recurso = (t_recurso*)list_find(recursos, es_t_recurso_buscado_aux);

            while(recurso_encontrado->instancia != 0){
                recurso_encontrado->instancia -= 1;
                recurso->instancia += 1;
            }

            pthread_mutex_lock(&mutex_recursos);
            asignar_instancia_recurso(queue_peek(recurso->procesos_bloqueados), recurso->nombre);
            pthread_mutex_unlock(&mutex_recursos);

            list_remove_and_destroy_by_condition(a_eliminar->recursos_adquiridos, es_p_recurso_buscado_aux, limpiar_recurso);
        }
    }
}

bool proceso_posee_recurso(pcb* proceso, char* nombre_recurso){
    if (!list_is_empty(proceso->recursos_adquiridos))
    {
        for(int i = 0; i < list_size(proceso->recursos_adquiridos); i++)
        {
            p_recurso *elemento_actual = list_get(proceso->recursos_adquiridos, i);

            if (elemento_actual != NULL && !strcmp(elemento_actual->nombre, nombre_recurso))
            {
                return true;
            }
        }
    }
    return false;
}

void limpiar_recurso(void* data){
    p_recurso* recurso_encontrado = (p_recurso*)data;
    free(recurso_encontrado->nombre);
    recurso_encontrado->nombre = NULL;
    free(recurso_encontrado);
    recurso_encontrado = NULL;
}

void guardar_solicitud_a_io(SOLICITUD_INTERFAZ* interfaz_solicitada){
    SOLICITUD_INTERFAZ* solicitud = malloc(sizeof(SOLICITUD_INTERFAZ));
    solicitud->nombre = strdup(interfaz_solicitada->nombre);
    solicitud->pid = strdup(interfaz_solicitada->pid);
    solicitud->solicitud = strdup(interfaz_solicitada->solicitud);
    
    int cantidad_argumentos = sizeof(interfaz_solicitada->args) / sizeof(interfaz_solicitada->args[0]);
    solicitud->args = malloc(sizeof(interfaz_solicitada->args));

    for(int i=0; i < cantidad_argumentos; i++){
        solicitud->args[i] = strdup(interfaz_solicitada->args[i]);
    }
    list_add(solicitudes,solicitud);
}