#include <stdlib.h>
#include <stdio.h>
#include <io_generica.h>

int conexion_kernel;
int id_nombre = 0;

t_log *logger_io_generica;
t_log *logger_stdin;
t_log *logger_stdout;
t_log *logger_dialfs;

t_config *config_generica;
t_config *config_stdin;
t_config *config_stdout;
t_config *config_dialfs;

t_list *interfaces;
pthread_t hilo_interfaz;

const char *operaciones_gen[1] = {"IO_GEN_SLEEP"};
const char *operaciones_stdin[1] = {"IO_STDIN_READ"};
const char *operaciones_stdout[1] = {"IO_STDOUT_WRITE"};
const char *operaciones_dialfs[5] = {"IO_FS_CREATE", "IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE", "IO_FS_READ"};
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

TIPO_INTERFAZ get_tipo_interfaz(INTERFAZ *interfaz, char *tipo_nombre)
{
    TIPO_INTERFAZ tipo;
    if (!strcmp(tipo_nombre, "GENERICA"))
    { // revisar si esta bien usado el strcmp
        tipo = GENERICA;
    }
    else if (!strcmp(tipo_nombre, "STDIN"))
    {
        tipo = STDIN;
    }
    else if (!strcmp(tipo_nombre, "STDOUT"))
    {
        tipo = STDOUT;
    }
    else if (!strcmp(tipo_nombre, "DIAL_FS"))
    {
        tipo = DIAL_FS;
    }
    return tipo;
}

void copiar_operaciones(INTERFAZ *interfaz)
{
    int cantidad_operaciones;
    switch (interfaz->datos->tipo)
    {
    case GENERICA:
        cantidad_operaciones = sizeof(operaciones_gen) / sizeof(operaciones_gen[0]);
        for (int i = 0; i < cantidad_operaciones; i++)
        {
            interfaz->datos->operaciones[i] = strdup(operaciones_gen[i]);
        }
        break;
    case STDIN:
        cantidad_operaciones = sizeof(operaciones_stdin) / sizeof(operaciones_stdin[0]);
        for (int i = 0; i < cantidad_operaciones; i++)
        {
            interfaz->datos->operaciones[i] = strdup(operaciones_stdin[i]);
        }
        break;
    case STDOUT:
        cantidad_operaciones = sizeof(operaciones_stdout) / sizeof(operaciones_stdout[0]);
        for (int i = 0; i < cantidad_operaciones; i++)
        {
            interfaz->datos->operaciones[i] = strdup(operaciones_stdout[i]);
        }
        break;
    case DIAL_FS:
        cantidad_operaciones = sizeof(operaciones_dialfs) / sizeof(operaciones_dialfs[0]);
        for (int i = 0; i < cantidad_operaciones; i++)
        {
            interfaz->datos->operaciones[i] = strdup(operaciones_dialfs[i]);
        }
        break;
    default:
        break;
    }
}

void peticion_IO_GEN(char *peticion, t_config *config)
{
    int tiempo_a_esperar = atoi(peticion);
    // Faltaria usar los datos de config como el TIEMPO_UNIDAD_TRABAJO, pero en el tp no dice mucho sobre como se usa en la interfaz genérica.
    sleep(tiempo_a_esperar);
}

void iniciar_interfaz(char *nombre, t_config *config, t_log *logger)
{
    INTERFAZ *interfaz = malloc(sizeof(INTERFAZ));
    interfaz->configuration = config;

    interfaz->datos = malloc(sizeof(DATOS_INTERFAZ));
    interfaz->datos->nombre = strdup(nombre);
    interfaz->datos->tipo = get_tipo_interfaz(interfaz, config_get_string_value(config, "TIPO_INTERFAZ"));

    switch (interfaz->datos->tipo)
    {
    case GENERICA:
        interfaz->datos->operaciones = malloc(sizeof(operaciones_gen));
        break;
    case STDIN:
        interfaz->datos->operaciones = malloc(sizeof(operaciones_stdin));
        break;
    case STDOUT:
        interfaz->datos->operaciones = malloc(sizeof(operaciones_stdout));
        break;
    case DIAL_FS:
        interfaz->datos->operaciones = malloc(sizeof(operaciones_dialfs));
        break;
    default:
        break;
    }

    copiar_operaciones(interfaz);

    log_info(logger, "Nombre: %s, Tipo: %s", interfaz->datos->nombre, config_get_string_value(interfaz->configuration, "TIPO_INTERFAZ"));
    log_info(logger, "Operacion: %s", interfaz->datos->operaciones[0]);

    paquete_nueva_IO(conexion_kernel, interfaz);
}
// TODO: Al eliminar la interfaz, borrar Interfaz, el config y los strings de Interfaz->datos->nombre, y los de operacion

// TODO: Modificar para que cada hilo pueda establecer una conexion con kernel

/*argumentos_correr_io args = {interfaz};

pthread_create(&hilo_interfaz, NULL, correr_interfaz, (void*)&args);
pthread_detach(hilo_interfaz);*/

// list_add(interfaces,interfaz); TODO: ver necesidad

// Esta función es la q va a correr en el hilo creado en iniciar_interfaz(), y tenemos que hacer q se conecte a kernel
void *correr_interfaz(void *args)
{
    // ESTE PUEDE SER EL ERROR,
    argumentos_correr_io *argumentos = (argumentos_correr_io *)args;

    paquete_nueva_IO(conexion_kernel, argumentos->interfaz);
}

void operar_interfaz(INTERFAZ*)
{
    // TODO
}

void *gestionar_peticion_kernel(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    INTERFAZ* nueva_interfaz;
    t_list *lista;
    while (1)
    {
        log_info(args_entrada->logger, "Esperando operacion...");
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {

        case IO_GENERICA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_io_generica);
            nueva_interfaz = list_get(lista, 0);
            log_info(logger_io_generica, "LA INTERFAZ %s", nueva_interfaz->datos->nombre);
            peticion_IO_GEN(nueva_interfaz->datos->nombre, nueva_interfaz->configuration);
            break;
        case IO_STDIN:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_stdin);
            nueva_interfaz = list_get(lista, 0);
            log_info(logger_io_generica, "LA INTERFAZ %s", nueva_interfaz->datos->nombre);
            operar_interfaz(nueva_interfaz);
            break;
        case IO_STDOUT:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_stdout);
            nueva_interfaz = list_get(lista, 0);
            log_info(logger_io_generica, "LA INTERFAZ %s", nueva_interfaz->datos->nombre);
            operar_interfaz(nueva_interfaz);
            break;
        case IO_DIALFS:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_dialfs);
            nueva_interfaz = list_get(lista, 0);
            log_info(logger_io_generica, "LA INTERFAZ %s", nueva_interfaz->datos->nombre);
            operar_interfaz(nueva_interfaz);
            break;
        case -1:
            log_error(args_entrada->logger, "el cliente se desconecto. Terminando servidor");
            return (void*)EXIT_FAILURE;
        default:
            log_warning(args_entrada->logger, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    // conexion_memoria;
    char *ip_kernel;     //*ip_memoria;
    char *puerto_kernel; //*puerto_memoria;

    interfaces = list_create();

    pthread_t hilo_llegadas;

    t_log *entrada_salida = iniciar_logger("main.log", "io_general_log", LOG_LEVEL_INFO);
    ;
    logger_io_generica = iniciar_logger("io_generica.log", "io_generica_log", LOG_LEVEL_INFO);
    logger_stdin = iniciar_logger("io_stdin.log", "io_stdin_log", LOG_LEVEL_INFO);
    logger_stdout = iniciar_logger("io_stdout.log", "io_stdout_log", LOG_LEVEL_INFO);
    logger_dialfs = iniciar_logger("io_dialfs.log", "io_dialfs_log", LOG_LEVEL_INFO);

    config_generica = iniciar_config("io_generica.config");
    config_stdin = iniciar_config("io_stdin.config");
    config_stdout = iniciar_config("io_stdout.config");
    config_dialfs = iniciar_config("io_dialfs.config");

    ip_kernel = config_get_string_value(config_generica, "IP_KERNEL");
    puerto_kernel = config_get_string_value(config_generica, "PUERTO_KERNEL");

    conexion_kernel = crear_conexion(ip_kernel, puerto_kernel);
    log_info(entrada_salida, "%s\n\t\t\t\t\t\t%s\t%s\t", "Se ha establecido la conexion con Kernel", ip_kernel, puerto_kernel);

    char *mensaje_para_kernel = "Se ha conectado la interfaz\n";
    enviar_operacion(mensaje_para_kernel, conexion_kernel, MENSAJE);
    log_info(entrada_salida, "Mensajes enviados exitosamente");

    ArgsGestionarServidor args_cliente = {entrada_salida, conexion_kernel};

    pthread_create(&hilo_llegadas, NULL, gestionar_llegada, (void *)&args_cliente);

    // Liberamos los hilos de cada interfaz de la lista al cerrar el programa

    sleep(1);
    // MENU PARA CREAR INTERFACES (PROVISIONAL?)

    int opcion;
    char *leido;

    printf("PUERTO DE CONEXIONES DE INTERFACES: ¿Que desea conectar?\n");
    while (opcion != 5)
    {

        printf("1. Conectar interfaz Generica \n");
        printf("2. Conectar interfaz STDIN \n");
        printf("3. Conectar interfaz STDOUT \n");
        printf("4. Conectar interfaz DIALFS \n");
        printf("5. Salir\n");
        printf("Seleccione una opción: ");
        scanf("%d", &opcion);

        switch (opcion)
        {
        case 1:
            printf("Conectando interfaz Generica...\n");
            printf("Ingresa el nombre de la interfaz Generica\n");
            leido = readline("> ");
            iniciar_interfaz(leido, config_generica, logger_io_generica);
            log_info(logger_io_generica, "Se creo la intefaz %s correctamente", leido);
            break;
        case 2:
            printf("Conectando interfaz STDIN...\n");
            printf("Ingresa el nombre de la interfaz STDIN\n");
            leido = readline("> ");
            iniciar_interfaz(leido, config_stdin, logger_stdin);
            log_info(logger_stdin, "Se creo la intefaz %s correctamente", leido);
            break;
        case 3:
            printf("Conectando interfaz STDOUT...\n");
            printf("Ingresa el nombre de la interfaz STDOUT\n");
            leido = readline("> ");
            iniciar_interfaz(leido, config_stdout, logger_stdout);
            log_info(logger_stdout, "Se creo la intefaz %s correctamente", leido);
            break;
        case 4:
            printf("Conectando interfaz DIALFS...\n");
            printf("Ingresa el nombre de la interfaz DIALFS\n");
            leido = readline("> ");
            iniciar_interfaz(leido, config_dialfs, logger_dialfs);
            log_info(logger_dialfs, "Se creo la intefaz %s correctamente", leido);
            break;
        case 5:
            printf("Cerrando puertos...\n");
            break;
        default:
            printf("Interfaz no válida. Por favor, conecte una opción válida.\n");
            break;
        }
    }

    /*while(!list_is_empty(interfaces)){
        INTERFAZ* aux;
        aux=list_remove(interfaces,0);
        pthread_join(aux->hilo, NULL);
        free(aux);
    }*/

    pthread_join(hilo_interfaz, NULL);
    pthread_join(hilo_llegadas, NULL);

    liberar_conexion(conexion_kernel);
    terminar_programa(logger_io_generica, config_generica);
    terminar_programa(logger_stdin, config_stdin);
    terminar_programa(logger_stdout, config_stdout);
    terminar_programa(logger_dialfs, config_dialfs);
    return 0;
}