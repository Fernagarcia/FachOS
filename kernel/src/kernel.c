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

bool llego_contexto;
bool flag_interrupcion;

char* tipo_de_planificacion;
char* name_recurso;

t_list *interfaces;
t_list *recursos;

// COLAS DE ESTADO

t_queue *cola_new;
t_queue *cola_ready;
t_queue *cola_ready_prioridad;
t_queue *cola_running;
t_queue *cola_blocked;
t_queue *cola_exit;

// COLAS DE INTERFACES

t_queue *io_generica;
t_queue *io_stdin;
t_queue *io_stdout;
t_queue *io_dial_fs;

t_log *logger_kernel;
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

pthread_t planificacion;
pthread_t interrupcion;

sem_t sem_planif;
sem_t recep_contexto;
sem_t creacion_proceso;
sem_t finalizacion_proceso;
sem_t sem_permiso_memoria;

void *FIFO()
{
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
            paqueteDeMensajes(conexion_memoria, a_ejecutar->path_instrucciones, CARGAR_INSTRUCCIONES);

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
                asignar_instancia_recurso(a_ejecutar, name_recurso);
                break;
            case T_SIGNAL:
                log_info(logger_kernel_planif, "PID: %d - Libero recurso %s", a_ejecutar->contexto->PID, name_recurso);
                cambiar_de_execute_a_blocked(a_ejecutar);
                liberar_instancia_recurso(a_ejecutar, name_recurso);
                break;
            default:
                 if (lista_seek_interfaces(interfaz_solicitada->nombre))
                {
                    INTERFAZ *interfaz = interfaz_encontrada(interfaz_solicitada->nombre);
                    if (lista_validacion_interfaces(interfaz, interfaz_solicitada->solicitud))
                    {
                        log_info(logger_kernel_mov_colas, "Operacion correcta. Enseguida se realizara la petición.");
                        interfaz_solicitada ->pid = string_itoa(a_ejecutar->contexto->PID);
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
        
        if(flag_interrupcion){
            flag_interrupcion = false;
            return NULL;
        }
        
        sem_post(&sem_planif);
    }
    return NULL;
}

void *RR()
{
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
            paqueteDeMensajes(conexion_memoria, a_ejecutar->path_instrucciones, CARGAR_INSTRUCCIONES);

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
                asignar_instancia_recurso(a_ejecutar, name_recurso);
                break;
            case T_SIGNAL:
                log_info(logger_kernel_planif, "PID: %d - Libero recurso %s", a_ejecutar->contexto->PID, name_recurso);
                cambiar_de_execute_a_blocked(a_ejecutar);
                liberar_instancia_recurso(a_ejecutar, name_recurso);
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
        
        if(flag_interrupcion){
            flag_interrupcion = false;
            return NULL;
        }

        sem_post(&sem_planif);
    }
    return NULL;
}

void *VRR()
{
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
            paqueteDeMensajes(conexion_memoria, a_ejecutar->path_instrucciones, CARGAR_INSTRUCCIONES);

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
                asignar_instancia_recurso(a_ejecutar, name_recurso);
                break;
            case T_SIGNAL:
                log_info(logger_kernel_planif, "PID: %d - Liberara recurso %s", a_ejecutar->contexto->PID, name_recurso);
                cambiar_de_execute_a_blocked(a_ejecutar);
                liberar_instancia_recurso(a_ejecutar, name_recurso);
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

        if(flag_interrupcion){
            flag_interrupcion = false;
            return NULL;
        }

        sem_post(&sem_planif);
    }
}

void *leer_consola()
{
    log_info(logger_kernel, "\n\t\t-CONSOLA INTERACTIVA DE KERNEL-\n");
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
    cola_new = queue_create();
    cola_ready = queue_create();
    cola_ready_prioridad = queue_create();
    cola_running = queue_create();
    cola_blocked = queue_create();
    cola_exit = queue_create();

    io_dial_fs = queue_create();
    io_stdin = queue_create();
    io_stdout = queue_create();
    io_generica = queue_create();

    interfaces = list_create();
    recursos = list_create();

    pthread_t id_hilo[4];
    
    sem_init(&sem_planif, 1, 0);
    sem_init(&recep_contexto, 1, 0);
    sem_init(&creacion_proceso, 1, 0);
    sem_init(&finalizacion_proceso, 1, 0);

    logger_kernel = iniciar_logger("kernel.log", "kernel-log", LOG_LEVEL_INFO);
    logger_kernel_mov_colas = iniciar_logger("kernel_colas.log", "kernel_colas-log", LOG_LEVEL_INFO);
    logger_kernel_planif = iniciar_logger("kernel_planif.log", "kernel_planificacion-log", LOG_LEVEL_INFO);
    log_info(logger_kernel, "\n \t\t\t-INICIO LOGGER GENERAL- \n");
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

    int server_kernel = iniciar_servidor(logger_kernel, puerto_escucha);
    log_info(logger_kernel, "Servidor listo para recibir al cliente");

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
    pthread_create(&id_hilo[1], NULL, gestionar_llegada_kernel_memoria, (void *)&args_sv_memoria);

    ArgsGestionarServidor args_sv_io = {logger_kernel, cliente_fd};
    pthread_create(&id_hilo[2], NULL, gestionar_llegada_io_kernel, (void *)&args_sv_io);

    sleep(2);

    pthread_create(&id_hilo[3], NULL, leer_consola, NULL);
    pthread_join(id_hilo[3], NULL);

    for (int i = 0; i < 3; i++)
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
    queue_destroy(io_dial_fs);
    queue_destroy(io_stdout);
    queue_destroy(io_stdin);
    queue_destroy(io_generica);

    sem_destroy(&sem_planif);
    sem_destroy(&recep_contexto);
    sem_destroy(&creacion_proceso);
    sem_destroy(&finalizacion_proceso);

    pthread_mutex_destroy(&mutex_cola_blocked);
    pthread_mutex_destroy(&mutex_cola_eliminacion);
    pthread_mutex_destroy(&mutex_cola_ready);
    pthread_mutex_destroy(&mutex_cola_ready_prioridad);
    pthread_mutex_destroy(&mutex_recursos);

    terminar_programa(logger_kernel, config_kernel);
    
    liberar_conexion(conexion_cpu_interrupt);
    liberar_conexion(conexion_cpu_dispatch);
    liberar_conexion(conexion_memoria);

    return 0;
}

t_config* iniciar_configuracion(){
    printf("1. Cargar configuracion de ejemplo (VRR)\n");
    printf("2. Cargar configuracion de Prueba planificacion\n");
    printf("3. Cargar configuracion de Prueba deadlock\n");
    printf("4. Cargar configuracion de Prueba memoria-tlb\n");
    char* opcion_en_string = readline("Seleccione una opción: ");
    int opcion = atoi(opcion_en_string);
    free(opcion_en_string);

    switch (opcion)
        {
        case 1:
            log_info(logger_kernel, "Se cargo la configuracion de ejemplo correctamente");
            return iniciar_config("../kernel/configs/kernel_ejemplo.config");
        case 2:
            log_info(logger_kernel, "Se cargo la configuracion 2 correctamente");
            return iniciar_config("../kernel/configs/prueba_planificacion.config");
        case 3:
            log_info(logger_kernel, "Se cargo la configuracion 3 correctamente");
            return iniciar_config("../kernel/configs/prueba_deadlock.config");
        case 4:            
            log_info(logger_kernel, "Se cargo la configuracion 4 correctamente");
            return iniciar_config("../kernel/configs/prueba_memoria_tlb.config");
        default:
            log_info(logger_kernel, "Se cargo la configuracion de ejemplo correctamente");
            return iniciar_config("../kernel/configs/kernel_ejemplo.config");
        }
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
        sleep(1);
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

    pthread_mutex_lock(&mutex_cola_new);
    queue_push(cola_new, proceso_creado);

    log_info(logger_kernel_mov_colas, "Se creo el proceso n° %d en NEW", proceso_creado->contexto->PID);

    if (procesos_en_ram < grado_multiprogramacion)
    {
        paqueteMemoria(conexion_memoria, proceso_creado->path_instrucciones, proceso_creado->contexto->registros->PTBR);
        sem_wait(&sem_permiso_memoria);
        cambiar_de_new_a_ready(proceso_creado);
        printf("Se a podido asignar correctamente espacio en memoria para el proceso\n");
    }
    idProceso++;
    pthread_mutex_unlock(&mutex_cola_new);
    return 0;
}

int finalizar_proceso(char *PID)
{
    int pid = atoi(PID);

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
        pcb* a_eliminar = buscar_pcb_en_cola(cola_running, pid);
        a_eliminar->contexto->motivo = INTERRUPTED;
        cambiar_de_execute_a_exit(a_eliminar);
        return EXIT_SUCCESS;
    }
    else if (buscar_pcb_en_cola(cola_blocked, pid) != NULL)
    {   
        pthread_mutex_lock(&mutex_cola_blocked);
        cambiar_de_blocked_a_exit(buscar_pcb_en_cola(cola_blocked, pid));
        pthread_mutex_unlock(&mutex_cola_blocked);
    }
    else if (buscar_pcb_en_cola(cola_exit, pid) == NULL)
    {
        log_error(logger_kernel, "El PCB con PID n°%d no existe", pid);
        return EXIT_FAILURE;
    }else{
        for(int i = 0; i < list_size(recursos); i++){
            t_recurso* recurso = list_get(recursos, i);
            pcb* pcb = buscar_pcb_en_cola(recurso->procesos_bloqueados, pid);
            if(pcb != NULL){
                cambiar_de_resourse_blocked_a_exit(buscar_pcb_en_cola(cola_blocked, pid), recurso->nombre);
            }
        }
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
    log_warning(logger_kernel, "-Stopping planning-\nWait a second...");
    paqueteDeMensajes(conexion_cpu_interrupt, "detencion de la planificacion", INTERRUPCION);
    flag_interrupcion = true;
    pthread_join(planificacion, NULL);
    log_warning(logger_kernel, "-Planning stopped-\n");
    return 0;
}

int algoritmo_planificacion(char* algoritmo)
{
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

int multiprogramacion(char *g_multiprogramacion)
{
    grado_multiprogramacion = atoi(g_multiprogramacion);
    log_info(logger_kernel, "Multiprogramming level set to %d", grado_multiprogramacion);
    config_set_value(config_kernel, "GRADO_MULTIPROGRAMACION", g_multiprogramacion);
    return 0;
}

int proceso_estado()
{
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

    printf("IO_GEN Queue:\t");
    iterar_cola_e_imprimir(io_generica);
    printf("IO_STDIN Queue:\t");
    iterar_cola_e_imprimir(io_stdin);
    printf("IO_STDOUT Queue:\t");
    iterar_cola_e_imprimir(io_stdout);
    printf("IO_FS Queue:\t");
    iterar_cola_e_imprimir(io_dial_fs);

    printf("EXIT Queue:\t");
    iterar_cola_e_imprimir(cola_exit);
    return 0;
}

int interfaces_conectadas()
{
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

void iterar_lista_interfaces_e_imprimir(t_list *lista)
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
        printf(" ]\tInterfaces conectadas: %d\n", list_size(lista));
    }
    list_iterator_destroy(lista_a_iterar);
}

void iterar_lista_recursos_e_imprimir(t_list *lista)
{
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
    default:
        log_info(logger_kernel_mov_colas, "Finaliza el proceso n°%d - Motivo: INVALID_INTERFACE ", PID);
        break;
    }

    if(!list_is_empty(a_eliminar->recursos_adquiridos)){
        
        pthread_mutex_lock(&mutex_recursos);
        liberar_todos_recursos_asignados(a_eliminar);
        pthread_mutex_unlock(&mutex_recursos);

        list_destroy(a_eliminar->recursos_adquiridos);
    }

    peticion_de_eliminacion_espacio_para_pcb(conexion_memoria, a_eliminar, FINALIZAR_PROCESO);
    pthread_mutex_unlock(&mutex_cola_eliminacion);
    sem_wait(&finalizacion_proceso);

    return EXIT_SUCCESS;
}

bool es_igual_a(int id_proceso, void *data)
{
    pcb *elemento = (pcb *)data;
    return (elemento->contexto->PID == id_proceso);
}

// ---------------------------------------- CAMBIO DE COLA ----------------------------------------

void cambiar_de_new_a_ready(pcb *pcb)
{
    queue_push(cola_ready, (void *)pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "NEW";
    queue_pop(cola_new);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = total_procesos_en_ram();
}

void cambiar_de_ready_a_execute(pcb *pcb)
{
    queue_push(cola_running, (void *)pcb);
    pcb->estadoActual = "EXECUTE";
    pcb->estadoAnterior = "READY";
    queue_pop(cola_ready);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);

    checkear_pasaje_a_ready();
}

void cambiar_de_ready_prioridad_a_execute(pcb *pcb)
{
    queue_push(cola_running, (void *)pcb);
    pcb->estadoActual = "EXECUTE";
    pcb->estadoAnterior = "READY_PRIORIDAD";
    queue_pop(cola_ready_prioridad);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);

    checkear_pasaje_a_ready();
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

    pcb->contexto->quantum = quantum_krn;
}

void cambiar_de_blocked_a_ready_prioridad(pcb *pcb)
{
    queue_push(cola_ready_prioridad, (void *)pcb);
    pcb->estadoActual = "READY_PRIORIDAD";
    pcb->estadoAnterior = "BLOCKED";
    queue_pop(cola_blocked);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_execute_a_exit(pcb *PCB)
{
    queue_push(cola_exit, (void *)PCB);
    PCB->estadoActual = "EXIT";
    PCB->estadoAnterior = "EXECUTE";
    queue_pop(cola_running);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", PCB->contexto->PID, PCB->estadoAnterior, PCB->estadoActual);
    procesos_en_ram = total_procesos_en_ram();
    
    checkear_pasaje_a_ready();

    liberar_recursos(PCB->contexto->PID, PCB->contexto->motivo);
}

void cambiar_de_ready_a_exit(pcb *pcb)
{
    queue_push(cola_exit, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "READY";
    list_remove_element(cola_ready->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = total_procesos_en_ram();

    checkear_pasaje_a_ready();
}

void cambiar_de_blocked_a_exit(pcb *pcb)
{
    queue_push(cola_exit, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "BLOCKED";
    list_remove_element(cola_blocked->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = total_procesos_en_ram();

    checkear_pasaje_a_ready();

    liberar_recursos(pcb->contexto->PID, pcb->contexto->motivo);
}

void cambiar_de_new_a_exit(pcb *pcb)
{
    queue_push(cola_exit, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "NEW";
    list_remove_element(cola_new->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = total_procesos_en_ram();
    checkear_pasaje_a_ready();
}

void cambiar_de_blocked_a_resourse_blocked(pcb *pcb, char* name_recurso)
{
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

void cambiar_de_resourse_blocked_a_ready_prioridad(pcb *pcb, char* name_recurso)
{
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

void cambiar_de_resourse_blocked_a_ready(pcb *pcb, char* name_recurso)
{
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

void cambiar_de_resourse_blocked_a_exit(pcb *pcb, char* name_recurso)
{
    bool es_t_recurso_buscado_aux (void *data){
        return es_t_recurso_buscado(name_recurso, data);
    };

    t_recurso* recurso = list_find(recursos, es_t_recurso_buscado_aux);

    queue_push(cola_ready, (void *)pcb);
    pcb->estadoActual = "EXIT";
    pcb->estadoAnterior = "RESOURSE_BLOCKED";
    list_remove_element(recurso->procesos_bloqueados->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
    procesos_en_ram = total_procesos_en_ram();

    checkear_pasaje_a_ready();
}

void cambiar_de_blocked_a_ready_prioridad_first(pcb *pcb)
{
    list_add_in_index(cola_ready_prioridad->elements, 0, pcb);
    pcb->estadoActual = "READY_PRIORIDAD";
    pcb->estadoAnterior = "BLOCKED";
    list_remove_element(cola_blocked->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void cambiar_de_blocked_a_ready_first(pcb *pcb )
{
    list_add_in_index(cola_ready->elements, 0, pcb);
    pcb->estadoActual = "READY";
    pcb->estadoAnterior = "BLOCKED";
    list_remove_element(cola_blocked->elements, (void *)pcb);
    log_info(logger_kernel_mov_colas, "PID: %d - ESTADO ANTERIOR: %s - ESTADO ACTUAL: %s", pcb->contexto->PID, pcb->estadoAnterior, pcb->estadoActual);
}

void checkear_pasaje_a_ready()
{
    if (procesos_en_ram < grado_multiprogramacion && !queue_is_empty(cola_new))
    {
        pthread_mutex_lock(&mutex_cola_new);
        cambiar_de_new_a_ready(queue_peek(cola_new));
        pthread_mutex_unlock(&mutex_cola_new);
    }
}

int total_procesos_en_ram(){
    return queue_size(cola_ready) + queue_size(cola_blocked) + queue_size(cola_running) + queue_size(cola_ready_prioridad) 
            + queue_size(io_dial_fs) + queue_size(io_stdin) + queue_size(io_stdout) + queue_size(io_generica) + procesos_bloqueados_en_recursos();
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
    nueva_interfaz->solicitud = NULL;
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



// ---------------------------------------- GESTION LLEGADAS -----------------------------------------

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
        case O_WAIT:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            contexto_recibido = list_get(lista, 0);
            contexto_recibido->registros = list_get(lista, 1);
            name_recurso = list_get(lista, 2);
            contexto_recibido->motivo = T_WAIT;
            sem_post(&recep_contexto);
            break;
        case O_SIGNAL:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            contexto_recibido = list_get(lista, 0);
            contexto_recibido->registros = list_get(lista, 1);
            name_recurso = list_get(lista, 2);
            contexto_recibido->motivo = T_SIGNAL;
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
            
            if(pcb->contexto->quantum > 0 && !strcmp(tipo_de_planificacion, "VRR")){
                pthread_mutex_lock(&mutex_cola_blocked);
                cambiar_de_blocked_a_ready_prioridad(pcb);
                pthread_mutex_unlock(&mutex_cola_blocked);
            }else{
                pcb->contexto->quantum = quantum_krn;
                pthread_mutex_lock(&mutex_cola_blocked);
                cambiar_de_blocked_a_ready(pcb);
                pthread_mutex_unlock(&mutex_cola_blocked);
            }

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
            proceso_creado->recursos_adquiridos = list_get(lista, 2);
            proceso_creado->contexto = list_get(lista, 3);
            proceso_creado->contexto->registros = list_get(lista, 4);
            sem_post(&creacion_proceso);
            break;
        case FINALIZAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            sem_post(&finalizacion_proceso);
            break;
        case MEMORIA_ASIGNADA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_kernel);
            int response = list_get(lista, 0);
            //TODO: Ojo que esta devolviendo un valor basura de response
            printf("VALOR DEL RESPONSE: %d", response);

            if (response != -1) {
                sem_post(&sem_permiso_memoria);
            }
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

    while(i < (args_del_hilo->tiempo_a_esperar - 500) && !llego_contexto){
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

void eliminar_recursos(void* data)
{
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

    if(recurso->instancia < 0){
        log_warning(logger_kernel, "\t-SIN INSTANCIAS DE RECURSOS %s-\n", recurso->nombre);
        pthread_mutex_lock(&mutex_cola_blocked);
        cambiar_de_blocked_a_resourse_blocked(proceso, name_recurso);
        pthread_mutex_unlock(&mutex_cola_blocked);
        return;
    }else{
        log_info(logger_kernel, "Asignando recurso solicitado...");
        
        pthread_mutex_lock(&mutex_recursos);
        recurso->instancia -= 1;
        pthread_mutex_unlock(&mutex_recursos);
        
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
        
        pthread_mutex_lock(&mutex_recursos);
        recurso->instancia += 1;
        pthread_mutex_unlock(&mutex_recursos);

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