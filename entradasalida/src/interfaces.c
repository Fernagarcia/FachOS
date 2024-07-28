#include <stdlib.h>
#include <stdio.h>
#include <interfaces.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>

t_log *entrada_salida;
t_log *logger_io_generica;
t_log *logger_stdin;
t_log *logger_stdout;
t_log *logger_dialfs;

t_config *config_generica;
t_config *config_stdin;
t_config *config_stdout;
t_config *config_dialfs;

pthread_t hilo_interfaz;

sem_t conexion_generica;
sem_t conexion_io;
sem_t desconexion_io;

char *directorio_interfaces;
char *bloques;
char* bitmap;
int bitmap_size;
int bloques_fd;
int bitmap_fd;
t_list *metadata_files;

int block_count;
int block_size;
int retraso_compactacion;

char *operaciones_gen[1] = {"IO_GEN_SLEEP"};
char *operaciones_stdin[1] = {"IO_STDIN_READ"};
char *operaciones_stdout[1] = {"IO_STDOUT_WRITE"};
char *operaciones_dialfs[5] = {"IO_FS_CREATE", "IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE", "IO_FS_READ"};

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
    else if (!strcmp(tipo_nombre, "DIALFS"))
    {
        tipo = DIAL_FS;
    }
    return tipo;
}

void copiar_operaciones(INTERFAZ *interfaz){
    switch (interfaz->datos->tipo)
    {
    case GENERICA:
        interfaz->datos->operaciones = operaciones_gen;
        break;
    case STDIN:
        interfaz->datos->operaciones = operaciones_stdin;
        break;
    case STDOUT:
        interfaz->datos->operaciones = operaciones_stdout;
        break;
    case DIAL_FS:
        interfaz->datos->operaciones = operaciones_dialfs;
        break;
    }
}

SOLICITUD_INTERFAZ *asignar_espacio_a_solicitud(t_list *lista){
    SOLICITUD_INTERFAZ *nueva_interfaz = list_get(lista, 0);
    nueva_interfaz->nombre = list_get(lista, 1);
    nueva_interfaz->solicitud = list_get(lista, 2);
    nueva_interfaz->pid = list_get(lista, 3);
    nueva_interfaz->args = string_array_new();

	for(int i = 4; i < list_size(lista); i++){
		char* nuevo_arg = strdup((char*)list_get(lista, i));
        string_array_push(&nueva_interfaz->args, nuevo_arg);
	} 

    return nueva_interfaz;
}

desbloquear_io *crear_solicitud_desbloqueo(char *nombre_io, int* pid){

    desbloquear_io *new_solicitude = malloc(sizeof(desbloquear_io));
    new_solicitude->nombre = nombre_io;
    new_solicitude->pid = pid;

    return new_solicitude;
}

// FUNCIONES DE ARCHIVOS FS

// METADATA

void listar_archivos_metadata(char *path) {
    DIR *d;
    struct dirent *dir;
    d = opendir(path);
    if (d == NULL) {
        log_error(logger_dialfs, "Error al abrir el directorio");
        exit(EXIT_FAILURE);
    }

    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_REG) { // Asegurarse de que sea un archivo regular
            MetadataArchivo *archivo = malloc(sizeof(MetadataArchivo));
            if (archivo == NULL) {
                log_error(logger_dialfs, "Error al asignar memoria para la estructura MetadataArchivo");
                closedir(d);
                exit(EXIT_FAILURE);
            }

            strncpy(archivo->nombre_archivo, dir->d_name, sizeof(archivo->nombre_archivo) - 1);
            archivo->nombre_archivo[sizeof(archivo->nombre_archivo) - 1] = '\0'; // Asegurarse de que esté terminada en '\0'

            // Construir la ruta completa al archivo de metadatos
            char ruta_completa[1024];
            snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", path, dir->d_name);

            // Leer los metadatos del archivo
            cargar_metadata(ruta_completa, archivo);

            // Agregar el archivo a la lista
            list_add(metadata_files, archivo);
        }
    }

    closedir(d);
}

void cargar_metadata(char *nombre_archivo, MetadataArchivo *metadata) {
    FILE *file = fopen(nombre_archivo, "r");
    if (file == NULL) {
        log_error(logger_dialfs, "Error al abrir el archivo de metadatos");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "BLOQUE_INICIAL=%d\n", &metadata->bloque_inicial);
    fscanf(file, "TAMANIO_ARCHIVO=%d\n", &metadata->tamanio_archivo);

    fclose(file);
}

// Función para encontrar un archivo por nombre en la lista
MetadataArchivo* encontrar_archivo_por_nombre(char *nombre_archivo) {
    for (int i = 0; i < list_size(metadata_files); i++) {
        MetadataArchivo *archivo = list_get(metadata_files, i);
        if (strcmp(archivo->nombre_archivo, nombre_archivo) == 0) {
            return archivo;
        }
    }
    return NULL;
}

// Función para encontrar un archivo en la lista por su nombre
int indice_de_archivo(char *nombre_archivo) {
    for (int i = 0; i < list_size(metadata_files); i++) {
        MetadataArchivo *archivo = list_get(metadata_files, i);
        if (strcmp(archivo->nombre_archivo, nombre_archivo) == 0) {
            return i;  // Retorna el índice del archivo
        }
    }
    return -1;  // No se encontró el archivo
}

// Función para modificar un archivo en la lista
void modificar_archivo_en_lista(char *nombre_archivo, int nuevo_bloque_inicial, int nuevo_tamanio_archivo) {
    MetadataArchivo *archivo = encontrar_archivo_por_nombre(nombre_archivo);
    if (archivo == NULL) {
        fprintf(stderr, "Error: No se encontró el archivo '%s' en la lista.\n", nombre_archivo);
        return;
    }

    archivo->bloque_inicial = nuevo_bloque_inicial;
    archivo->tamanio_archivo = nuevo_tamanio_archivo;
}

// Función para eliminar un archivo de la lista por su nombre
void eliminar_archivo_de_lista(char *nombre_archivo) {
    int indice = indice_de_archivo(nombre_archivo);
    if (indice != -1) {
        MetadataArchivo *archivo_a_eliminar = list_remove(metadata_files, indice);
        free(archivo_a_eliminar);  // Liberar la memoria del archivo eliminado
    } else {
        log_error(logger_dialfs, "Archivo %s no encontrado en la lista.\n", nombre_archivo);
    }
}

// Función para imprimir la lista de archivos
void imprimir_lista_archivos() {
    for (int i = 0; i < list_size(metadata_files); i++) {
        MetadataArchivo *archivo = list_get(metadata_files, i);
        printf("Archivo %d: %s\n", i + 1, archivo->nombre_archivo);
        printf("  Bloque Inicial: %d\n", archivo->bloque_inicial);
        printf("  Tamaño Archivo: %d\n", archivo->tamanio_archivo);
    }
}

//

// BITMAP

void crear_y_mapear_bitmap(char *nombre_archivo) {
    // Calcula el tamaño del bitmap en bytes
    bitmap_size = (block_count + 7) / 8;

    // Abre el archivo
    bitmap_fd = open(nombre_archivo, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (bitmap_fd == -1) {
        log_error(logger_dialfs, "Error al abrir o crear el archivo");
        exit(EXIT_FAILURE);
    }

    // Verifica el tamaño del archivo y ajusta si es necesario
    struct stat st;
    if (fstat(bitmap_fd, &st) == -1) {
        log_error(logger_dialfs, "Error al obtener el tamaño del archivo");
        close(bitmap_fd);
        exit(EXIT_FAILURE);
    }

    if (st.st_size < bitmap_size) {
        if (ftruncate(bitmap_fd, bitmap_size) == -1) {
            log_error(logger_dialfs, "Error al ajustar el tamaño del archivo");
            close(bitmap_fd);
            exit(EXIT_FAILURE);
        }
    }

    // Mapea el archivo a memoria
    bitmap = mmap(NULL, bitmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, bitmap_fd, 0);
    if (bitmap == MAP_FAILED) {
        log_error(logger_dialfs, "Error al mapear el archivo");
        close(bitmap_fd);
        exit(EXIT_FAILURE);
    }

    // Inicializa el bitmap a 0 si es un archivo nuevo
    if (st.st_size == 0) {
        memset(bitmap, 0, bitmap_size);
    }
    close(bitmap_fd);
}

// Función para establecer un bit en el bitmap
void establecer_bit(int indice, int valor) {
    int byteIndex = indice / 8;
    int bitIndex = indice % 8;
    if (valor) {
        bitmap[byteIndex] |= (1 << bitIndex);
    } else {
        bitmap[byteIndex] &= ~(1 << bitIndex);
    }
}

// Función para obtener el valor de un bit en el bitmap
int obtener_bit(int indice) {
    int byteIndex = indice / 8;
    int bitIndex = indice % 8;
    return (bitmap[byteIndex] & (1 << bitIndex)) != 0;
}

// Función para imprimir el bitmap
void imprimir_bitmap() {
    for (int i = 0; i < block_count; i++) {
        printf("El bit %d está %s\n", i, obtener_bit(i) ? "ocupado" : "libre");
    }
}

// Función para liberar el bitmap y cerrar el archivo
void liberar_bitmap() {
    if (munmap(bitmap, bitmap_size) == -1) {
        log_error(logger_dialfs, "Error al desmapear el archivo");
    }
    close(bitmap_fd);
}

int buscar_bloque_libre() {
    for (int i = 0; i < block_count; i++) {
        if (!obtener_bit(i)) {
            return i;
        }
    }
    return -1;
}

// Asigna bloques en el bitmap
void asignar_espacio_en_bitmap(int bloque_inicial, int tamanio_archivo) {
    int bloques_a_asignar = bloques_necesarios(tamanio_archivo);
    for(int i= bloque_inicial; i < (bloque_inicial + bloques_a_asignar); i++) {
        if(i>block_count) {
            log_error(logger_dialfs, "FLACO ESTAS ASIGNANDO MAS BLOQUES DE LOS QUE HAY");
            exit (-32);
        }
        establecer_bit(i, 1);
    }
}

// Desasigna bloques en el bitmap
void actualizar_bitmap(int bloque_inicial, int bloques_actuales, int bloques_requeridos) {
    for (int i = bloques_requeridos; i < bloques_actuales; i++) {
        establecer_bit(bloque_inicial + i, false);
    }
}



void iniciar_archivo_bloques(char *filename) {
    // Abre el archivo
    bloques_fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (bloques_fd == -1) {
        log_error(logger_dialfs,"Error al abrir o crear el archivo");
        exit(EXIT_FAILURE);
    }

    int size = block_size * block_count;
    if (ftruncate(bloques_fd, size) == -1) {
        log_error(logger_dialfs,"Error al ajustar el tamaño del archivo");
        close(bloques_fd);
        exit(EXIT_FAILURE);
    }

    bloques = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, bloques_fd, 0);
    if (bloques == MAP_FAILED) {
        log_error(logger_dialfs, "Error al mapear el archivo a memoria");
        close(bloques_fd);
        exit(EXIT_FAILURE);
    }

    close(bloques_fd);
}

char* crear_path_metadata(char* nombre_archivo) {
    char* path_retorno = string_new();
    string_append(&path_retorno, directorio_interfaces);
    string_append(&path_retorno, "/");
    string_append(&path_retorno, nombre_archivo);

    return path_retorno;
}

void crear_metadata(char *nombre_archivo, int bloque_inicial, int tamanio_archivo) {
    FILE *file = fopen(crear_path_metadata(nombre_archivo), "w");
    if (file == NULL) {
        log_error(logger_dialfs, "Error al crear el archivo de metadatos");
        exit(EXIT_FAILURE);
    }

    fprintf(file, "BLOQUE_INICIAL=%d\n", bloque_inicial);
    fprintf(file, "TAMANIO_ARCHIVO=%d\n", tamanio_archivo);

    fclose(file);

    MetadataArchivo* metadata = malloc(sizeof(MetadataArchivo));

    strncpy(metadata->nombre_archivo, nombre_archivo, sizeof(metadata->nombre_archivo) - 1);
    metadata->nombre_archivo[sizeof(metadata->nombre_archivo) - 1] = '\0';  // Asegurar la terminación de la cadena
    metadata->bloque_inicial = bloque_inicial;
    metadata->tamanio_archivo = tamanio_archivo;

    list_add(metadata_files, metadata);
}

void leer_metadata(char *nombre_archivo, int *bloque_inicial, int *tamanio_archivo) {
    FILE *file = fopen(crear_path_metadata(nombre_archivo), "r");
    log_debug(logger_dialfs, "Intentando abrir archivo de metadatos en: %s\n", nombre_archivo);
    if (file == NULL) {
        log_error(logger_dialfs, "Error al abrir el archivo de metadatos");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "BLOQUE_INICIAL=%d\n", bloque_inicial);
    fscanf(file, "TAMANIO_ARCHIVO=%d\n", tamanio_archivo);

    fclose(file);
}

void modificar_metadata(char *nombre_archivo, int nuevo_bloque_inicial, int nuevo_tamanio_archivo) {
    // Abre el archivo en modo lectura/escritura ("r+")
    FILE *file = fopen(crear_path_metadata(nombre_archivo), "r+");
    if (file == NULL) {
        log_error(logger_dialfs, "Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }

    // Variables para almacenar las líneas leídas y para construir el nuevo contenido
    char linea[256];
    char nuevo_contenido[512] = "";

    // Banderas para saber si hemos encontrado las líneas que debemos modificar
    int encontrado_bloque_inicial = 0;
    int encontrado_tamanio_archivo = 0;

    // Leer el archivo línea por línea y modificar las líneas deseadas
    while (fgets(linea, sizeof(linea), file) != NULL) {
        if (strncmp(linea, "BLOQUE_INICIAL=", 15) == 0) {
            sprintf(linea, "BLOQUE_INICIAL=%d\n", nuevo_bloque_inicial);
            encontrado_bloque_inicial = 1;
        } else if (strncmp(linea, "TAMANIO_ARCHIVO=", 16) == 0) {
            sprintf(linea, "TAMANIO_ARCHIVO=%d\n", nuevo_tamanio_archivo);
            encontrado_tamanio_archivo = 1;
        }
        strcat(nuevo_contenido, linea);
    }

    // Verificar si no se encontraron las líneas y agregarlas al final si es necesario
    if (!encontrado_bloque_inicial) {
        sprintf(linea, "BLOQUE_INICIAL=%d\n", nuevo_bloque_inicial);
        strcat(nuevo_contenido, linea);
    }
    if (!encontrado_tamanio_archivo) {
        sprintf(linea, "TAMANIO_ARCHIVO=%d\n", nuevo_tamanio_archivo);
        strcat(nuevo_contenido, linea);
    }

    // Volver al inicio del archivo y truncarlo
    rewind(file);
    if (ftruncate(fileno(file), 0) != 0) {
        log_error(logger_dialfs, "Error al truncar el archivo");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Escribir el nuevo contenido en el archivo
    fputs(nuevo_contenido, file);

    // Cerrar el archivo
    fclose(file);

    modificar_archivo_en_lista(nombre_archivo, nuevo_bloque_inicial, nuevo_tamanio_archivo);
}

void borrar_metadata(char* nombre_archivo) {
    if (remove(crear_path_metadata(nombre_archivo)) == 0) {
        log_debug(logger_dialfs, "Metadata de %s borrado exitosamente.\n", nombre_archivo);
    } else {
        log_error(logger_dialfs, "Error al borrar el archivo de datos");
        return;
    }

    eliminar_archivo_de_lista(nombre_archivo);    
}

int crear_archivo(char* nombre_archivo) {
    int bloque_inicial = buscar_bloque_libre();
    if(bloque_inicial == -1) {
        log_error(logger_dialfs, "No hay bloques libres");
        return -1;
    }
    crear_metadata(nombre_archivo, bloque_inicial, 0);
    establecer_bit(bloque_inicial, 1);   // modificamos el bitmap para aclarar que el bloque no esta libre

    return bloque_inicial;
}

void borrar_archivo(char* nombre_archivo) {
    int bloque_inicial;
    int tamanio_archivo;
    leer_metadata(nombre_archivo, &bloque_inicial, &tamanio_archivo);
    int cantidad_bloques_a_borrar = bloques_necesarios(tamanio_archivo);
    for (int i = bloque_inicial; i < (bloque_inicial + cantidad_bloques_a_borrar) ; i++) {
        establecer_bit(i, 0);      // liberamos los bits del bitmap
    }
    borrar_metadata(nombre_archivo);

}

// Función para escribir en un archivo
void escribir_en_archivo(char* nombre_archivo, char* dato_a_escribir, int tamanio_dato, int posicion_a_escribir) {
    int bloque_inicial;
    int tamanio_archivo;
    leer_metadata(nombre_archivo, &bloque_inicial, &tamanio_archivo);
    
    // Verificar si hay suficiente espacio en el archivo
    if (posicion_a_escribir + tamanio_dato > tamanio_archivo) {
        log_error(logger_dialfs, "Error: No hay suficiente espacio en el archivo para escribir los datos.\n");
        return;
    }

    // Calcular la posición en el archivo mapeado
    int posicion_global = (bloque_inicial * block_size) + posicion_a_escribir;
    
    // Verificar que el puntero bloques no sea NULL
    if (bloques == NULL) {
        log_error(logger_dialfs, "Error: El archivo de bloques no está mapeado correctamente.\n");
        return;
    }

    // Escribir los datos en la posición calculada
    memcpy(bloques + posicion_global, dato_a_escribir, tamanio_dato);

    // Si necesitas asegurarte de que los cambios se escriban inmediatamente en el archivo:
    msync(bloques + posicion_global, tamanio_dato, MS_SYNC);

}

void dial_fs_write(INTERFAZ* io, int pid, char* nombre_archivo, char* registro_direccion, char* registro_tamanio, char* registro_puntero_archivo) {
    // Leer en memoria registro_tamanio bytes a partir de la posicion registro_direccion
    // Escribirlo en el archivo a partir de la posicion registro_puntero_archivo

    PAQUETE_LECTURA* paquete = malloc(sizeof(PAQUETE_LECTURA));
    paquete->direccion_fisica = strdup(registro_direccion);
    paquete->pid = pid;
    paquete->tamanio = atoi(registro_tamanio);

    paquete_leer_memoria(io->sockets->conexion_memoria, paquete);
    
    // Recibir el dato de la direccion de memoria
    int cod_op = recibir_operacion(io->sockets->conexion_memoria);

    if(cod_op != RESPUESTA_LEER_MEMORIA){ /* ERROR OPERACION INVALIDA */ exit(-32); }

    t_list* lista = recibir_paquete(io->sockets->conexion_memoria, logger_dialfs);
    
    char* leido = list_get(lista, 0);
    // Mostrar dato leido de memoria
    log_debug(logger_dialfs, "\nEl dato solicitado de memoria es: < %s >", leido);

    // Escribo el dato en el archivo en la posicion de registro_puntero_archivo
    escribir_en_archivo(nombre_archivo, leido, atoi(registro_tamanio), atoi(registro_puntero_archivo));

    free(leido);
    leido = NULL;

    // Libero datos**
    
    free(paquete->direccion_fisica);
    paquete->direccion_fisica = NULL;
    free(paquete);
    paquete = NULL;

    list_destroy(lista);
}

// Funcion para leer en un archivo
void leer_en_archivo(char* nombre_archivo, void* buffer, int tamanio_dato, int posicion_a_leer) {
    // Leer los metadatos del archivo
    int bloque_inicial;
    int tamanio_archivo;
    leer_metadata(nombre_archivo, &bloque_inicial, &tamanio_archivo);

    // Verificar si la posición y el tamaño a leer están dentro del tamaño del archivo
    if (posicion_a_leer + tamanio_dato > tamanio_archivo) {
        log_error(logger_dialfs, "Error: La posición y el tamaño a leer exceden el tamaño del archivo.\n");
        return;
    }

    // Calcular la posición en el archivo mapeado
    int posicion_global = (bloque_inicial * block_size) + posicion_a_leer;
    
    // Leer los datos desde la posición calculada al buffer proporcionado
    memcpy(buffer, bloques + posicion_global, tamanio_dato);
    
    // Si necesitas asegurarte de que los cambios se escriban inmediatamente en el archivo:
    msync(bloques + posicion_global, tamanio_dato, MS_SYNC);
}

void dial_fs_read(INTERFAZ* io,int pid, char* nombre_archivo, char* registro_direccion, char* registro_tamanio, char* registro_puntero_archivo) {
    // Leer en el archivo registro_tamanio bytes a partir de registro_puntero_archivo
    // Escribirlo en registro_direccion en memoria

    int tamanio_lectura = atoi(registro_tamanio);

    void* buffer = malloc(tamanio_lectura + 1); // +1 para el terminador nulo

    leer_en_archivo(nombre_archivo, buffer, tamanio_lectura, atoi(registro_puntero_archivo));

    PAQUETE_ESCRITURA* paquete_escribir = malloc(sizeof(PAQUETE_ESCRITURA));
    paquete_escribir->pid = pid;
    paquete_escribir->direccion_fisica = registro_direccion;
    paquete_escribir->dato = malloc(sizeof(t_dato));
    paquete_escribir->dato->data = buffer;
    paquete_escribir->dato->tamanio = tamanio_lectura + 1;

    paquete_escribir_memoria(io->sockets->conexion_memoria, paquete_escribir);

    log_info(logger_dialfs, "Se escribio correctamente. Enviando mensaje a kernel"); 

    free(paquete_escribir->dato);
    paquete_escribir->dato = NULL;
    free(paquete_escribir);
    paquete_escribir = NULL;
}

void truncar(char *nombre_archivo, int nuevo_tamanio, int pid) {
    int bloque_inicial;
    int tamanio_archivo;
    leer_metadata(nombre_archivo, &bloque_inicial, &tamanio_archivo);
    
    int bloques_requeridos = bloques_necesarios(nuevo_tamanio);
    int bloques_actuales = bloques_necesarios(tamanio_archivo);

    if (bloques_requeridos <= bloques_actuales) {
        // Caso de reducción o mantener tamaño
        modificar_metadata(nombre_archivo, bloque_inicial, nuevo_tamanio);
        actualizar_bitmap(bloque_inicial, bloques_actuales, bloques_requeridos);
    } else {
        // Caso de aumento de tamaño
        if (tiene_espacio_suficiente(bloque_inicial, tamanio_archivo, nuevo_tamanio)) {
            modificar_metadata(nombre_archivo, bloque_inicial, nuevo_tamanio);
            asignar_espacio_en_bitmap(bloque_inicial, nuevo_tamanio);
        } else {
            compactar_y_mover_archivo_al_final(nombre_archivo, pid);
            leer_metadata(nombre_archivo, &bloque_inicial, &tamanio_archivo);
            if (tiene_espacio_suficiente(bloque_inicial, tamanio_archivo, nuevo_tamanio)) {
                modificar_metadata(nombre_archivo, bloque_inicial, nuevo_tamanio);
                asignar_espacio_en_bitmap(bloque_inicial, nuevo_tamanio);
            } else {
                log_error(logger_dialfs, "NO HAY ESPACIO EN EL DISCO, COMPRATE UNO MAS GRANDE RATON");
            }
        }
    }
}

void compactar_archivo_bloques() {
    int write_index = 0;

    // Recorrer el bitmap y mover los bloques usados al principio del archivo
    for (int read_index = 0; read_index < block_count; read_index++) {
        if (obtener_bit(read_index)) {
            if (write_index != read_index) {
                // Mover el bloque del read_index al write_index
                memcpy(bloques + write_index * block_size, bloques + read_index * block_size, block_size);

    // Actualizar el archivo de metadatos si es el primer bloque del archivo
                for (int i = 0; i < list_size(metadata_files); i++) {
                    MetadataArchivo *archivo = list_get(metadata_files, i);
                    if (archivo->bloque_inicial == read_index) {
                        // Actualizar el bloque inicial del archivo en el archivo de metadatos
                        archivo->bloque_inicial = write_index;
                        modificar_metadata(archivo->nombre_archivo, archivo->bloque_inicial, archivo->tamanio_archivo);
                    }
                }

                establecer_bit(write_index, true);
                establecer_bit(read_index, false);
            }
            write_index++;
        }
    }

    /*// Ajustar el tamaño del archivo de bloques si es necesario
    if (ftruncate(bloques_fd, write_index * block_size) == -1) {
        perror("Error al ajustar el tamaño del archivo");
        exit(EXIT_FAILURE);
    }*/
}

void compactar_y_mover_archivo_al_final(char* nombre_archivo, int pid) {

    log_info(logger_dialfs, "PID: %d - Inicio Compactación.", pid);
    
    // Leer los metadatos del archivo
    int bloque_inicial;
    int tamanio_archivo;
    leer_metadata(nombre_archivo, &bloque_inicial, &tamanio_archivo);

    // Almacenar temporalmente los datos del archivo
    char* buffer = (char*)malloc(tamanio_archivo);
    if (buffer == NULL) {
        log_error(logger_dialfs, "Error al asignar memoria para el buffer temporal");
        exit(EXIT_FAILURE);
    }
    memcpy(buffer, bloques + bloque_inicial * block_size, tamanio_archivo);

    // Borrar el archivo almacenado temporalmente del bitmap
    for (int i = 0; i < bloques_necesarios(tamanio_archivo); i++) {
        establecer_bit(bloque_inicial + i, false);
    }

    // Simulo el retraso de la compactacion con usleep

    usleep(retraso_compactacion * 1000);
    
    // Compactar el archivo de bloques
    compactar_archivo_bloques();

    // Buscar el nuevo bloque inicial (primer bloque libre luego de compactar)
    int nuevo_bloque_inicial = buscar_bloque_libre();
    // Escribir los datos almacenados temporalmente en la nueva ubicación
    memcpy(bloques + nuevo_bloque_inicial * block_size, buffer, tamanio_archivo);
    free(buffer);

    // Modifico el bitmap
    asignar_espacio_en_bitmap(nuevo_bloque_inicial, tamanio_archivo);

    // Actualizar los metadatos del archivo
    modificar_metadata(nombre_archivo, nuevo_bloque_inicial, tamanio_archivo);

    log_info(logger_dialfs, "PID: %d - Fin Compactación.", pid);    
}

int bloques_necesarios(int tamanio_archivo) {
    int bloques_necesarios = (tamanio_archivo + block_size - 1) / block_size;
    if (tamanio_archivo == 0) {
        bloques_necesarios = 1;
    }
    return bloques_necesarios;
}

bool tiene_espacio_suficiente(int bloque_inicial, int tamanio_actual, int nuevo_tamanio) {
    // Calcula el número de bloques necesarios para el tamaño actual y el nuevo tamaño
    int bloques_actuales = bloques_necesarios(tamanio_actual);
    int bloques_nuevos = bloques_necesarios(nuevo_tamanio);

    // Si los bloques nuevos son menores o iguales a los actuales, hay espacio suficiente
    if (bloques_nuevos <= bloques_actuales) {
        return true;
    }

    // Si se necesitan más bloques, verifica si hay suficientes bloques contiguos libres a partir del bloque inicial
    int bloques_adicionales_necesarios = bloques_nuevos - bloques_actuales;
    int bloques_libres_contiguos = 0;

    for (int i = bloque_inicial + bloques_actuales; i < block_count; i++) {
        if (!obtener_bit(i)) {
            bloques_libres_contiguos++;
            if (bloques_libres_contiguos >= bloques_adicionales_necesarios) {
                return true;
            }
        } else {
            bloques_libres_contiguos = 0;
            return false;
            // Resetear el contador si se encuentra un bloque ocupado
        }
    }

    // Si no se encuentran suficientes bloques contiguos libres a partir del bloque inicial, retorna false
    return false;
}

// FUNCIONES I/O

void peticion_IO_GEN(SOLICITUD_INTERFAZ *interfaz_solicitada, INTERFAZ* io){
    log_info(logger_io_generica, "PID: %d - Operacion: %s", *interfaz_solicitada->pid, interfaz_solicitada->solicitud);

    log_info(logger_io_generica, "Ingreso de Proceso PID: %d a IO_GENERICA: %s\n", *interfaz_solicitada->pid, interfaz_solicitada->nombre);
    int tiempo_a_esperar = atoi(interfaz_solicitada->args[0]);

    sleep(tiempo_a_esperar);

    log_info(logger_io_generica, "Tiempo cumplido. Enviando mensaje a Kernel");
}

void peticion_STDIN(SOLICITUD_INTERFAZ *interfaz_solicitada, INTERFAZ* io){
    log_info(logger_stdin, "PID: %d - Operacion: %s - Tamaño: %s", *interfaz_solicitada->pid, interfaz_solicitada->solicitud, interfaz_solicitada->args[1]);

    char* registro_direccion = interfaz_solicitada->args[0];
    char* registro_tamanio = interfaz_solicitada->args[1];
    
    char* dato_a_escribir = readline("Ingrese dato a escribir en memoria: ");

    if(strlen(dato_a_escribir) <= atoi(registro_tamanio)){
        PAQUETE_ESCRITURA* paquete_escribir = malloc(sizeof(PAQUETE_ESCRITURA));
        paquete_escribir->pid = *interfaz_solicitada->pid;
        paquete_escribir->direccion_fisica = registro_direccion;
        paquete_escribir->dato = malloc(sizeof(t_dato));
        paquete_escribir->dato->data = strdup(dato_a_escribir);
        paquete_escribir->dato->tamanio = strlen(dato_a_escribir);

        paquete_escribir_memoria(io->sockets->conexion_memoria, paquete_escribir);

        free(paquete_escribir->dato);
        paquete_escribir->dato = NULL;
        free(paquete_escribir);
        paquete_escribir = NULL;
        log_info(logger_stdin, "Se escribio correctamente. Enviando mensaje a kernel"); 
    } else {
        // EXPLOTA TODO: 
        log_error(logger_stdin, "Dato muy grande para el tamanio solicitado."); 
    }
    free(dato_a_escribir);
}

void peticion_STDOUT(SOLICITUD_INTERFAZ *interfaz_solicitada, INTERFAZ *io ){
    log_info(logger_stdout, "PID: %d - Operacion: %s", *interfaz_solicitada->pid, interfaz_solicitada->solicitud);

    char* registro_direccion = interfaz_solicitada->args[0];
    char* registro_tamanio = interfaz_solicitada->args[1];

    // Reservo memoria para los datos q vamos a enviar en el char**
    PAQUETE_LECTURA* plectura = malloc(sizeof(PAQUETE_LECTURA));
    plectura->direccion_fisica = strdup(registro_direccion);
    plectura->pid = *interfaz_solicitada->pid;
    plectura->tamanio = atoi(registro_tamanio) + 1;

    paquete_leer_memoria(io->sockets->conexion_memoria, plectura);

    // Recibir el dato de la direccion de memoria
    int cod_op = recibir_operacion(io->sockets->conexion_memoria);

    if(cod_op != RESPUESTA_LEER_MEMORIA){ /* ERROR OPERACION INVALIDA */ exit(-32); }

    t_list* lista = recibir_paquete(io->sockets->conexion_memoria, logger_stdout);
    
    char* leido = list_get(lista, 0);
    // Mostrar dato leido de memoria
    log_info(logger_stdout, "\nEl dato solicitado de memoria es: < %s >", leido);

    free(leido);
    leido = NULL;

    // Libero datos**
    free(plectura->direccion_fisica);
    plectura->direccion_fisica = NULL;
    
    list_destroy(lista);
}

void peticion_DIAL_FS(SOLICITUD_INTERFAZ *interfaz_solicitada, INTERFAZ *io){
    log_info(logger_dialfs, "PID: %d - Operacion: %s", *interfaz_solicitada->pid, interfaz_solicitada->solicitud);

    char* nombre_archivo = interfaz_solicitada->args[0];
    char* registro_direccion;
    char* registro_tamanio;
    char* registro_puntero_archivo;

    switch (dial_fs_parser(interfaz_solicitada->solicitud)){

    case DIALFS_CREATE: 
       crear_archivo(nombre_archivo);
       log_info(logger_dialfs, "PID: %d - Crear Archivo: %s", *interfaz_solicitada->pid, nombre_archivo);
        break;
    case DIALFS_DELETE:
        borrar_archivo(nombre_archivo);
        log_info(logger_dialfs, "PID: %d - Eliminar Archivo: %s", *interfaz_solicitada->pid, nombre_archivo);
        break;
    case DIALFS_TRUNCATE:
        int nuevo_tamanio = atoi(interfaz_solicitada->args[1]); // atoi(registro_tamanio)
        truncar(nombre_archivo, nuevo_tamanio, *interfaz_solicitada->pid);
        log_info(logger_dialfs, "PID: %d - Truncar Archivo: %s - Tamaño: %i", *interfaz_solicitada->pid, nombre_archivo, nuevo_tamanio);
        break;
    case DIALFS_WRITE:
        registro_direccion = interfaz_solicitada->args[1];       // direccion de memoria de la que se obtiene el dato a escribir
        registro_tamanio = interfaz_solicitada->args[2];         // tamaño del dato a leer en memoria
        registro_puntero_archivo = interfaz_solicitada->args[3]; // posicion del archivo a partir de la que debo escribir
        dial_fs_write(io, *interfaz_solicitada->pid, nombre_archivo, registro_direccion, registro_tamanio, registro_puntero_archivo);
        log_info(logger_dialfs, "PID: %d - Escribir Archivo: %s - Tamaño a Escribir: %s - Puntero Archivo: %s", *interfaz_solicitada->pid, nombre_archivo, registro_tamanio, registro_puntero_archivo);
        break;
    case DIALFS_READ:
        registro_direccion = interfaz_solicitada->args[1];       // direccion de memoria en la que voy a escribir el dato
        registro_tamanio = interfaz_solicitada->args[2];         // tamaño del dato a leer en el archivo
        registro_puntero_archivo = interfaz_solicitada->args[3]; // posicion del archivo a partir de la que debo leer
        dial_fs_read(io, *interfaz_solicitada->pid, nombre_archivo, registro_direccion, registro_tamanio, registro_puntero_archivo);
        log_info(logger_dialfs, "PID: %d - Leer Archivo: %s - Tamaño a Leer: %s - Puntero Archivo: %s", *interfaz_solicitada->pid, nombre_archivo, registro_tamanio, registro_puntero_archivo);
        break;    
    default:
        log_error(logger_dialfs, "Operacion invalida");
        break;
    }
    
}

op_code dial_fs_parser(char* command) {
    if (strcmp(command, "IO_FS_CREATE") == 0) {
        return DIALFS_CREATE;
    } else if (strcmp(command, "IO_FS_DELETE") == 0) {
        return DIALFS_DELETE;
    } else if (strcmp(command, "IO_FS_TRUNCATE") == 0) {
        return DIALFS_TRUNCATE;
    } else if (strcmp(command, "IO_FS_WRITE") == 0) {
        return DIALFS_WRITE;
    } else if (strcmp(command, "IO_FS_READ") == 0) {
        return DIALFS_READ;
    }else{
        log_error(logger_dialfs,"PUSISTE UNA SOLICITUD EQUIVOCADA BROTHER");
    }
    return 0;
}

void recibir_peticiones_interfaz(INTERFAZ* interfaz, int cliente_fd, t_log* logger){

    SOLICITUD_INTERFAZ *solicitud;
    t_list *lista;
    desbloquear_io* aux;

    while (1) {
        
        int cod_op = recibir_operacion(cliente_fd);
        switch (cod_op) {

        case IO_GENERICA:
            lista = recibir_paquete(cliente_fd, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_IO_GEN(solicitud, interfaz);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(interfaz->sockets->conexion_kernel, aux);
            string_array_destroy(solicitud->args);

            list_destroy(lista);
            break;

        case IO_STDIN:
            lista = recibir_paquete(cliente_fd, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_STDIN(solicitud, interfaz);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(interfaz->sockets->conexion_kernel, aux);
            string_array_destroy(solicitud->args);
            
            list_destroy(lista);
            break;

        case IO_STDOUT:
            lista = recibir_paquete(interfaz->sockets->conexion_kernel, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_STDOUT(solicitud, interfaz);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(interfaz->sockets->conexion_kernel, aux);
            string_array_destroy(solicitud->args);
            
            list_destroy(lista);
            break;

        case IO_DIALFS:
            lista = recibir_paquete(interfaz->sockets->conexion_kernel, logger);
            solicitud = asignar_espacio_a_solicitud(lista);
            peticion_DIAL_FS(solicitud, interfaz);

            aux = crear_solicitud_desbloqueo(solicitud->nombre, solicitud->pid);
            paqueteDeDesbloqueo(interfaz->sockets->conexion_kernel, aux);
            string_array_destroy(solicitud->args);
            
            list_destroy(lista);
            break;

        case DESCONECTAR_IO:
            lista = recibir_paquete(interfaz->sockets->conexion_kernel, logger);
            sem_post(&desconexion_io);
            
            list_destroy(lista);
            break;

        default:
            return;
        }
    }
}

void *correr_interfaz(INTERFAZ* interfaz){

    // TOMA DATOS DE KERNEL DE EL CONFIG
    char *ip_kernel = config_get_string_value(interfaz->configuration, "IP_KERNEL");
    char *puerto_kernel = config_get_string_value(interfaz->configuration, "PUERTO_KERNEL");

    // CREA LA CONEXION CON KERNEL  
    interfaz->sockets->conexion_kernel = crear_conexion(ip_kernel, puerto_kernel);
    log_info(entrada_salida, "La interfaz %s está conectandose a kernel", interfaz->sockets->nombre);
    paquete_nueva_IO(interfaz->sockets->conexion_kernel, interfaz); // ENVIA PAQUETE A KERNEL
    log_warning(entrada_salida, "Conexion creada con Kernel -  PUERTO: %s  -  IP: %s\n", puerto_kernel, ip_kernel);

    if(interfaz->datos->tipo != GENERICA){

        // TOMA DATOS DE MEMORIA DE EL CONFIG
        char *ip_memoria = config_get_string_value(interfaz->configuration, "IP_MEMORIA");
        char *puerto_memoria = config_get_string_value(interfaz->configuration, "PUERTO_MEMORIA");

        // CREA LA CONEXION CON MEMORIA salvaje
        interfaz->sockets->conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
        log_info(entrada_salida, "La interfaz %s está conectandose a memoria \n", interfaz->sockets->nombre);
        paquete_llegada_io_memoria(interfaz->sockets->conexion_memoria, interfaz->sockets); // ENVIA PAQUETE A MEMORIA
        log_warning(entrada_salida, "Conexion creada con Memoria -  PUERTO: %s  -  IP: %s\n", puerto_memoria, ip_memoria);
    }    

    if (interfaz->datos->tipo == DIAL_FS) {
        directorio_interfaces = strdup(config_get_string_value(interfaz->configuration, "PATH_BASE_DIALFS"));

        retraso_compactacion = config_get_int_value(interfaz->configuration, "RETRASO_COMPACTACION");
        block_count = config_get_int_value(interfaz->configuration, "BLOCK_COUNT");
        block_size = config_get_int_value(interfaz->configuration, "BLOCK_SIZE");

        char* path_bloques = string_new();
        char* nombre_bloques = string_new();
        char* path_bitmap = string_new();
        char* nombre_bitmap = string_new();
        char* metadata_path = string_new();
        
        string_append(&path_bloques, directorio_interfaces);
        string_append(&path_bloques, "/");
        string_append(&path_bloques, interfaz->sockets->nombre);
        string_append(&path_bloques, "_bloques.dat");
        
        string_append(&path_bitmap, directorio_interfaces);
        string_append(&path_bitmap, "/");
        string_append(&path_bitmap, interfaz->sockets->nombre);
        string_append(&path_bitmap, "_bitmap.dat");
        
        string_append(&metadata_path, directorio_interfaces);
        string_append(&metadata_path, "/");

        metadata_files = list_create();

        iniciar_archivo_bloques(path_bloques);
        crear_y_mapear_bitmap(path_bitmap);
        listar_archivos_metadata(metadata_path);
        
        string_append(&nombre_bloques, interfaz->sockets->nombre);
        string_append(&nombre_bloques, "_bloques.dat");
        string_append(&nombre_bitmap, interfaz->sockets->nombre);
        string_append(&nombre_bitmap, "_bitmap.dat");
        eliminar_archivo_de_lista(nombre_bloques);
        eliminar_archivo_de_lista(nombre_bitmap);
    }
    recibir_peticiones_interfaz(interfaz, interfaz->sockets->conexion_kernel, entrada_salida);   
    return NULL;
}

void iniciar_interfaz(char *nombre, t_config *config, t_log *logger){

    INTERFAZ* interfaz = malloc(sizeof(INTERFAZ));

    interfaz->configuration = config;

    interfaz->procesos_bloqueados = NULL;

    interfaz->datos = malloc(sizeof(DATOS_INTERFAZ));
    interfaz->datos->tipo = get_tipo_interfaz(interfaz, config_get_string_value(interfaz->configuration, "TIPO_INTERFAZ"));

    interfaz->sockets = malloc(sizeof(DATOS_CONEXION));
    interfaz->sockets->nombre = strdup(nombre);
    
    copiar_operaciones(interfaz);  
    correr_interfaz(interfaz);
}

void conectar_interfaces(){
    
    int opcion;
    printf("SELECCIONE EL TIPO DE INTERFAZ\n");

    printf("1. Conectar interfaz GENERICA \n");
    printf("2. Conectar interfaz STDIN    \n");
    printf("3. Conectar interfaz STDOUT   \n");
    printf("4. Conectar interfaz DIALFS   \n");
    printf("6. Salir \n");

    printf("Seleccione una opcion: ");
    scanf("%d", &opcion);
        
    switch (opcion) {

    case CONECTAR_GENERICA:
        printf("Conectando interfaz Generica... \n\r ");
        fflush(stdout);
        iniciar_configuracion();
        terminar_programa(logger_io_generica, config_generica);
        break;

    case CONECTAR_STDIN:
        printf("Conectando interfaz STDIN... \n\r ");
        fflush(stdout);
        iniciar_interfaz("TECLADO", config_stdin, logger_stdin);
        break;

    case CONECTAR_STDOUT:
        printf("Conectando interfaz STDOUT... \n ");
        fflush(stdout);
        iniciar_interfaz("MONITOR", config_stdout, logger_stdout);
        break;

    case CONECTAR_DIALFS:
        printf("Conectando interfaz DIALFS... \n ");
        iniciar_interfaz("FS", config_dialfs, logger_dialfs);
        break;

    case SALIR:
        return;

    default:
        printf("Opcion no valida. Por favor seleccione una opcion correcta \n");
        fflush(stdout);
        break;

    }
}

int main(int argc, char *argv[]){
    sem_init(&conexion_generica, 1, 0);
    sem_init(&desconexion_io, 1, 0);
    sem_init(&conexion_io, 1, 0);

    entrada_salida     = iniciar_logger("main.log", "io_general_log", LOG_LEVEL_INFO);
    logger_io_generica = iniciar_logger("io_generica.log", "io_generica_log", LOG_LEVEL_INFO);
    logger_stdin       = iniciar_logger("io_stdin.log", "io_stdin_log", LOG_LEVEL_INFO);
    logger_stdout      = iniciar_logger("io_stdout.log", "io_stdout_log", LOG_LEVEL_INFO);
    logger_dialfs      = iniciar_logger("io_dialfs.log", "io_dialfs_log", LOG_LEVEL_INFO);

    config_stdin       = iniciar_config("../entradasalida/configs/TECLADO.config");
    config_stdout      = iniciar_config("../entradasalida/configs/MONITOR.config");
    config_dialfs      = iniciar_config("../entradasalida/configs/FS.config");

    conectar_interfaces(); // CREA LA INTERFAZ A CONECTAR

    // LIBERA MEMORIA Y CONEXIONES
    sem_destroy(&desconexion_io);
    terminar_programa(logger_stdin, config_stdin);
    terminar_programa(logger_stdout, config_stdout);
    terminar_programa(logger_dialfs, config_dialfs);

    return 0;
}

t_config* iniciar_configuracion(){
    t_config* configuracion;
    
    printf("1. Conectar SLP1\n");
    printf("2. Conectar ESPERA\n");
    printf("3. Conectar GENERICA\n");
    char* opcion_en_string = readline("Seleccione una opción: ");
    int opcion = atoi(opcion_en_string);
    free(opcion_en_string);

    switch (opcion)
        {
        case 1:
            log_info(logger_io_generica, "Conectando SLP1...");
            configuracion = iniciar_config("../entradasalida/configs/SLP1.config");
            iniciar_interfaz("SLP1", configuracion, logger_io_generica);
            break;
        case 2:
            log_info(logger_io_generica, "Conectando ESPERA...");
            configuracion = iniciar_config("../entradasalida/configs/ESPERA.config");
            iniciar_interfaz("ESPERA", configuracion, logger_io_generica);
            break;
        case 3:
            log_info(logger_io_generica, "Conectando GENERICA...");
            configuracion = iniciar_config("../entradasalida/configs/GENERICA.config");
            iniciar_interfaz("GENERICA", configuracion, logger_io_generica);
            break;
        default:
            log_info(logger_io_generica, "Conectando GENERICA...");
            configuracion = iniciar_config("../entradasalida/configs/GENERICA.config");
            iniciar_interfaz("GENERICA", configuracion, logger_io_generica);
            break;
        }

        return configuracion;
}