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

void paqueteIO(int conexion, SOLICITUD_INTERFAZ* solicitud){
	t_paquete* paquete;

	paquete = crear_paquete(SOLICITUD_IO);
	agregar_a_paquete(paquete, solicitud, sizeof(solicitud));

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void enviar_contexto_pcb(int conexion, cont_exec* contexto)
{	
	t_paquete* paquete;
	paquete = crear_paquete(CONTEXTO);
	
	agregar_a_paquete(paquete, (void*)contexto, sizeof(contexto));
	
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

// -------------------------------------- SERVER --------------------------------------  

t_log* logger;

void* gestionar_llegada(void* args){
	ArgsGestionarServidor* args_entrada = (ArgsGestionarServidor*)args;

	void iterator_adapter(void* a) {
		iterator(args_entrada->logger, (char*)a);
	};

	t_list* lista;
	while (1) {
		log_info(args_entrada->logger, "Esperando operacion...");
		int cod_op = recibir_operacion(args_entrada->cliente_fd);
		switch (cod_op) {
		case MENSAJE:
			char* mensaje = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
			free(mensaje);
			break;
		case INSTRUCCION:
			char* instruccion = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, INSTRUCCION);
			free(instruccion);
			break;
		case CONTEXTO:
			lista = recibir_paquete(args_entrada->cliente_fd, logger);
			log_info(args_entrada->logger, "Me llegaron los siguientes valores:\n");
			list_iterate(lista, iterator_adapter);
			break;
		case -1:
			log_error(args_entrada->logger, "el cliente se desconecto. Terminando servidor");
			return EXIT_FAILURE;
		default:
			log_warning(args_entrada->logger,"Operacion desconocida. No quieras meter la pata");
			break;
		}
	}
}

void iterator(t_log* logger, char* value){
	log_info(logger,"%s", value);
}

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
	case PATH:
		log_info(logger, "INSTRUCTION PATH IN > %s", mensaje);
		break;
	case INSTRUCCION:
		log_info(logger, "NEXT INSTRUCTION > %s\n", mensaje);
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

