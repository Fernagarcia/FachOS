#include <kernel.h>

int conexion_memoria;

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

int main(int argc, char* argv[]) {
    int i;

    char* ip_cpu, *ip_memoria;
    char* puerto_cpu_dispatch, *puerto_cpu_interrupt, *puerto_memoria;

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
    puerto_cpu_interrupt = config_get_string_value(config, "PUERTO_CPU_INTERRUPT");
    ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");

    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE CPU", ip_cpu, puerto_cpu_dispatch);
    log_info(logger_kernel, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);

    int server_kernel = iniciar_servidor(logger_kernel, puerto_escucha);
    log_info(logger_kernel, "Servidor listo para recibir al cliente");
    
    //CONEXIONES
    int conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_mensaje("KERNEL LLEGO A LA CASA MAMIIII", conexion_memoria);
    int conexion_cpu_dispatch = crear_conexion(ip_cpu, puerto_cpu_dispatch);
    enviar_mensaje("KERNEL LLEGO A LA CASA MAMIIII", conexion_cpu_dispatch);
    int conexion_cpu_interrupt = crear_conexion(ip_cpu, puerto_cpu_interrupt);
    enviar_mensaje("KERNEL LLEGO A LA CASA MAMIIII", conexion_cpu_interrupt);
	int cliente_fd = esperar_cliente(server_kernel, logger_kernel);

    log_info(logger_kernel, "Conexiones con modulos establecidas");

    ArgsGestionarServidor args_sv = {logger_kernel, cliente_fd};
    pthread_create(&id_hilo[0], NULL, gestionar_llegada, (void*)&args_sv);
    
    ArgsLeerConsola args_consola = {logger_kernel};
    pthread_create(&id_hilo[1], NULL, leer_consola, (void*)&args_consola);

    for(i = 0; i<3; i++){
        pthread_join(id_hilo[i], NULL);
    }
    
    terminar_programa(logger_kernel, config);
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
int iniciar_proceso(char* path_instrucciones){
    enviar_mensaje(path_instrucciones, conexion_memoria);
    return 0;
}
int finalizar_proceso(char* param){
    int number = atoi(param);
    printf("Number: %d", number);
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
int multiprogramacion(char*){
    printf("Hola mundo");
    return 0;
}
int proceso_estado(){
    printf("Hola mundo");
    return 0;
}