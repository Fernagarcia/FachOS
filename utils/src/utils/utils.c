#include <utils/utils.h>

t_log* iniciar_logger(char* log_path, char* log_name, t_log_level log_level)
{
	t_log* nuevo_logger;
	
	nuevo_logger = log_create(log_path, log_name, 1, log_level);

	return nuevo_logger;
}

t_config* iniciar_config(char* config_path)
{
    t_config* nuevo_config;
    nuevo_config = config_create(config_path);

    return nuevo_config;
}

void terminar_programa(t_log* logger, t_config* config)
{
	log_destroy(logger);

	config_destroy(config);
}

void eliminarEspaciosBlanco(char *cadena) {
    int i = strlen(cadena) - 1;

    while (isspace(cadena[i])) {
        i--;
    }
    cadena[i + 1] = '\0';
}

bool es_nombre_de_interfaz(char *nombre, void *data)
{
    INTERFAZ *interfaz = (INTERFAZ *)data;

    return !strcmp(interfaz->datos->nombre, nombre);
}

bool es_nombre_de_interfaz_io(char *nombre, void *data)
{
    INTERFAZ_CON_HILO *interfaz_con_hilo = (INTERFAZ_CON_HILO *)data;

    return !strcmp(interfaz_con_hilo->interfaz->datos->nombre, nombre);
}

void liberar_memoria(char **cadena, int longitud) {
    for (int i = 0; i < longitud; ++i) {
        free(cadena[i]);
		cadena[i] = NULL;
    }
    free(cadena);
	cadena = NULL;
}

void destruir_interfaz(void* data){
    INTERFAZ* a_eliminar = (INTERFAZ*)data;
	int operaciones = sizeof(a_eliminar->datos->operaciones) / sizeof(a_eliminar->datos->operaciones[0]);
    liberar_memoria(a_eliminar->datos->operaciones, operaciones);
    free(a_eliminar->datos->nombre);
	a_eliminar->datos->nombre = NULL;
    free(a_eliminar->datos);
	a_eliminar->datos = NULL;
	if(a_eliminar->solicitud != NULL){
		eliminar_io_solicitada(a_eliminar->solicitud);
	}
	a_eliminar = NULL;
}

void destruir_interfaz_io(void* data){
    INTERFAZ_CON_HILO* a_eliminar = (INTERFAZ_CON_HILO*)data;
	int operaciones = sizeof(a_eliminar->interfaz->datos->operaciones) / sizeof(a_eliminar->interfaz->datos->operaciones[0]);
    liberar_memoria(a_eliminar->interfaz->datos->operaciones, operaciones);
    free(a_eliminar->interfaz->datos->nombre);
	a_eliminar->interfaz->datos->nombre = NULL;
    free(a_eliminar->interfaz->datos);
	a_eliminar->interfaz->datos = NULL;
	if(a_eliminar->interfaz->solicitud != NULL){
		eliminar_io_solicitada(a_eliminar->interfaz->solicitud);
	}
	a_eliminar->interfaz = NULL;
	// TODO revisar el borrado del hilo
	pthread_join(a_eliminar->hilo_interfaz,NULL);
	a_eliminar->hilo_interfaz = NULL;
	a_eliminar = NULL;
}

void buscar_y_desconectar(char* leido, t_list* interfaces, t_log* logger){
     bool es_nombre_de_interfaz_aux(void *data)
    {
        return es_nombre_de_interfaz(leido, data);
    };
    log_info(logger, "Se desconecto la interfaz %s", leido);

    list_remove_and_destroy_by_condition(interfaces, es_nombre_de_interfaz_aux, destruir_interfaz);
}

void buscar_y_desconectar_io(char* leido, t_list* interfaces, t_log* logger){
     bool es_nombre_de_interfaz_aux(void *data)
    {
        return es_nombre_de_interfaz_io(leido, data);
    };
    log_info(logger, "Se desconecto la interfaz %s", leido);

    list_remove_and_destroy_by_condition(interfaces, es_nombre_de_interfaz_aux, destruir_interfaz);
}

void eliminar_io_solicitada(SOLICITUD_INTERFAZ* io_solicitada){
	int cantidad_argumentos = sizeof(io_solicitada->args) / sizeof(io_solicitada->args[0]);

    liberar_memoria(io_solicitada->args, cantidad_argumentos);
	free(io_solicitada->nombre);
	io_solicitada->nombre = NULL;
	free(io_solicitada->pid);
	io_solicitada->pid = NULL;
	free(io_solicitada->solicitud);
	io_solicitada->solicitud = NULL;
	io_solicitada = NULL;
}

// -------------------------------------- CLIENTE --------------------------------------  


void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints, *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

	connect(socket_cliente, server_info->ai_addr,server_info->ai_addrlen);
	freeaddrinfo(server_info);
	return socket_cliente;
}

void enviar_operacion(char* mensaje, int socket_cliente, op_code cod_op)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = cod_op;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}

void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(op_code codigo)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = codigo;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);
	
	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}

void paqueteDeMensajes(int conexion, char* mensaje, op_code codigo)
{	
	t_paquete* paquete;
	paquete = crear_paquete(codigo);

	agregar_a_paquete(paquete, mensaje, strlen(mensaje) + 1);

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void peticion_de_espacio_para_pcb(int conexion, pcb* process, op_code codigo){
	t_paquete* paquete;
	paquete = crear_paquete(codigo);

	agregar_a_paquete(paquete, &process, sizeof(process));
	agregar_a_paquete(paquete, process->path_instrucciones, strlen(process->path_instrucciones) + 1);
	agregar_a_paquete(paquete, &process->contexto, sizeof(process->contexto));
	agregar_a_paquete(paquete, &process->contexto->registros, sizeof(process->contexto->registros));

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void peticion_de_eliminacion_espacio_para_pcb(int conexion, pcb* process, op_code codigo){
	t_paquete* paquete;
	paquete = crear_paquete(codigo);

	agregar_a_paquete(paquete, &process, sizeof(process));
	agregar_a_paquete(paquete, process->path_instrucciones, strlen(process->path_instrucciones) + 1);
	agregar_a_paquete(paquete, process->estadoActual, strlen(process->path_instrucciones) + 1);
	agregar_a_paquete(paquete, process->estadoAnterior, strlen(process->path_instrucciones) + 1);
	agregar_a_paquete(paquete, process->contexto, sizeof(process->contexto));
	agregar_a_paquete(paquete, process->contexto->registros, sizeof(process->contexto->registros));

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paqueteIO(int conexion, SOLICITUD_INTERFAZ* solicitud, cont_exec* contexto){
	t_paquete* paquete;

	paquete = crear_paquete(SOLICITUD_IO);
	agregar_a_paquete(paquete, contexto, sizeof(contexto));
	agregar_a_paquete(paquete, contexto->registros, sizeof(contexto->registros));
	agregar_a_paquete(paquete, &solicitud, sizeof(solicitud));
	agregar_a_paquete(paquete, solicitud->nombre, strlen(solicitud->nombre) + 1);
	agregar_a_paquete(paquete, solicitud->solicitud, strlen(solicitud->solicitud) + 1);
	agregar_a_paquete(paquete, &(solicitud->args), sizeof(solicitud->args));

	int argumentos = sizeof(solicitud->args) / sizeof(solicitud->args[0]);

	for(int i = 0; i < argumentos; i++){
		agregar_a_paquete(paquete, solicitud->args[i], strlen(solicitud->args[i]) + 1);
	}

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void enviar_solicitud_io(int conexion, SOLICITUD_INTERFAZ* solicitud, op_code tipo){
	t_paquete* paquete;

	paquete = crear_paquete(tipo);
	agregar_a_paquete(paquete, &solicitud, sizeof(solicitud));
	agregar_a_paquete(paquete, solicitud->nombre, strlen(solicitud->nombre) + 1);
	agregar_a_paquete(paquete, solicitud->solicitud, strlen(solicitud->solicitud) + 1);
	agregar_a_paquete(paquete, solicitud->pid, strlen(solicitud->pid) + 1);
	agregar_a_paquete(paquete, &(solicitud->args), sizeof(solicitud->solicitud));

	int argumentos = sizeof(solicitud->args) / sizeof(solicitud->args[0]);

	for(int i = 0; i < argumentos; i++){
		agregar_a_paquete(paquete, solicitud->args[i], strlen(solicitud->args[i]) + 1);
	}

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paquete_nueva_IO(int conexion, INTERFAZ* interfaz){
	t_paquete* paquete;

	paquete = crear_paquete(NUEVA_IO);

	agregar_a_paquete(paquete, &interfaz, sizeof(interfaz));
	agregar_a_paquete(paquete, interfaz->datos, sizeof(interfaz->datos));
	agregar_a_paquete(paquete, interfaz->datos->nombre, strlen(interfaz->datos->nombre) + 1);
	agregar_a_paquete(paquete, &(interfaz->datos->operaciones), sizeof(interfaz->datos->operaciones));

	int operaciones = sizeof(interfaz->datos->operaciones) / sizeof(interfaz->datos->operaciones[0]);

	for(int i = 0; i < operaciones; i++){
		agregar_a_paquete(paquete, interfaz->datos->operaciones[i], strlen(interfaz->datos->operaciones[i]) + 1);
	}

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paqueteRecurso(int conexion, char* recurso, int op_recurso){
	t_paquete* paquete;

	paquete = crear_paquete(RECURSO);

	agregar_a_paquete(paquete, recurso, strlen(recurso) + 1);
	agregar_a_paquete(paquete, op_recurso, sizeof(op_recurso));

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paqueteDeDesbloqueo(int conexion, desbloquear_io *solicitud){
	t_paquete* paquete;
	paquete = crear_paquete(DESBLOQUEAR_PID);
	
	agregar_a_paquete(paquete, solicitud, sizeof(solicitud));
	agregar_a_paquete(paquete, solicitud->pid, strlen(solicitud->pid) + 1);
	agregar_a_paquete(paquete, solicitud->nombre, strlen(solicitud->nombre) + 1);
	
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void enviar_contexto_pcb(int conexion, cont_exec* contexto, op_code codigo)
{	
	t_paquete* paquete;
	paquete = crear_paquete(codigo);
	
	agregar_a_paquete(paquete, contexto, sizeof(contexto));
	agregar_a_paquete(paquete, contexto->registros, sizeof(contexto->registros));
	
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

// -------------------------------------- SERVER --------------------------------------  

t_log* logger;

int iniciar_servidor(t_log* logger, char* puerto_escucha)
{
	int socket_servidor;
	int err;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto_escucha, &hints, &servinfo);

	// fd = file descriptor
	err = socket_servidor = socket(servinfo->ai_family,
							servinfo->ai_socktype,
							servinfo->ai_protocol);

	if (socket_servidor == -1) {
		log_error(logger, "Error en socket: %s", strerror(errno));
		exit(-1);
	}
	err = bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
	err = listen(socket_servidor, SOMAXCONN);

	if (err == -1) {
		log_error(logger, "Error en escucha: %s", strerror(errno));
	}

	freeaddrinfo(servinfo);
	log_trace(logger, "Listo para escuchar a mi cliente");
	return socket_servidor;
}

int esperar_cliente(int socket_servidor, t_log* logger)
{
	int socket_cliente;

	socket_cliente = accept(socket_servidor, NULL, NULL);

	log_info(logger, "Se conecto un cliente!");

	return socket_cliente;
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void* recibir_buffer(int* size, int socket_cliente)
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

void* recibir_mensaje(int socket_cliente, t_log* logger, op_code codigo)
{
	int size;
	void* buffer = recibir_buffer(&size, socket_cliente);

	char* mensaje = strdup((char*)buffer);

	switch (codigo){
	case MENSAJE:
		log_info(logger, "MENSAJE > %s", mensaje);
		break;
	case DESCARGAR_INSTRUCCIONES:
		log_info(logger, "De CPU: %s", mensaje);
		break;
	default:
		break;
	}
	free(buffer);
	return mensaje;
}

t_list* recibir_paquete(int socket_cliente, t_log* logger)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer + desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

