#include <cpu.h>

int main(int argc, char* argv[]) {   
    t_log* logger = iniciar_logger("../cpu/cpu.log", "cpu-log", LOG_LEVEL_INFO);

    char* config_path = "../cpu/cpu.config";

    t_config* config = iniciar_config(config_path);

    // Get info from cpu.config
    //char* ip_memoria = config_get_string_value(config,"IP_MEMORIA");
    //char* puerto_memoria = config_get_string_value(config,"PUERTO_MEMORIA");
    char* puerto_dispatch = config_get_string_value(config,"PUERTO_ESCUCHA_DISPATCH");
    char* puerto_interrupt = config_get_string_value(config,"PUERTO_ESCUCHA_INTERRUPT");
    //char* cant_ent_tlb = config_get_string_value(config,"CANTIDAD_ENTRADAS_TLB");
    //char* algoritmo_tlb = config_get_string_value(config,"ALGORITMO_TLB");

    ArgsAbrirServidor args = {logger, puerto_dispatch};

    abrir_servidor(&args);
    //abrir_servidor(logger, puerto_interrupt);

    terminar_programa(logger, config);
}
