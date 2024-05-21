#include <stdlib.h>
#include <stdio.h>
#include <io_generica.h>

int conexion_kernel;
int id_nombre=0;

t_log* logger_io_generica;
t_config* config;
t_list* interfaces;
/* Planteo
    Opcion 1: Creamos la interfaz con el nombre y archivo que nos pasan (armamos un struct interfaz) y agregamos la interfaz a una lista (que estaría en el modulo IO),
    el modulo de IO recibe una peticion para una interfaz, la busca en la lista y le manda a una funcion intermedia la interfaz y la petición, y está función se ocupa
    de la lógica basandose en el archivo config de la interfaz. Para que pueda ejecutar la peticion en paralelo a otras, podemos usar threads dinamicosp para correr
    la funcion intermedia. CREO Q ES POR ACA... CREO Q NO ERA POR ACA: lei la parte de kernel sobre el manejo de interfaces, y creo q van a estar corriendo en un 
    thread, con un semaforo que espere a que le lleguen operaciones desde kernel, es decir q vamos x la opcion 2.
    Opcion 2: Cuando se usa iniciar_interaz(nombre,config) la corremos en un thread y le creamos un semaforo para las peticiones (y le hacemos un wait de ese semaforo),
    además creamos una struct interfaz con el nombre y el semaforo, y agregamos esta struct a una lista (variable globla del modulo IO). Cuando llega algo a 
    gestionar_llegada() (que esta corriendo en el main), si es una peticion para una interfaz, llega el nombre de la interfaz y la peticion (datos que tenga una 
    peticion), entonces buscamos en la lista por el nombre de interfaz, y hacemos un signal al semaforo asociado a esa interaz. NO PENSE BIEN LA SOLUCION 2m pero
    creo q es la q tenemos q desarrollar.
*/

enum TIPO_INTERFAZ get_tipo_interfaz(char* tipo_nombre){    
    enum TIPO_INTERFAZ tipo;
    if(!strcmp(tipo_nombre,"GENERICA")){        // revisar si esta bien usado el strcmp
        tipo= GENERICA;
    }else if(!strcmp(tipo_nombre, "STDIN")){
        tipo= STDIN;
    }else if(!strcmp(tipo_nombre, "STDOUT")){
        tipo= STDOUT;
    }else if(!strcmp(tipo_nombre, "DIAL_FS")){
        tipo= DIAL_FS;
    }
    return tipo;
}

void peticion_IO_GEN(char* peticion, t_config* config){
    int tiempo_a_esperar= atoi(peticion);
    // Faltaria usar los datos de config como el TIEMPO_UNIDAD_TRABAJO, pero en el tp no dice mucho sobre como se usa en la interfaz genérica.
    sleep(tiempo_a_esperar);

}

void iniciar_interfaz(t_config* config){
    pthread_t hilo_interfaz;
    INTERFAZ* interfaz= malloc(sizeof(INTERFAZ));
    interfaz->name= id_nombre;
    interfaz->configuration= config;
    interfaz->tipo= get_tipo_interfaz(config_get_string_value(config,"TIPO_INTERFAZ"));

    // CREAR HILO QUE CORRA LA INTERFAZ Y LO AGREGAMOS A UNA LISTA
    pthread_create(&hilo_interfaz,NULL,correr_interfaz,NULL);
    interfaz->hilo=&hilo_interfaz;
    list_add(interfaces,interfaz);
    id_nombre++;
}


// Esta función es la q va a correr en el hilo creado en iniciar_interfaz(), y tenemos que hacer q se conecte a kernel
void correr_interfaz(INTERFAZ* interfaz){
    char* ip_kernel= config_get_string_value(interfaz->configuration,"IP_KERNEL");
    char* puerto_kernel= config_get_string_value(interfaz->configuration,"PUERTO_KERNEL");
    // conectar interfaz al kernel
  
    // espera a que kernel le mande una peticion

    // recibe una operacion
    log_info(logger_io_generica,"operacion que kernel me mandó");
    // atiende la peticion de kernel
}

int main(int argc, char* argv[]) {
     //conexion_memoria;
    
    char* ip_kernel; //*ip_memoria;
    char* puerto_kernel; //*puerto_memoria;

    char* path_config = "../entradasalida/entradasalida.config";
    interfaces=list_create();
    logger_io_generica = iniciar_logger("entradasalida.log", "entradasalida_log", LOG_LEVEL_INFO);
    log_info(logger_io_generica, "Logger Creado. Esperando mensaje para enviar...");
    
    config = iniciar_config(path_config);
    ip_kernel = config_get_string_value(config, "IP_KERNEL");
	puerto_kernel = config_get_string_value(config, "PUERTO_KERNEL");
    //int unidadTiempo = config_get_int_value(config, "TIEMPO_UNIDAD_TRABAJO");
    //ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    //puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");

    conexion_kernel = crear_conexion(ip_kernel, puerto_kernel);
    log_info(logger_io_generica, "%s\n\t\t\t\t\t\t%s\t%s\t", "Se ha establecido la conexion con Kernel", ip_kernel, puerto_kernel);
    
    /*
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    log_info(logger_io_generica, "%s\n\t\t\t\t\t\t%s\t%s\t", "Se ha establecido la conexion con Memoria", ip_memoria, puerto_memoria);
    */

    char* mensaje_para_kernel = "Se ha conectado la interfaz\n";
    enviar_operacion(mensaje_para_kernel, conexion_kernel, MENSAJE);
    log_info(logger_io_generica, "Mensajes enviados exitosamente");

    ArgsGestionarServidor args_cliente = {logger_io_generica, conexion_kernel};

    gestionar_llegada((void*)&args_cliente);
    
    /*
    char* mensaje_para_memoria = "Se ha conectado la interfaz\n";
    enviar_operacion(mensaje_para_memoria, conexion_memoria, MENSAJE);
    log_info(logger_io_generica, "Mensaje enviado exitosamente");
    */
    
    //liberar_conexion(conexion_memoria); TODO: descomentar las lineas de memoria cuando sea necesario
    //liberamos los hilos de cada interfaz de la lista
    while(!list_is_empty(interfaces)){
        INTERFAZ aux=list_remove(interfaces,0);
        pthread_join(aux->hilo,NULL);
    }
    liberar_conexion(conexion_kernel);           
    terminar_programa(logger_io_generica, config);
    return 0;
}