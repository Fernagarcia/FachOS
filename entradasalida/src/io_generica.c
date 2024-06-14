#include <stdlib.h>
#include <stdio.h>
#include <io_generica.h>

int conexion_kernel;
int ip_memoria;
int puerto_memoria;
int conexion_memoria;


int id_nombre = 0;

t_log *entrada_salida;
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

sem_t synchronization;

const char *operaciones_gen[1] = {"IO_GEN_SLEEP"};
const char *operaciones_stdin[1] = {"IO_STDIN_READ"};
const char *operaciones_stdout[1] = {"IO_STDOUT_WRITE"};
const char *operaciones_dialfs[5] = {"IO_FS_CREATE", "IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE", "IO_FS_READ"};

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

void peticion_IO_GEN(SOLICITUD_INTERFAZ *interfaz_solicitada, t_config *config)
{
    log_info(logger_io_generica, "Ingreso de Proceso PID: %s a IO_GENERICA: %s\n", interfaz_solicitada->pid, interfaz_solicitada->nombre);
    int tiempo_a_esperar = atoi(interfaz_solicitada->args[0]);

    sleep(tiempo_a_esperar);

    log_info(logger_io_generica, "Tiempo cumplido. Enviando mensaje a Kernel");
}

void peticion_STDIN(SOLICITUD_INTERFAZ *interfaz_solicitada, t_config *config)
{
    char* registro_direccion = interfaz_solicitada->args[0];
    char* registro_tamanio = interfaz_solicitada->args[1];
    
    printf("Ingrese valor a escribir en memoria");
    // TODO: implementar console in 
    char* dato_a_escribir;

    // TODO: validar que el tamaño del dato es menor o igual al del registro
    if(sizeof(dato_a_escribir) <= registro_tamanio){
        // Tamaño del char** para reservar memoria
        int tamanio_datos = sizeof(registro_direccion) + sizeof(registro_tamanio) + sizeof(dato_a_escribir);
        // Reservo memoria para los datos q vamos a enviar en el char**
        char** datos = malloc(tamanio_datos);
        datos[0] = registro_direccion;
        // ANALIZAR: si validamos que el dato entre en el tamaño del registro antes de enviar el paquete, no hace falta incluir este dato
        datos[1] = registro_tamanio;
        datos[2] = dato_a_escribir;

        paquete_io_memoria(conexion_memoria, datos, IO_STDIN_READ);

        // Libero datos**
        free(datos[0]);
        free(datos[1]);
        free(datos[2]);
        free(datos);
        datos = NULL;
    }

}

void peticion_STDOUT(SOLICITUD_INTERFAZ *interfaz_solicitada, t_config *config )
{
    char* registro_direccion = interfaz_solicitada->args[0];
    char* registro_tamanio = interfaz_solicitada->args[1];

    // Tamaño del char** para reservar memoria
    int tamanio_datos = sizeof(registro_direccion) + sizeof(registro_tamanio); 
    // Reservo memoria para los datos q vamos a enviar en el char**
    char** datos = malloc(tamanio_datos);
    datos[0] = registro_direccion;
    datos[1] = registro_tamanio;

    paquete_io_memoria(conexion_memoria, datos, IO_STDOUT_WRITE);

    // Recibir el dato de la direccion de memoria
    int cod_op = recibir_operacion(conexion_memoria);
    t_list lista = recibir_paquete(conexion_memoria, logger_stdout);
    char* leido = list_get(lista, 0);
    
    // Mostrar dato leido de memoria
    printf("Dato leido: %s", leido);

    // Libero datos**
    free(datos[0]);
    free(datos[1]);
    free(datos);
    datos = NULL;

}

void peticion_DIAL_FS(SOLICITUD_INTERFAZ *interfaz_solicitada, t_config *config)
{
    // TODO: switch con las opciones del dial_fs (NO PERDER TIEMPO X AHORA, ES PARA LA ULTIMA ENTREGA)
}

void iniciar_interfaz(char *nombre, t_config *config, t_log *logger)
{
    INTERFAZ *interfaz = malloc(sizeof(INTERFAZ));

    interfaz->configuration = config;

    interfaz->solicitud = NULL;

    interfaz->datos = malloc(sizeof(DATOS_INTERFAZ));
    interfaz->datos->nombre = strdup(nombre);
    interfaz->datos->tipo = get_tipo_interfaz(interfaz, config_get_string_value(interfaz->configuration, "TIPO_INTERFAZ"));

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

    pthread_t interface_thread;
    argumentos_correr_io args = {interfaz};

    //INTERFAZ_CON_HILO *interfaz_con_hilo = malloc(sizeof(INTERFAZ_CON_HILO));
    //interfaz_con_hilo->interfaz = interfaz;
    //interfaz_con_hilo->hilo_interfaz = interface_thread;

    //list_add(interfaces, interfaz_con_hilo);
    
    
    if(pthread_create(&interface_thread, NULL, correr_interfaz, (void *) interfaz) != 0)
    {
        log_error(logger, "ERROR AL CREAR EL HILO DE INTERFAZ");
        exit(32);
    }
    
    pthread_detach(interface_thread);
}

void *correr_interfaz(void *interfaz_void)
{

    INTERFAZ *interfaz = (INTERFAZ*)interfaz_void;

    char *ip_kernel = config_get_string_value(interfaz->configuration, "IP_KERNEL");
    char *puerto_kernel = config_get_string_value(interfaz->configuration, "PUERTO_KERNEL");
    
    int kernel_conection = crear_conexion(ip_kernel, puerto_kernel);
    log_info(entrada_salida, "La interfaz %s está conectandose a kernel", interfaz->datos->nombre);

    paquete_nueva_IO(kernel_conection, interfaz);
    

    recibir_peticiones_interfaz(interfaz, kernel_conection, entrada_salida);
}

void operar_interfaz(SOLICITUD_INTERFAZ *solicitud)
{
    // TODO
}

void recibir_peticiones_interfaz(INTERFAZ *interfaz, int cliente_fd, t_log *logger)
{
    SOLICITUD_INTERFAZ *solicitud;
    t_list *lista;
    while (1)
    {
        lista = recibir_paquete(cliente_fd, logger);
        solicitud = asignar_espacio_a_solicitud(lista);
        // sabemos que no le van a llegar varias solicitudes juntas a la interfaz, de esto se ocupa kernel (probablemente con semaforos)
        switch (interfaz->datos->tipo)
        {
        case GENERICA:
            peticion_IO_GEN(solicitud, interfaz->configuration);
            break;
        case STDIN:
            peticion_STDIN(solicitud, interfaz->configuration);
            break;
        case STDOUT:
            peticion_STDOUT(solicitud, interfaz->configuration);
            break;
        case DIAL_FS:
            peticion_DIAL_FS(solicitud, interfaz->configuration);
            break;
        default:
            break;
        }
    }
}

void *gestionar_peticion_kernel(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    SOLICITUD_INTERFAZ *nueva_interfaz;
    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case IO_GENERICA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_io_generica);
            nueva_interfaz = asignar_espacio_a_solicitud(lista);
            peticion_IO_GEN(nueva_interfaz, config_generica);
            desbloquear_io *solicitud_desbloqueo = crear_solicitud_desbloqueo(nueva_interfaz->nombre, nueva_interfaz->pid);
            paqueteDeDesbloqueo(conexion_kernel, solicitud_desbloqueo);
            eliminar_io_solicitada(nueva_interfaz);
            break;
        case IO_STDIN:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_stdin);
            nueva_interfaz = list_get(lista, 0);
            log_info(logger_io_generica, "LA INTERFAZ %s", nueva_interfaz->nombre);
            operar_interfaz(nueva_interfaz);
            break;
        case IO_STDOUT:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_stdout);
            nueva_interfaz = list_get(lista, 0);
            log_info(logger_io_generica, "LA INTERFAZ %s", nueva_interfaz->nombre);
            operar_interfaz(nueva_interfaz);
            break;
        case IO_DIALFS:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_dialfs);
            nueva_interfaz = list_get(lista, 0);
            log_info(logger_io_generica, "LA INTERFAZ %s", nueva_interfaz->nombre);
            operar_interfaz(nueva_interfaz);
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

void *conectar_interfaces(void *args)
{
    char *opcion_en_string;
    int opcion;
    char *leido;

    printf("PUERTO DE CONEXIONES DE INTERFACES: ¿Que desea conectar?\n");
    while (opcion != 6)
    {

        printf("1. Conectar interfaz Generica \n");
        printf("2. Conectar interfaz STDIN \n");
        printf("3. Conectar interfaz STDOUT \n");
        printf("4. Conectar interfaz DIALFS \n");
        printf("5. Desconectar una interfaz \n");
        printf("6. Salir\n");
        opcion_en_string = readline("Seleccione una opción: ");
        opcion = atoi(opcion_en_string);

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
            printf("Ingresa el nombre de la interfaz que quieres desconectar\n");
            leido = readline("> ");
            paqueteDeMensajes(conexion_kernel, leido, DESCONECTAR_IO);
            buscar_y_desconectar_io(leido, interfaces, entrada_salida);
            break;
        case 6:
            printf("Cerrando puertos...\n");
            paqueteDeMensajes(conexion_kernel, "-Desconexion de todas las interfaces-", DESCONECTAR_TODO);
            list_clean_and_destroy_elements(interfaces, destruir_interfaz);
            return (void *)EXIT_SUCCESS;
        default:
            printf("Interfaz no válida. Por favor, conecte una opción válida.\n");
            break;
        }
        free(opcion_en_string);
    }
    return NULL;
}

SOLICITUD_INTERFAZ *asignar_espacio_a_solicitud(t_list *lista)
{
    SOLICITUD_INTERFAZ *nueva_interfaz = list_get(lista, 0);
    nueva_interfaz->nombre = list_get(lista, 1);
    nueva_interfaz->solicitud = list_get(lista, 2);
    nueva_interfaz->pid = list_get(lista, 3);
    nueva_interfaz->args = list_get(lista, 4);

    int argumentos = sizeof(nueva_interfaz->args) / sizeof(nueva_interfaz->args[0]);

    int j = 5;
    for (int i = 0; i < argumentos; i++)
    {
        nueva_interfaz->args[i] = strdup((char *)(list_get(lista, j)));
        j++;
    }

    return nueva_interfaz;
}

desbloquear_io *crear_solicitud_desbloqueo(char *nombre_io, char *pid)
{

    desbloquear_io *new_solicitude = malloc(sizeof(desbloquear_io));
    new_solicitude->nombre = strdup(nombre_io);
    new_solicitude->pid = strdup(pid);

    return new_solicitude;
}

int main(int argc, char *argv[])
{
    char *ip_kernel;
    char *puerto_kernel;

    interfaces = list_create();

    pthread_t hilo_llegadas;
    pthread_t hilo_menu;

    entrada_salida = iniciar_logger("main.log", "io_general_log", LOG_LEVEL_INFO);
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

    ip_memoria = config_get_string_value(config_generica, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config_generica, "PUERTO_MEMORIA");
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);

    conexion_kernel = crear_conexion(ip_kernel, puerto_kernel);
    log_info(entrada_salida, "%s\n\t\t\t\t\t\t%s\t%s\t", "Se ha establecido la conexion con memoria", ip_memoria, puerto_memoria);


    ArgsGestionarServidor args_cliente = {entrada_salida, conexion_kernel};

    pthread_create(&hilo_llegadas, NULL, gestionar_peticion_kernel, (void *)&args_cliente);

    sleep(1);
    // MENU PARA CREAR INTERFACES (PROVISIONAL?)
    pthread_create(&hilo_menu, NULL, conectar_interfaces, NULL);

    pthread_join(hilo_menu, NULL);
    //pthread_join(hilo_interfaz, NULL);
    pthread_join(hilo_llegadas, NULL);

    liberar_conexion(conexion_kernel);

    terminar_programa(logger_io_generica, config_generica);
    terminar_programa(logger_stdin, config_stdin);
    terminar_programa(logger_stdout, config_stdout);
    terminar_programa(logger_dialfs, config_dialfs);

    return 0;
}