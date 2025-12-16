#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "parser.h"
#include <sys/stat.h> //Para el umask

#define MAX_JOBS 20

typedef struct {
    pid_t pid;
    char *command;
    char status; // 'R' = Running, 'S' = Stopped
} Job;
Job jobs_list[MAX_JOBS];
int n_jobs = 0;

void umask_execute(tline *line);
void cd_execute(tline *line);
void jobs_execute();
void bg_execute(tline *line);
void add_job(pid_t pid, char *command, char status);
void delete_job(int index);

int main() {

    char buffer[1024];
    tline *line;
    int i, j;
    pid_t pid;
    pid_t pid_bg;
    int status_bg;
	int status;

    // Contrrol de señales
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    // Variables para la gestión de pipes
    int p[2];
    int fd_in;

    printf("msh> ");
    while (fgets(buffer, 1024, stdin)) {
        line = tokenize(buffer);
        buffer[strcspn(buffer, "\n")] = 0;; //Para que se vean limpios los comandos del jobs
        // Zombies check
        for (int k = 0; k < n_jobs; k++) {
            // WNOHANG: No se bloquea si el proceso sigue vivo
            pid_bg = waitpid(jobs_list[k].pid, &status_bg, WNOHANG);

            if (pid_bg > 0) {
                // El proceso terminó. Avisamos y lo borramos.
                printf("[%d]+ Done\t%s\n", k + 1, jobs_list[k].command);
                delete_job(k);
                k--; // Retrocedemos índice porque la lista se ha desplazo
            }
        }

        // Si la linea esta vacia o hubo un error
        if (line == NULL || line->ncommands == 0) {
            printf("msh> ");
            continue;
        }

        // Comprobacion de mandatos
        if (strcmp(line->commands[0].argv[0], "exit") == 0) {
            exit(0);
        } else if (strcmp(line->commands[0].argv[0], "umask") == 0) {
            umask_execute(line);
        } else if (strcmp(line->commands[0].argv[0], "jobs") == 0) {
            jobs_execute();
        } else if (strcmp(line->commands[0].argv[0], "cd") == 0) {
            cd_execute(line);
        } else if (strcmp(line->commands[0].argv[0], "bg") == 0) {
            bg_execute(line);
        } else {
            fd_in = -1;

            for (i = 0; i < line->ncommands; i++) {

                if (i < line->ncommands - 1) {
                    pipe(p);
                }
                pid = fork();

                if (pid == 0) {
                    // Gestión de señales en el hijo
                    if (line->background) {
                        signal(SIGINT, SIG_IGN);
                        signal(SIGTSTP, SIG_IGN);
                    } else {
                        signal(SIGINT, SIG_DFL);
                        signal(SIGTSTP, SIG_DFL);
                    }

                    // Redirección de entrada
                    if (i == 0) { // Primer mandato
                        if (line->redirect_input != NULL) {
                            int file_in = open(line->redirect_input, O_RDONLY);
                            if (file_in == -1) {
                                perror("Error apertura entrada");
                                exit(1);
                            }
                            dup2(file_in, STDIN_FILENO);
                            close(file_in);
                        }
                    } else { // Mandatos intermedios o último
                        // El hijo lee del extremo de lectura del pipe anterior
                        dup2(fd_in, STDIN_FILENO);
                        close(fd_in);
                    }

                    // -Redirección de salida
                    if (i == line->ncommands - 1) { // Último mandato
                        if (line->redirect_output != NULL) {
                            int file_out = open(line->redirect_output, O_CREAT | O_TRUNC | O_WRONLY, 0666);
                            if (file_out == -1) {
                                perror("Error apertura salida");
                                exit(1);
                            }
                            dup2(file_out, STDOUT_FILENO);
                            close(file_out);
                        }
                        // Redirección de error
                        if (line->redirect_error != NULL) {
                            int file_err = open(line->redirect_error, O_CREAT | O_TRUNC | O_WRONLY, 0666);
                            if (file_err == -1) {
                                perror("Error apertura error");
                                exit(1);
                            }
                            dup2(file_err, STDERR_FILENO);
                            close(file_err);
                        }
                    } else { // Mandatos que NO son el último
                        // El hijo escribe en el extremo de escritura del pipe actual
                        close(p[0]); // Cerramos lectura que no usaremos
                        dup2(p[1], STDOUT_FILENO);
                        close(p[1]);
                    }

                    execvp(line->commands[i].filename, line->commands[i].argv);
                    fprintf(stderr, "%s: No se encuentra el mandato\n", line->commands[i].filename);
                    exit(1);
                }
                else { // PADRE
                    if (fd_in != -1) {
                        close(fd_in);
                    }
                    if (i < line->ncommands - 1) {
                        fd_in = p[0]; // Guardamos lectura para el siguiente hijo
                        close(p[1]);  // Cerramos la lectura
                    }
                }
            }
            // Espera de procesos
            if (line->background) {
                add_job(pid, buffer, 'R');
            }
            else {
                for (j = 0; j < line->ncommands; j++) {
                    pid_t child_pid = waitpid(-1, &status, WUNTRACED);

                    if (child_pid > 0) {

                        // Ctrl + Z (Proceso detenido)
                        if (WIFSTOPPED(status)) {
                            // Añadimos a la lista 'S' (Stopped)
                            add_job(child_pid, buffer , 'S');

                        }

                        // Ctrl + C
                        else if (WIFSIGNALED(status)) {
                            printf("\n");
                        }
                    }
                }
            }
        }
        printf("msh> ");
    }

    return 0;
}

void umask_execute(tline *line) {
    //Control de pipes (no puede aceptarlas)
    if (line->ncommands > 1) {
        fprintf(stderr, "umask: no se puede ejecutar con pipes\n");
    }
    // Sin argumento: Mostrar máscara
    else if (line->commands[0].argc == 1) {
        mode_t old_mask = umask(0); // Cambiamos a 0 y recogemos la anterior (la que queremos mostrar)
        umask(old_mask); // La restauramos inmediatamente
        printf("%04o\n", old_mask); // %04o imprime en formato Octal (ej: 0022)
    }
    // Con argumento: Cambiar máscara
    else if (line->commands[0].argc == 2) {
        mode_t new_mask = (mode_t)strtoul(line->commands[0].argv[1], NULL, 8); // Convertimos el string a long usando octal
        umask(new_mask);
    }
    else {
        fprintf(stderr, "uso: umask [numero_octal]\n");
    }

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

// Añadir un job a la lista
void add_job(pid_t pid, char *command, char status) {
    if (n_jobs < MAX_JOBS) {
        jobs_list[n_jobs].pid = pid;
        jobs_list[n_jobs].command = strdup(command); // Copiamos el string
        jobs_list[n_jobs].status = status;
        n_jobs++;
        printf("\n");
        printf("[%d]+ %s\t%s\n", n_jobs, (status == 'R' ? "Running" : "Stopped"), command);
    } else {
        fprintf(stderr, "Error: Lista de trabajos llena\n");
    }
}

// Eliminar un job de la lista
void delete_job(int index) {
    if (index < 0 || index >= n_jobs) return;

    free(jobs_list[index].command); // Liberamos memoria del string

    for (int i = index; i < n_jobs - 1; i++) {
        jobs_list[i] = jobs_list[i + 1];
    }
    n_jobs--;
}

void jobs_execute() {
    for (int i = 0; i < n_jobs; i++) {
        printf("[%d]+ %s\t%s\n", i + 1,
               (jobs_list[i].status == 'R' ? "Running" : "Stopped"),
               jobs_list[i].command);
    }
}

void bg_execute(tline *line) {
    int job_num = -1;
    int index = -1;

    // bg sin argumentos: último trabajo parado
    if (line->commands[0].argc == 1) {
        for (int i = n_jobs - 1; i >= 0; i--) {
            if (jobs_list[i].status == 'S') {
                index = i;
                break;
            }
        }
        if (index == -1) {
            fprintf(stderr, "bg: no hay trabajos detenidos actualmente\n");
            return;
        }
    }
    // bg <numero>: trabajo específico
    else {
        job_num = atoi(line->commands[0].argv[1]);
        if (job_num < 1 || job_num > n_jobs) {
            fprintf(stderr, "bg: trabajo no encontrado\n");
            return;
        }
        index = job_num - 1;
    }

    if (jobs_list[index].status == 'S') {
        kill(jobs_list[index].pid, SIGCONT); // Señal CONTINUAR
        jobs_list[index].status = 'R';       // Actualizamos estado
        printf("[%d]+ %s &\n", index + 1, jobs_list[index].command);
    } else {
        fprintf(stderr, "bg: el trabajo ya está en ejecución\n");
    }
}





