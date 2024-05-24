#include <stdlib.h>
#include <stdio.h>
#include <io_generica.h>

int conexion_kernel;
int id_nombre=0;

t_log* logger_io_generica;
t_log* logger_stdin;
t_log* logger_stdout;
t_log* logger_dialfs;

t_config* config_generica;
t_config* config_stdin;
t_config* config_stdout;
t_config* config_dialfs;

t_list* interfaces;

pthread_t hilo_interfaz;
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

void iniciar_interfaz(char* nombre, t_config* config){
    INTERFAZ* interfaz = malloc(sizeof(INTERFAZ));
    interfaz->name = nombre;
    interfaz->configuration= config;
    interfaz->tipo= get_tipo_interfaz(config_get_string_value(config,"TIPO_INTERFAZ"));

    // CREAR HILO QUE CORRA LA INTERFAZ Y LO AGREGAMOS A UNA LISTA

    argumentos_correr_io args = {interfaz};
    
    pthread_create(&hilo_interfaz, NULL, correr_interfaz, (void*)&args);
    list_add(interfaces,interfaz);
}


// Esta función es la q va a correr en el hilo creado en iniciar_interfaz(), y tenemos que hacer q se conecte a kernel
void* correr_interfaz(void* args){
    //ESTE PUEDE SER EL ERROR, 
    argumentos_correr_io* argumentos = (argumentos_correr_io*)args;
    char* ip_kernel = config_get_string_value(argumentos->interfaz->configuration, "IP_KERNEL");
    char* puerto_kernel = config_get_string_value(argumentos->interfaz->configuration, "PUERTO_KERNEL");
    //ESTE PUEDE SER EL ERROR
    
//    char* ip_kernel= config_get_string_value(args->interfaz->configuration,"IP_KERNEL");
//    char* puerto_kernel= config_get_string_value(args->interfaz->configuration,"PUERTO_KERNEL");
    // conectar interfaz al kernel
    int conexion_kernel= crear_conexion(ip_kernel,puerto_kernel);
    // espera a que kernel le mande una peticion
    gestionar_peticion_kernel();    // TODO esta función va a ser el while(1){ recibir_operacion(); switch() con los casos q correspondan}
    // recibe una operacion (esto probablemente esté incluido en la función de arriba, y dentro de la misma lo mandariamos tambien a q resuelva la peticion)
    log_info(logger_io_generica,"operacion que kernel me mandó");
    // atiende la peticion de kernel
}

int main(int argc, char* argv[]) {
    //conexion_memoria;
    char* ip_kernel; //*ip_memoria;
    char* puerto_kernel; //*puerto_memoria;

    interfaces=list_create();

    logger_io_generica = iniciar_logger("io_gen.log", "io_gen_log", LOG_LEVEL_INFO);
    logger_stdin = iniciar_logger("io_stdin.log", "io_stdin_log", LOG_LEVEL_INFO);
    logger_stdout = iniciar_logger("io_stdout.log", "io_stdout_log", LOG_LEVEL_INFO);
    logger_dialfs = iniciar_logger("io_dialfs.log", "io_dialfs_log", LOG_LEVEL_INFO);
    
    config_generica = iniciar_config("io_generica.config");
    config_stdin = iniciar_config("io_stdin.config");
    config_stdout = iniciar_config("io_stdout.config");
    config_dialfs = iniciar_config("io_dialfs.config");
    
    char* mensaje_para_kernel = "Se ha conectado la interfaz\n";
    enviar_operacion(mensaje_para_kernel, conexion_kernel, MENSAJE);
    log_info(logger_io_generica, "Mensajes enviados exitosamente");

    ArgsGestionarServidor args_cliente = {logger_io_generica, conexion_kernel};

    gestionar_llegada((void*)&args_cliente);
   
    // Liberamos los hilos de cada interfaz de la lista al cerrar el programa


    while(!list_is_empty(interfaces)){
        INTERFAZ* aux;
        aux=list_remove(interfaces,0);
        pthread_join(aux->hilo, NULL);
        free(aux);
    }

    liberar_conexion(conexion_kernel);           
    terminar_programa(logger_io_generica, config_generica);
    terminar_programa(logger_stdin, config_stdin);
    terminar_programa(logger_stdout, config_stdout);
    terminar_programa(logger_dialfs, config_dialfs);
    return 0;
}