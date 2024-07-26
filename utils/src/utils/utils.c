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

    return !strcmp(interfaz->sockets->nombre, nombre);
}

void liberar_memoria(char **cadena, int longitud) {
    for (int i = 0; i < longitud; i++) {
        free(cadena[i]);
		cadena[i] = NULL;
    }
    free(cadena);
	cadena = NULL;
}

void destruir_interfaz(void* data){
    INTERFAZ* a_eliminar = (INTERFAZ*)data;
	destruir_datos_io(a_eliminar->sockets);
	pthread_join(a_eliminar->sockets->hilo_de_llegada_kernel, NULL);

	pthread_mutex_destroy(&a_eliminar->mutex);
    free(a_eliminar->datos);
	a_eliminar->datos = NULL;
	a_eliminar = NULL;
}

void destruir_datos_io(void* data){
	DATOS_CONEXION* datos = (DATOS_CONEXION*)data;
	free(datos->nombre);
	datos->nombre = NULL;
	free(datos);
	datos = NULL;
}


void buscar_y_desconectar(char* leido, t_list* interfaces, t_log* logger){
     bool es_nombre_de_interfaz_aux(void *data)
    {
        return es_nombre_de_interfaz(leido, data);
    };
    log_warning(logger, "Despedimos con un fuerte aplauso por favor a %s. Gracias por todo loco!", leido);
 
    list_remove_and_destroy_by_condition(interfaces, es_nombre_de_interfaz_aux, destruir_interfaz);
}


void eliminar_io_solicitada(void* data){
	SOLICITUD_INTERFAZ* soli_a_eliminar = (SOLICITUD_INTERFAZ*)data;

    string_array_destroy(soli_a_eliminar->args);
	free(soli_a_eliminar->nombre);
	soli_a_eliminar->nombre = NULL;
	free(soli_a_eliminar->pid);
	soli_a_eliminar->pid = NULL;
	free(soli_a_eliminar->solicitud);
	soli_a_eliminar->solicitud = NULL;
	free(soli_a_eliminar);
	soli_a_eliminar = NULL;
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

void paquete_marco(int conexion, PAQUETE_MARCO *marco_paquete)
{	
	t_paquete* paquete;
	paquete = crear_paquete(ACCEDER_MARCO);

	agregar_a_paquete(paquete, marco_paquete, sizeof(marco_paquete));

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paquete_resize(int conexion, t_resize* dato)
{	
	t_paquete* paquete;
	paquete = crear_paquete(RESIZE);

	agregar_a_paquete(paquete, dato, sizeof(t_resize));
	agregar_a_paquete(paquete, dato->tamanio, strlen(dato->tamanio) + 1);

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paquete_leer_memoria(int conexion, PAQUETE_LECTURA* paquete_lectura)
{	
	t_paquete* paquete;
	paquete = crear_paquete(LEER_MEMORIA);

	agregar_a_paquete(paquete, paquete_lectura->direccion_fisica, strlen(paquete_lectura->direccion_fisica) + 1);
	agregar_a_paquete(paquete, paquete_lectura->tamanio, strlen(paquete_lectura->tamanio) + 1);
	agregar_a_paquete(paquete, paquete_lectura->pid, strlen(paquete_lectura->pid) + 1);

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paquete_copy_string(int conexion, PAQUETE_COPY_STRING* paquete_copy_string)
{	
	t_paquete* paquete;
	paquete = crear_paquete(COPY_STRING);

	agregar_a_paquete(paquete, paquete_copy_string->direccion_fisica_origen, strlen(paquete_copy_string->direccion_fisica_origen) + 1);
	agregar_a_paquete(paquete, paquete_copy_string->direccion_fisica_destino, strlen(paquete_copy_string->direccion_fisica_destino) + 1);
	agregar_a_paquete(paquete, paquete_copy_string->tamanio, strlen(paquete_copy_string->tamanio) + 1);
	agregar_a_paquete(paquete, paquete_copy_string->pid, strlen(paquete_copy_string->pid) + 1);

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paqueT_dato(int conexion, t_dato* data)
{	
	t_paquete* paquete;
	paquete = crear_paquete(RESPUESTA_LEER_MEMORIA);

	agregar_a_paquete(paquete, (char*)data->data, data->tamanio);
	
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paquete_escribir_memoria(int conexion, PAQUETE_ESCRITURA* paquete_escritura)
{	
	t_paquete* paquete;
	paquete = crear_paquete(ESCRIBIR_MEMORIA);

	agregar_a_paquete(paquete, paquete_escritura, sizeof(PAQUETE_ESCRITURA));
	agregar_a_paquete(paquete, paquete_escritura->direccion_fisica, strlen(paquete_escritura->direccion_fisica) + 1);
	agregar_a_paquete(paquete, paquete_escritura->dato, sizeof(t_dato));
	agregar_a_paquete(paquete, paquete_escritura->dato->data, paquete_escritura->dato->tamanio);

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paquete_creacion_proceso(int conexion, c_proceso_data* data)
{	
	t_paquete* paquete;
	paquete = crear_paquete(CREAR_PROCESO);

	agregar_a_paquete(paquete, string_itoa(data->id_proceso), strlen(string_itoa(data->id_proceso)) + 1);
	agregar_a_paquete(paquete, data->path, strlen(data->path) + 1);

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paquete_solicitud_instruccion(int conexion, t_instruccion* fetch){
	t_paquete* paquete;
	paquete = crear_paquete(INSTRUCCION);

	agregar_a_paquete(paquete, fetch->pc, strlen(fetch->pc) + 1);
	agregar_a_paquete(paquete, fetch->pid, strlen(fetch->pid) + 1);

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void peticion_de_espacio_para_pcb(int conexion, pcb* process, op_code codigo){
	t_paquete* paquete;
	paquete = crear_paquete(codigo);

	agregar_a_paquete(paquete, &process, sizeof(process));
	agregar_a_paquete(paquete, &process->recursos_adquiridos, sizeof(process->recursos_adquiridos));
	agregar_a_paquete(paquete, &process->contexto, sizeof(cont_exec));
	agregar_a_paquete(paquete, &process->contexto->registros, sizeof(regCPU));

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void peticion_de_eliminacion_espacio_para_pcb(int conexion, pcb* process, op_code codigo){
	t_paquete* paquete;
	paquete = crear_paquete(codigo);

	agregar_a_paquete(paquete, &process, sizeof(process));
	agregar_a_paquete(paquete, process->estadoActual, strlen(process->estadoActual) + 1);
	agregar_a_paquete(paquete, process->estadoAnterior, strlen(process->estadoAnterior) + 1);
	agregar_a_paquete(paquete, process->contexto, sizeof(cont_exec));
	agregar_a_paquete(paquete, process->contexto->registros, sizeof(*(process->contexto->registros)));
	//agregar_a_paquete(paquete, process->contexto->registros->PTBR, sizeof(process->contexto->registros->PTBR));

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paqueteIO(int conexion, SOLICITUD_INTERFAZ* solicitud, cont_exec* contexto){
	t_paquete* paquete;

	paquete = crear_paquete(SOLICITUD_IO);
	agregar_a_paquete(paquete, contexto, sizeof(contexto));
	agregar_a_paquete(paquete, contexto->registros, sizeof(*(contexto->registros)));
	agregar_a_paquete(paquete, solicitud->nombre, strlen(solicitud->nombre) + 1);
	agregar_a_paquete(paquete, solicitud->solicitud, strlen(solicitud->solicitud) + 1);

	int argumentos = string_array_size(solicitud->args);

	for(int i = 0; i < argumentos; i++){
		agregar_a_paquete(paquete, solicitud->args[i], strlen(solicitud->args[i]) + 1);
	}

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void enviar_solicitud_io(int conexion, SOLICITUD_INTERFAZ* solicitud, op_code tipo){
	t_paquete* paquete;

	paquete = crear_paquete(tipo);
	agregar_a_paquete(paquete, &solicitud, sizeof(SOLICITUD_INTERFAZ));
	agregar_a_paquete(paquete, solicitud->nombre, strlen(solicitud->nombre) + 1);
	agregar_a_paquete(paquete, solicitud->solicitud, strlen(solicitud->solicitud) + 1);
	agregar_a_paquete(paquete, solicitud->pid, strlen(solicitud->pid) + 1);
	//agregar_a_paquete(paquete, &solicitud->args, sizeof(solicitud->args));
	
	int argumentos = string_array_size(solicitud->args);

	for(int i = 0; i < argumentos; i++){
		agregar_a_paquete(paquete, solicitud->args[i], strlen(solicitud->args[i]) + 1);
	}

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paquete_guardar_en_memoria(int conexion, pcb* proceso_en_ram){
	t_paquete* paquete;

	paquete = crear_paquete(SOLICITUD_MEMORIA);
	agregar_a_paquete(paquete, string_itoa(proceso_en_ram->contexto->PID), strlen(string_itoa(proceso_en_ram->contexto->PID)) + 1);
	agregar_a_paquete(paquete, &proceso_en_ram->contexto->registros, sizeof(proceso_en_ram->contexto->registros));
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paquete_nueva_IO(int conexion, INTERFAZ* interfaz){
	t_paquete* paquete;

	paquete = crear_paquete(NUEVA_IO);

	agregar_a_paquete(paquete, &interfaz, sizeof(INTERFAZ));
	agregar_a_paquete(paquete, interfaz->datos, sizeof(DATOS_INTERFAZ));	
	agregar_a_paquete(paquete, &interfaz->sockets, sizeof(DATOS_CONEXION));
	agregar_a_paquete(paquete, interfaz->sockets->nombre, strlen(interfaz->sockets->nombre) + 1);
	//agregar_a_paquete(paquete, &interfaz->datos->operaciones, sizeof(interfaz->datos->operaciones));

	int operaciones = string_array_size(interfaz->datos->operaciones);

	for(int i = 0; i < operaciones; i++){
		agregar_a_paquete(paquete, interfaz->datos->operaciones[i], strlen(interfaz->datos->operaciones[i]) + 1);
	}
	
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paquete_llegada_io_memoria(int conexion, DATOS_CONEXION* interfaz){
	t_paquete* paquete;
	paquete = crear_paquete(NUEVA_IO);

	agregar_a_paquete(paquete, interfaz, sizeof(interfaz));
	agregar_a_paquete(paquete, interfaz->nombre, strlen(interfaz->nombre) + 1);

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void paqueteRecurso(int conexion, cont_exec* contexto, char* recurso, op_code op_recurso){
	t_paquete* paquete;

	paquete = crear_paquete(op_recurso);

	agregar_a_paquete(paquete, contexto, sizeof(contexto));
	agregar_a_paquete(paquete, contexto->registros, sizeof(contexto->registros));
	agregar_a_paquete(paquete, recurso, strlen(recurso) + 1);

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

void paquete_memoria_io(int conexion, char* dato){
	t_paquete* paquete;
	paquete = crear_paquete(RESPUESTA_LEER_MEMORIA);

	agregar_a_paquete(paquete, dato, strlen(dato) + 1);

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void enviar_contexto_pcb(int conexion, cont_exec* contexto, op_code codigo)
{	
	t_paquete* paquete;
	paquete = crear_paquete(codigo);
	
	agregar_a_paquete(paquete, contexto, sizeof(contexto));
	agregar_a_paquete(paquete, contexto->registros, sizeof(*(contexto->registros)));
	
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

	if (setsockopt(socket_servidor, SOL_SOCKET,SO_REUSEADDR,&(int){1}, sizeof(int)) < 0) {
		log_error(logger, "setsockopt(SO_REUSEADDR) failed.");
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

void recibir_mensaje(int socket_cliente, t_log* logger, op_code codigo)
{
	int size;
	void* buffer = recibir_buffer(&size, socket_cliente);

	char* mensaje = strdup((char*)buffer);

	switch (codigo){
	case MENSAJE:
		log_info(logger, "MENSAJE > %s", mensaje);
		break;
	default:
		break;
	}
	free(buffer);
	buffer = NULL;
	free(mensaje);
	mensaje = NULL;
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

