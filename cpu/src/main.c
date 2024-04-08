#include <stdlib.h>
#include <stdio.h>
#include <utils/server.h>
#include <utils/client.h>

int main(int argc, char* argv[]) {   
    logger = log_create("cpu-log.log", "CPU-Server", 1, LOG_LEVEL_DEBUG);

    int servidor = iniciar_servidor();

    if (servidor == -1) {
        log_error(logger, "No se pudo iniciar el servidor");
        exit(-1);
    }

    int cliente_fd = esperar_cliente(servidor);

}
