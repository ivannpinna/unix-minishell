#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "parser.h"

int main() {

    char buffer[1024];
    tline * line;

    //Aqui hay que implementar la gestion de señales
    printf("msh> ");
    while (fgets(buffer, 1024, stdin)) {
        line = tokenize(buffer);

        // Si la linea esta vacia o hubo un error
        if (line == NULL || line -> ncommands == 0) {
            printf("msh> ");
            continue;
        }

        // Comprobacion de mandatos
        if (strcmp(line->commands[0].filename, "exit") == 0) {
            exit(0);
        }
        else if (strcmp(line->commands[0].filename, "umask") == 0) {
            // Llamar a la funcion umask
        }
        else if (strcmp(line->commands[0].filename, "jobs") == 0) {
            // Llamar a la funcion jobs
        }
        else if (strcmp(line->commands[0].filename, "cd") == 0) {
            // Llamar a la funcion cd
        }
        else {
            // Ejecutar mandato externo
        }
        return 0;
    }

    return 0;
}
