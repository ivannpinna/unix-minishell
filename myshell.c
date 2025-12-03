#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "parser.h"

void cd_execute(tline *line);

int main() {

    char buffer[1024];
    tline *line;
    int i, j;
    pid_t pid;

    // Contrrol de señales
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    // Variables para la gestión de pipes
    int p[2];
    int fd_in;

    printf("msh> ");
    while (fgets(buffer, 1024, stdin)) {
        line = tokenize(buffer);

        // Si la linea esta vacia o hubo un error
        if (line == NULL || line->ncommands == 0) {
            printf("msh> ");
            continue;
        }

        // Comprobacion de mandatos
        if (strcmp(line->commands[0].argv[0], "exit") == 0) {
            exit(0);
        } else if (strcmp(line->commands[0].argv[0], "umask") == 0) {
            // Llamar a la funcion umask
        } else if (strcmp(line->commands[0].argv[0], "jobs") == 0) {
            // Llamar a la funcion jobs
        } else if (strcmp(line->commands[0].argv[0], "cd") == 0) {
            cd_execute(line);
        } else if (strcmp(line->commands[0].argv[0], "bg") == 0) {
            // Aquí iría la implementación de bg
        } else {
            fd_in = -1;

            for (i = 0; i < line->ncommands; i++) {

                if (i < line->ncommands - 1) {
                    pipe(p);
                }
                pid = fork();
                if (pid == -1) {
                    // Error al hacer el fork
                }
                else if (pid == 0) {
                    //Gestion de señales
                    if (line -> background) {
                        signal(SIGINT, SIG_IGN);
                    } else {
                        signal(SIGINT, SIG_DFL);
                    }

                    // Redireccion de entrada
                    if (i == 0) {
                        if (line->redirect_input != NULL) {
                            int file_in = open(line->redirect_input, O_RDONLY);
                            if (file_in == -1) {
                                fprintf(stderr, "Could not open redirection input\n");
                                exit(1);
                            }
                            dup2(file_in, STDIN_FILENO);
                            close(file_in);
                        }
                    }
                    else {
                        dup2(fd_in, STDOUT_FILENO);
                        close(fd_in);
                    }

                    // Redireccion de salida
                    if (i == line->ncommands - 1) {
                        if (line->redirect_output != NULL) {
                            int file_out = open(line->redirect_output, O_CREAT | O_TRUNC | O_WRONLY, 0666);
                            if (file_out == -1) {
                                fprintf(stderr, "Could not open redirection output\n");
                                exit(1);
                            }
                            dup2(file_out, STDOUT_FILENO);
                            close(file_out);
                        }
                        // Redireccion de salida de error
                        if (line->redirect_error != NULL) {
                            int file_err = open(line->redirect_error, O_CREAT | O_TRUNC | O_WRONLY, 0666);
                            if (file_err == -1) {
                                fprintf(stderr, "%s: Error. Descripción del error\n", line->redirect_error);
                                exit(1);
                            }
                            dup2(file_err, STDERR_FILENO);
                            close(file_err);
                        }
                    }
                    else {
                        dup2(p[1], STDIN_FILENO);
                        close(p[1]);
                        close(p[0]);
                    }

                    // Ejecucion
                    execvp(line->commands[i].filename, line->commands[i].argv);

                    // Error en el exec
                    fprintf(stderr, "%s: No se encuentra el mandato\n", line->commands[i].filename);
                    exit(1);
                }
                else { // Proceso padre

                    if (fd_in != -1) {
                        close(fd_in); // Cierra el pipe anterior
                    }

                    // Prepara para el siguiente mandato del pipe si lo hay
                    if (i < line->ncommands - 1) {
                        fd_in = p[0];
                        close(p[1]);
                    }
                }
            }
            // Espera de procesos
            if (line->background) {
                printf("[%d]\n", pid);
            }
            else {
                for (j = 0; j < line->ncommands; j++) {
                    wait(NULL);
                }
            }
        }
        printf("msh> ");
    }

    return 0;
}


void cd_execute(tline *line) {

    char *dir;
    char buffer[1024];

    // Si no indica el directorio mandamos a HOME
    if (line->commands[0].argc == 1) {
        dir = getenv("HOME");
    } else {
        dir = line -> commands[0].argv[1]; //Guardamos en dir, el directorio al que queremos cambiar
    }

    // Cambio de directorio y visualizacion de la nueva ruta
    if (chdir(dir) != 0) {
        perror("Imposible cambiar de directorio");
    } else {
        if (getcwd(buffer, sizeof(buffer)) != NULL) {
            printf("%s\n", buffer);
        } else {
            perror("cd: error obteniendo ruta actual");
        }
    }
}

