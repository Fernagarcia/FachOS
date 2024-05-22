#include<cpu_instructions.h>

REGISTER* find_register(const char *name, regCPU* registers) {
    // Mapping
    REGISTER register_map[] = {
        {"PC", &registers->PC, "b"},
        {"AX", &registers->AX, "a"},
        {"BX", &registers->BX, "a"},
        {"CX", &registers->CX, "a"},
        {"DX", &registers->DX, "a"},
        {"EAX", &registers->EAX, "b"},
        {"EBX", &registers->EBX, "b"},
        {"ECX", &registers->ECX, "b"},
        {"EDX", &registers->EDX, "b"},
        {"SI", &registers->SI, "b"},
        {"DI", &registers->DI, "b"}
    };

    const int num_register = sizeof(register_map) / sizeof(REGISTER);

    for (int i = 0; i < num_register; ++i) {
        if (strcmp(register_map[i].name, name) == 0) {
            return &register_map[i];
        }
    }
    return NULL;
}

void set(char **params, regCPU *registers) {
  printf("Ejecutando instruccion set\n");
  printf("Me llegaron los parametros: %s, %s\n", params[0], params[1]);

    const char* register_name = params[0];
    int new_register_value = atoi(params[1]);


    REGISTER* found_register = find_register(register_name, registers);
    if (found_register != NULL) {
        if (strcmp(found_register->type, "a")) {
            *(uint8_t*)found_register = new_register_value; // Si el registro es de tipo 'a'
        } else {
            *(uint32_t*)found_register = new_register_value; // Si el registro es de tipo 'b'
        }
        printf("Valor del registro %s actualizado a %d\n", register_name, new_register_value);
    } else {
        printf("Registro desconocido: %s\n", register_name);
    }
}

void sum(char **params, regCPU *registers) {
    printf("Ejecutando instruccion sum");
    printf("Me llegaron los registros: %s, %s\n", params[0], params[1]);

    REGISTER* register_origin = find_register(params[0], registers);
    REGISTER* register_target = find_register(params[1], registers);


    if (register_origin != NULL && register_target != NULL) {
        if (register_origin->type == register_target->type) {
            if (strcmp(register_origin->type, "a")) {
                *(uint8_t*)register_target->ptr += *(uint8_t*)register_origin->ptr;
            } else {
                *(uint32_t*)register_target->ptr += *(uint32_t*)register_origin->ptr;
            }
            printf("Suma realizada y almacenada en %s\n", params[1]);
        } else {
            printf("Los registros no son del mismo tipo\n");
        }
    } else {
        printf("Alguno de los registros no fue encontrado\n");
    }
}

void sub(char **params, regCPU *registers) {
    printf("Ejecutando instruccion sub");
    printf("Me llegaron los registros: %s, %s\n", params[0], params[1]);

    REGISTER* register_origin = find_register(params[0], registers);
    REGISTER* register_target = find_register(params[1], registers);


    if (register_origin != NULL && register_target != NULL) {
        if (register_origin->type == register_target->type) {
            if (strcmp(register_origin->type, "a")) {
                *(uint8_t*)register_target->ptr -= *(uint8_t*)register_origin->ptr;
            } else {
                *(uint32_t*)register_target->ptr -= *(uint32_t*)register_origin->ptr;
            }
            printf("Suma realizada y almacenada en %s\n", params[1]);
        } else {
            printf("Los registros no son del mismo tipo\n");
        }
    } else {
        printf("Alguno de los registros no fue encontrado\n");
    }
}

void jnz(char **params, regCPU *registers) {
    printf("Ejecutando instruccion set\n");
    printf("Me llegaron los parametros: %s\n", params[0]);

    const char* register_name = params[0];
    const int next_instruction = atoi(params[1]);

    REGISTER* found_register_name = find_register(register_name, registers);

    if (found_register_name != NULL && found_register_name->ptr != NULL) {
        if (found_register_name->ptr != 0) {
            registers->PC = next_instruction;
        }
    } else {
        printf("Registro no encontrado o puntero nulo\n");
    }
}

    //TODO
void mov(char**, regCPU*){

}

void resize(char**, regCPU*){

}

void copy_string(char**, regCPU*){

}

void wait(char**, regCPU*){

}

void SIGNAL(char**, regCPU*){

}

void io_gen_sleep(char** params, regCPU*){
    char* interfaz= params[0];
    char* tiempo_a_esperar= params[1];
    // enviar a kernel la peticion de la interfaz con el argumento especificado, capaz no hace falta extraer cada char* de params, sino enviar todo params
    
}

void io_stdin_read(char**, regCPU*){

}

void mov_in(char**, regCPU*){

}

void mov_out(char**, regCPU*){
    
}