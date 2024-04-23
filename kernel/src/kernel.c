#include <kernel.h>
// TODO es una idea de como podria ser...
int idProceso=0;
PCB iniciarProceso(t_config* config){
    PCB pcb;
    pcb.PID=idProceso;
    pcb.quantum=config_get_int_value(config,"QUANTUM");
    //pcb.contextoDeEjecucion=NULL; no se sabe si lo recibe o lo ponemos xq esta en el path
    pcb.estado=NEW;
    idProceso++;
    return pcb;
}
    void FIFO(){
            paquetePCB(queue_pop(colaReady)->contextoDeEjecucion);
    }
    void RR(PCB proceso){
        paquetePCB(queue_pop(colaReady)->contextoDeEjecucion);
        if(proceso.quantum=NULL){//ni idea cual seria el tiempo de ejecucuion o rafaga que deberia comparar

        }
    }
    void procesoReady(PCB proceso, t_queue colaReady){
        if (proceso.estado==1){
            queue_push(colaReady,proceso);
        }
    }

// TODO se podria hacer mas simple pero es para salir del paso <3 (por ejemplo que directamente se pase la funcion)
    void planificadorCortoPlazo(PCB proceso,char* tipo){
        t_queue* colaReady=queue_create();
        procesoReady(proceso,colaReady);

        if(tipo=="FIFO"){
            FIFO(proceso);
        }else if(tipo=="RR"){
            RR(proceso,quantum);
        }
   }

int main(int argc, char* argv[]) {
    int conexion_memoria, conexion_cpu;

    char* ip_cpu, *ip_memoria;
    char* puerto_cpu_dispatch, *puerto_memoria;

    char* path_config = "../kernel/kernel.config";
    char* puerto_escucha;

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

    //TODO VER IMPLEMENTACION DE HILOS PARA PODER HACER LAS ACCIONES EN SIMULTANEO
    conexion_cpu = crear_conexion(ip_cpu, puerto_cpu_dispatch);
    enviar_mensaje("Hola CPU :)", conexion_cpu);
    paquete(conexion_cpu);

    /*conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_mensaje("Hola MEMORIA", conexion_memoria);
    paquete(conexion_memoria);*/

    int server_kernel = iniciar_servidor(logger_kernel, puerto_escucha);

    while(1){
        gestionar_llegada(logger_kernel, server_kernel);
        log_info(logger_kernel, "Mensajes recibidos exitosamente");
    }
    
    liberar_conexion(conexion_memoria);
    liberar_conexion(conexion_cpu);
    terminar_programa(logger_kernel, config);
    return 0;
}
