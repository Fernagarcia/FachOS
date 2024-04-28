#include <kernel.h>

//TODO Desarrollar las funciones 

int ejecutar_script(char*){
    printf("Hola mundo");
    return 0;
}
int iniciar_proceso(char*){
    printf("Hola mundo");
    return 0;
}
int finalizar_proceso(int){
    printf("Hola mundo");
    return 0;
}
int iniciar_planificacion(){
    printf("Hola mundo");
    return 0;
}
int detener_planificacion(){
    printf("Hola mundo");
    return 0;
}
int multiprogramacion(int){
    printf("Hola mundo");
    return 0;
}
int proceso_estado(){
    printf("Hola mundo");
    return 0;
}

void* leer_consola(void* args){
	ArgsLeerConsola* args_lectura = (ArgsLeerConsola*)args;
    
    log_info(args_lectura->logger, "CONSOLA INTERACTIVA DE KERNEL\n Ingrese comando a ejecutar...");

    char* leido, *s;

	while (1)
	{
		leido = readline("> ");

        if (strncmp(leido, "EXIT", 4) == 0) {
            free(leido);
            break;
        }else{
            s = stripwhite(leido);
            if(*s)
            {
                add_history(s);
                execute_line(s, args_lectura->logger);
            }
            log_info(args_lectura->logger, leido);
            free (leido);
        }	
	}
}

void* inicializar_servidor(void* args){
    args_inicializar_servidor* args_sv = (args_inicializar_servidor*)args;

    log_info(args_sv->logger, "Servidor listo para recibir al cliente");
	int cliente_fd = esperar_cliente(args_sv->sv_kernel, args_sv->logger);

    ArgsGestionarServidor args_servidor = {args_sv->logger, cliente_fd};

    gestionar_llegada((void*)&args_servidor);
}

int main(int argc, char* argv[]) {
    int conexion_memoria, conexion_cpu, i;

    char* ip_cpu, *ip_memoria;
    char* puerto_cpu_dispatch, *puerto_memoria;

    char* path_config = "../kernel/kernel.config";
    char* puerto_escucha;

    pthread_t id_hilo[2];

    // CREAMOS LOG Y CONFIG
    t_log* logger_kernel = iniciar_logger("kernel.log", "kernel-log", LOG_LEVEL_INFO);
    log_info(logger_kernel, "Logger Creado.");

    t_config* config = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    ip_cpu = config_get_string_value(config, "IP_CPU");
    puerto_cpu_dispatch = config_get_string_value(config, "PUERTO_CPU_DISPATCH");
    ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");

    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE CPU", ip_cpu, puerto_cpu_dispatch);
    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);

    int server_kernel = iniciar_servidor(logger_kernel, puerto_escucha);
    args_inicializar_servidor args_sv = {logger_kernel, server_kernel};
    pthread_create(&id_hilo[0], NULL, inicializar_servidor, (void*)&args_sv);

    //CONEXIONES
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    conexion_cpu = crear_conexion(ip_cpu, puerto_cpu_dispatch);

    //MENSAJES
    
    enviar_mensaje("Hola CPU", conexion_cpu);
    paqueteDeMensajes(conexion_cpu);

    enviar_mensaje("Hola MEMORIA", conexion_memoria);
    paqueteDeMensajes(conexion_memoria);
    
    ArgsLeerConsola args_consola = {logger_kernel};
    pthread_create(&id_hilo[1], NULL, leer_consola, (void*)&args_consola);

    for(i = 0; i<2; i++){
        pthread_join(id_hilo[i], NULL);
    }
    
    terminar_programa(logger_kernel, config);
    liberar_conexion(conexion_cpu);
    liberar_conexion(conexion_memoria);

    return 0;
}
