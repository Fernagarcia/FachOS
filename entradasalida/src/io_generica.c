#include <stdlib.h>
#include <stdio.h>
#include <io_generica.h>

int conexion_kernel;
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
sem_t desconexion_io;

const char *operaciones_gen[1] = {"IO_GEN_SLEEP"};
const char *operaciones_stdin[1] = {"IO_STDIN_READ"};
const char *operaciones_stdout[1] = {"IO_STDOUT_WRITE"};
const char *operaciones_dialfs[5] = {"IO_FS_CREATE", "IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE", "IO_FS_READ"};

TIPO_INTERFAZ get_tipo_interfaz(INTERFAZ *interfaz, char *tipo_nombre){
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

void copiar_operaciones(INTERFAZ *interfaz){
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

void recibir_peticiones_interfaz(INTERFAZ* interfaz, int cliente_fd, t_log* logger, FILE* bloques, FILE* bitmap){

    SOLICITUD_INTERFAZ *solicitud;
    t_list *lista;
    desbloquear_io* aux;

    while (1)
    {
        int cod_op = recibir_operacion(cliente_fd);
        switch (cod_op)
        {
        case IO_GENERICA:
            lista = recibir_paquete(cliente_fd, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_IO_GEN(solicitud, interfaz->configuration);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(conexion_kernel, aux);
            eliminar_io_solicitada(solicitud);
            break;

        case IO_STDIN_READ:
            lista = recibir_paquete(cliente_fd, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_STDIN(solicitud, interfaz->configuration);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(conexion_kernel, aux);
            eliminar_io_solicitada(solicitud);
            break;

        case IO_STDOUT_WRITE:
            lista = recibir_paquete(cliente_fd, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_STDOUT(solicitud, interfaz->configuration);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(conexion_kernel, aux);
            eliminar_io_solicitada(solicitud);
            break;

        case DIAL_FS:
            lista = recibir_paquete(cliente_fd, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_DIAL_FS(solicitud, interfaz->configuration, bloques, bitmap);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(conexion_kernel, aux);
            eliminar_io_solicitada(solicitud);
            break;

        case DESCONECTAR_IO:
            log_warning(logger, "Desconectando interfaz. Espere un segundo...");
            sem_post(&desconexion_io);
            return;

        default:
            return;
        }
    }
}

SOLICITUD_INTERFAZ *asignar_espacio_a_solicitud(t_list *lista){
    SOLICITUD_INTERFAZ *nueva_interfaz = malloc(sizeof(SOLICITUD_INTERFAZ));
    nueva_interfaz = list_get(lista, 0);
    nueva_interfaz->nombre = strdup(list_get(lista, 1));
    nueva_interfaz->solicitud = strdup(list_get(lista, 2));
    nueva_interfaz->pid = strdup(list_get(lista, 3));
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

desbloquear_io *crear_solicitud_desbloqueo(char *nombre_io, char *pid){

    desbloquear_io *new_solicitude = malloc(sizeof(desbloquear_io));
    new_solicitude->nombre = strdup(nombre_io);
    new_solicitude->pid = strdup(pid);

    return new_solicitude;
}

// FUNCIONES DE ARCHIVOS FS

FILE* iniciar_archivo(char* nombre) {
        FILE* archivo = fopen(nombre,"r");
    if (archivo == NULL) {
        //  archivo no existe, crearlo
        archivo = fopen(nombre, "w+");
        if (archivo == NULL) {
            log_error(logger_dialfs, "Error al crear el archivo");
            return NULL;
        }
        log_info(logger_dialfs, "Archivo no existía, creado nuevo archivo.\n");
    } else {
        // El archivo existe, cerrarlo y abrirlo en modo lectura/escritura
        fclose(archivo);
        archivo = fopen(nombre, "r+");
        if (archivo == NULL) {
            log_error(logger_dialfs, "Error al abrir el archivo para lectura/escritura");
            return NULL;
        }
        log_info(logger_dialfs, "Archivo existía, abierto para lectura/escritura.\n");
    }
    return archivo;
}

FILE* inicializar_archivo_bloques(const char *filename, int block_size, int block_count) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Error al crear el archivo de bloques");
        exit(EXIT_FAILURE);
        return NULL;
    }
    
    char empty_block[block_size];
    memset(empty_block, 0, block_size);

    for (int i = 0; i < block_count; i++) {
        fwrite(empty_block, 1, block_size, file);
    }

    fclose(file);

    return file;
}

void leer_bloque(const char *filename, int block_size, int bloque_ini, char *buffer) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error al abrir el archivo de bloques para lectura");
        exit(EXIT_FAILURE);
    }

    fseek(file, bloque_ini * block_size, SEEK_SET);
    fread(buffer, 1, block_size, file);

    fclose(file);
}

void escribir_bloque(const char *filename, int block_size, int bloque_num, const char *data) {
    FILE *file = fopen(filename, "r+b");
    if (file == NULL) {
        perror("Error al abrir el archivo de bloques para escritura");
        exit(EXIT_FAILURE);
    }

    fseek(file, bloque_num * block_size, SEEK_SET);
    fwrite(data, 1, block_size, file);

    fclose(file);
}

FILE* inicializar_bitmap(const char *nombre_archivo, int block_count) {
    int bitmap_size = block_count / 8;
    FILE *file = fopen(nombre_archivo, "wb");
    if (file == NULL) {
        perror("Error al crear el archivo");
        exit(EXIT_FAILURE);
    }

    // Crear un buffer inicializado a 0
    unsigned char *buffer = (unsigned char *)calloc(bitmap_size, 1);
    if (buffer == NULL) {
        perror("Error al asignar memoria");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fwrite(buffer, bitmap_size, 1, file);

    return file;
}

int buscar_bloque_libre(const char *bitmap) {
    FILE *file = fopen(bitmap, "rb");
    if (file == NULL) {
        perror("Error al abrir el archivo bitmap");
        return -1;
    }

    unsigned char byte;
    int byte_index = 0;

    while (fread(&byte, 1, 1, file) == 1) {
        for (int bit_offset = 0; bit_offset < 8; bit_offset++) {
            if ((byte & (1 << bit_offset)) == 0) {
                fclose(file);
                return byte_index * 8 + bit_offset;
            }
        }
        byte_index++;
    }

    fclose(file);
    return -1;
}

void escribirBit(const char *nombre_archivo, int bit_index) {
    FILE *file = fopen(nombre_archivo, "r+b");
    if (file == NULL) {
        perror("Error al abrir el archivo bitmap");
        return;
    }

    int byte_index = bit_index / 8;
    int bit_offset = bit_index % 8;

    fseek(file, byte_index, SEEK_SET);

    unsigned char byte;
    fread(&byte, 1, 1, file);

    byte |= (1 << bit_offset);

    fseek(file, byte_index, SEEK_SET);
    fwrite(&byte, 1, 1, file);

    fclose(file);
}

void crear_archivo(const char* filename, const char *bitmap, int block_size) {
    int bloque_libre = buscar_bloque_libre(bitmap);

    if (bloque_libre == -1) {
        return;
    }

    escribirBit(bitmap, bloque_libre);
    escribir_bloque(filename, block_size, bloque_libre, 0);
}

// FUNCIONES I/O

void peticion_IO_GEN(SOLICITUD_INTERFAZ *interfaz_solicitada, t_config *config){
    log_info(logger_io_generica, "PID: %s - Operacion: %s", interfaz_solicitada->pid, interfaz_solicitada->solicitud);

    log_info(logger_io_generica, "Ingreso de Proceso PID: %s a IO_GENERICA: %s\n", interfaz_solicitada->pid, interfaz_solicitada->nombre);
    int tiempo_a_esperar = atoi(interfaz_solicitada->args[0]);

    sleep(tiempo_a_esperar);

    log_info(logger_io_generica, "Tiempo cumplido. Enviando mensaje a Kernel");
}

void peticion_STDIN(SOLICITUD_INTERFAZ *interfaz_solicitada, t_config *config){
    log_info(logger_stdin, "PID: %s - Operacion: %s", interfaz_solicitada->pid, interfaz_solicitada->solicitud);

    char* registro_direccion = interfaz_solicitada->args[0];
    char* registro_tamanio = interfaz_solicitada->args[1];
    
    // TODO: implementar console in 
    char* dato_a_escribir = readline("Ingrese valor a escribir en memoria: ");

    if((strlen(dato_a_escribir) + 1) <= atoi(registro_tamanio)){
        // Tamaño del char** para reservar memoria
        int tamanio_datos = strlen(registro_direccion) + strlen(registro_tamanio) + strlen(dato_a_escribir) + strlen(interfaz_solicitada->pid) + 4;

        // Reservo memoria para los datos q vamos a enviar en el char**
        char** datos = malloc(tamanio_datos);
        datos[0] = strdup(registro_direccion);
        datos[1] = strdup(dato_a_escribir);
        datos[2] = strdup(interfaz_solicitada->pid);

        paquete_io_memoria(conexion_memoria, datos, IO_STDIN_READ);

        // Libero datos**
        liberar_memoria(datos, 3);
    } else {
        // EXPLOTA TODO: 
        log_info(logger_stdout, "dato muy grande para este registro"); 
    }

}

void peticion_STDOUT(SOLICITUD_INTERFAZ *interfaz_solicitada, t_config *config ){
    log_info(logger_stdout, "PID: %s - Operacion: %s", interfaz_solicitada->pid, interfaz_solicitada->solicitud);

    char* registro_direccion = interfaz_solicitada->args[0];
    char* registro_tamanio = interfaz_solicitada->args[1];

    // Tamaño del char** para reservar memoria
    int tamanio_datos = strlen(registro_direccion) + strlen(registro_tamanio) + strlen(interfaz_solicitada->pid) + 3; 
    // Reservo memoria para los datos q vamos a enviar en el char**
    char** datos = malloc(tamanio_datos);
    datos[0] = strdup(registro_direccion);
    datos[1] = strdup(registro_tamanio);
    datos[2] = strdup(interfaz_solicitada->pid);

    paquete_io_memoria(conexion_memoria, datos, IO_STDOUT_WRITE);

    // Recibir el dato de la direccion de memoria
    int cod_op = recibir_operacion(conexion_memoria);
    t_list* lista = recibir_paquete(conexion_memoria, logger_stdout);
    char* leido = list_get(lista, 0);
    
    // Mostrar dato leido de memoria
    printf("Dato leido: %s", leido);

    free(leido);
    leido = NULL;
    // Libero datos**
    liberar_memoria(datos, 3);

}

void peticion_DIAL_FS(SOLICITUD_INTERFAZ *interfaz_solicitada, t_config *config, FILE* bloques, FILE* bitmap){

    //TODO: LOGICA PRINCIPAL DE LAS INSTRUCCIONES DE ENTRADA/SALIDA DIALFS
    /*switch (interfaz_solicitada->solicitud){
    case "IO_FS_CREATE":
        int block_size = config_get_int_value(config, "BLOCK_SIZE");
        crear_archivo(bloques, bitmap, block_size);
        break;
    
    case "IO_FS_DELETE":
        break;
    default:
        break;
    }
    */
}

void *conectar_interfaces(void *args){
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
            sem_wait(&desconexion_io);
            buscar_y_desconectar(leido, interfaces, entrada_salida);
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

void iniciar_interfaz(char *nombre, t_config *config, t_log *logger){

    INTERFAZ *interfaz = malloc(sizeof(INTERFAZ));

    interfaz->configuration = config;

    interfaz->procesos_bloqueados = NULL;

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

    pthread_t interface_thread = PTHREAD_ONCE_INIT;
    
    interfaz->hilo_de_ejecucion = interface_thread;

    list_add(interfaces,interfaz);
    
    if(pthread_create(&interfaz->hilo_de_ejecucion, NULL, correr_interfaz, (void *) interfaz) != 0)
    {
        log_error(logger, "ERROR AL CREAR EL HILO DE INTERFAZ");
        exit(32);
    }
}

void *correr_interfaz(void *interfaz_void){

    INTERFAZ *interfaz = (INTERFAZ*)interfaz_void;

    // TOMA DATOS DE KERNEL DE EL CONFIG
    char *ip_kernel = config_get_string_value(interfaz->configuration, "IP_KERNEL");
    char *puerto_kernel = config_get_string_value(interfaz->configuration, "PUERTO_KERNEL");

    // CREA LA CONEXION CON KERNEL  
    int kernel_conection = crear_conexion(ip_kernel, puerto_kernel);
    log_info(entrada_salida, "\nLa interfaz %s está conectandose a kernel", interfaz->datos->nombre);
    paquete_nueva_IO(kernel_conection, interfaz); // ENVIA PAQUETE A KERNEL
    log_info(entrada_salida, "\nConexion creada");

    // TOMA DATOS DE MEMORIA DE EL CONFIG
    char *ip_memoria = config_get_string_value(interfaz->configuration, "IP_MEMORIA");
    char *puerto_memoria = config_get_string_value(interfaz->configuration, "PUERTO_MEMORIA");

    // CREA LA CONEXION CON MEMORIA    
    int memoria_conection = crear_conexion(ip_memoria, puerto_memoria);
    log_info(entrada_salida, "\nLa interfaz %s está conectandose a memoria", interfaz->datos->nombre);
    paquete_nueva_IO(memoria_conection, interfaz); // ENVIA PAQUETE A MEMORIA
    log_info(entrada_salida, "\nConexion creada");
         
    // TODO: cambiar la ruta relativa a la absoluta de la carpeta donde deberian estar estos archivos
    if (interfaz->datos->tipo == DIAL_FS) {
        int block_count = config_get_int_value(interfaz->configuration, "BLOCK_COUNT");
        int block_size = config_get_int_value(interfaz->configuration, "BLOCK_SIZE");

        FILE* bloques = inicializar_archivo_bloques("bloques.dat", block_size, block_count);
        FILE* bitmap = inicializar_bitmap("bitmap.dat", block_count);

        recibir_peticiones_interfaz(interfaz, kernel_conection, entrada_salida, bloques, bitmap);
    } else {
        recibir_peticiones_interfaz(interfaz, kernel_conection, entrada_salida, NULL, NULL);
    }   
    return NULL;
}

int main(int argc, char *argv[]){

    char *ip_kernel;
    char *puerto_kernel;
    char *ip_memoria;
    char *puerto_memoria;
    interfaces = list_create();

    sem_init(&desconexion_io, 1, 0);

    entrada_salida     = iniciar_logger("main.log", "io_general_log", LOG_LEVEL_INFO);
    logger_io_generica = iniciar_logger("io_generica.log", "io_generica_log", LOG_LEVEL_INFO);
    logger_stdin       = iniciar_logger("io_stdin.log", "io_stdin_log", LOG_LEVEL_INFO);
    logger_stdout      = iniciar_logger("io_stdout.log", "io_stdout_log", LOG_LEVEL_INFO);
    logger_dialfs      = iniciar_logger("io_dialfs.log", "io_dialfs_log", LOG_LEVEL_INFO);

    config_generica    = iniciar_config("io_generica.config");
    config_stdin       = iniciar_config("io_stdin.config");
    config_stdout      = iniciar_config("io_stdout.config");
    config_dialfs      = iniciar_config("io_dialfs.config");

    pthread_t hilo_llegadas;
    pthread_t hilo_menu;

    // CREA CONEXION CON KERNEL
    ip_kernel = config_get_string_value(config_generica, "IP_KERNEL");
    puerto_kernel = config_get_string_value(config_generica, "PUERTO_KERNEL");
    conexion_kernel = crear_conexion(ip_kernel, puerto_kernel);
    log_info(entrada_salida, "%s\n\t\t\t\t\t\t%s\t%s\t", "Se ha establecido la conexion con Kernel", ip_kernel, puerto_kernel);

    // CREA CONEXION CON MEMORIA
    ip_memoria = config_get_string_value(config_generica, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config_generica, "PUERTO_MEMORIA");
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    log_info(entrada_salida, "%s\n\t\t\t\t\t\t%s\t%s\t", "Se ha establecido la conexion con memoria", ip_memoria, puerto_memoria);

    // ENVIA A KERNEL QUE SE CONECTO I/O
    char *mensaje_para_kernel = "Se ha conectado la interfaz\n";
    enviar_operacion(mensaje_para_kernel, conexion_kernel, MENSAJE);
    log_info(entrada_salida, "Mensajes enviados exitosamente");

    // MENU DE INTERFACES
    pthread_create(&hilo_menu, NULL, conectar_interfaces, NULL);
    pthread_join(hilo_menu, NULL);

    // LIBERA MEMORIA Y CONEXIONES
    sem_destroy(&desconexion_io);
    liberar_conexion(conexion_kernel);
    liberar_conexion(conexion_memoria);
    terminar_programa(logger_io_generica, config_generica);
    terminar_programa(logger_stdin, config_stdin);
    terminar_programa(logger_stdout, config_stdout);
    terminar_programa(logger_dialfs, config_dialfs);

    return 0;
}