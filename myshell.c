#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "parser.h"

#define MAX_JOBS 20

typedef struct {
    pid_t *pids;      // Array dinámico de PIDs
    int npids;
    char *command;
    char status;      // 'R' = Running, 'S' = Stopped
} Job;

Job jobs_list[MAX_JOBS];
int n_jobs = 0;


void umask_execute(tline *line);
void cd_execute(tline *line);
void jobs_execute();
void bg_execute(tline *line);
void add_job(pid_t *pids, int n, char *command, char status);
void delete_job(int index);
void free_all_jobs();

int main() {

    char buffer[1024];
    tline *line;
    int i, j;
    pid_t pid;
    pid_t pid_bg;
    int status_bg;
    int status;

    // Control de señales
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    // Variables para la gestión de pipes
    int p[2];
    int fd_in;

    printf("msh> ");
    while (fgets(buffer, 1024, stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;

        line = tokenize(buffer);

        // Zombies check
        for (int k = 0; k < n_jobs; k++) {
            pid_bg = waitpid(jobs_list[k].pids[jobs_list[k].npids - 1], &status_bg, WNOHANG);

            if (pid_bg > 0) {
                printf("[%d]+ Done\t%s\n", k + 1, jobs_list[k].command);
                delete_job(k);
                k--;
            }
        }

        if (line == NULL || line->ncommands == 0) {
            printf("msh> ");
            continue;
        }

        // Comprobacion de mandatos
        if (strcmp(line->commands[0].argv[0], "exit") == 0) {
            free_all_jobs();
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
            // Reservamos memoria para guardar los PIDs de esta ejecución
            pid_t *pids_temp = malloc(line->ncommands * sizeof(pid_t));

            for (i = 0; i < line->ncommands; i++) {

                if (i < line->ncommands - 1) {
                    pipe(p);
                }
                pid = fork();

                if (pid == 0) { // Hijo
                    if (line->background) {
                        signal(SIGINT, SIG_IGN);
                        signal(SIGTSTP, SIG_IGN);
                    } else {
                        signal(SIGINT, SIG_DFL);
                        signal(SIGTSTP, SIG_DFL);
                    }

                    if (i == 0) {
                        if (line->redirect_input != NULL) {
                            int file_in = open(line->redirect_input, O_RDONLY);
                            if (file_in == -1) {
                                perror("Error apertura entrada");
                                exit(1);
                            }
                            dup2(file_in, STDIN_FILENO);
                            close(file_in);
                        }
                    } else {
                        dup2(fd_in, STDIN_FILENO);
                        close(fd_in);
                    }

                    if (i == line->ncommands - 1) {
                        if (line->redirect_output != NULL) {
                            int file_out = open(line->redirect_output, O_CREAT | O_TRUNC | O_WRONLY, 0666);
                            if (file_out == -1) {
                                perror("Error apertura salida");
                                exit(1);
                            }
                            dup2(file_out, STDOUT_FILENO);
                            close(file_out);
                        }
                        if (line->redirect_error != NULL) {
                            int file_err = open(line->redirect_error, O_CREAT | O_TRUNC | O_WRONLY, 0666);
                            if (file_err == -1) {
                                perror("Error apertura error");
                                exit(1);
                            }
                            dup2(file_err, STDERR_FILENO);
                            close(file_err);
                        }
                    } else {
                        close(p[0]);
                        dup2(p[1], STDOUT_FILENO);
                        close(p[1]);
                    }

                    execvp(line->commands[i].filename, line->commands[i].argv);
                    fprintf(stderr, "%s: No se encuentra el mandato\n", line->commands[i].filename);
                    exit(1);
                }
                else { // Padre
                    pids_temp[i] = pid; // Guardamos el PID en el array temporal

                    if (fd_in != -1) {
                        close(fd_in);
                    }
                    if (i < line->ncommands - 1) {
                        fd_in = p[0];
                        close(p[1]);
                    }
                }
            }

            // Espera de procesos
            if (line->background) {
                add_job(pids_temp, line->ncommands, buffer, 'R');
            }
            else {
                int job_added = 0;
                for (j = 0; j < line->ncommands; j++) {
                    pid_t child_pid = waitpid(-1, &status, WUNTRACED);

                    if (child_pid > 0) {
                        // Ctrl + Z
                        if (WIFSTOPPED(status)) {
                            if (!job_added) {
                                add_job(pids_temp, line->ncommands, buffer , 'S');
                                job_added = 1;
                            }
                        }
                        // Ctrl + C
                        else if (WIFSIGNALED(status)) {
                            printf("\n");
                        }
                    }
                }
            }
            free(pids_temp); // Liberamos el array temporal
        }
        printf("msh> ");
    }

    free_all_jobs();
    return 0;
}



void umask_execute(tline *line) {
    if (line->ncommands > 1) {
        fprintf(stderr, "umask: no se puede ejecutar con pipes\n");
    }
    else if (line->commands[0].argc == 1) {
        mode_t old_mask = umask(0);
        umask(old_mask);
        printf("%04o\n", old_mask);
    }
    else if (line->commands[0].argc == 2) {
        mode_t new_mask = (mode_t)strtoul(line->commands[0].argv[1], NULL, 8);
        umask(new_mask);
    }
    else {
        fprintf(stderr, "uso: umask [numero_octal]\n");
    }
}

void cd_execute(tline *line) {
    char *dir;
    char buffer[1024];

    if (line->commands[0].argc == 1) {
        dir = getenv("HOME");
    } else {
        dir = line -> commands[0].argv[1];
    }

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


void add_job(pid_t *pids, int n, char *command, char status) {
    if (n_jobs < MAX_JOBS) {
        // Reservamos memoria
        jobs_list[n_jobs].pids = malloc(n * sizeof(pid_t));

        for(int k = 0; k < n; k++) {
            jobs_list[n_jobs].pids[k] = pids[k];
        }

        jobs_list[n_jobs].npids = n;
        jobs_list[n_jobs].command = strdup(command);
        jobs_list[n_jobs].status = status;
        n_jobs++;

        if (status == 'S') printf("\n");
        printf("[%d]+ %s\t%s\n", n_jobs, (status == 'R' ? "Running" : "Stopped"), command);
    } else {
        fprintf(stderr, "Error: Lista de trabajos llena\n");
    }
}

void delete_job(int index) {
    if (index < 0 || index >= n_jobs) return;

    free(jobs_list[index].command);
    free(jobs_list[index].pids); // Liberamos el array de PIDs

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
    else {
        job_num = atoi(line->commands[0].argv[1]);
        if (job_num < 1 || job_num > n_jobs) {
            fprintf(stderr, "bg: trabajo no encontrado\n");
            return;
        }
        index = job_num - 1;
    }

    if (jobs_list[index].status == 'S') {
        for (int k = 0; k < jobs_list[index].npids; k++) {
            kill(jobs_list[index].pids[k], SIGCONT);
        }

        jobs_list[index].status = 'R';
        printf("[%d]+ %s &\n", index + 1, jobs_list[index].command);
    } else {
        fprintf(stderr, "bg: el trabajo ya está en ejecución\n");
    }
}

void free_all_jobs() {
    for (int i = 0; i < n_jobs; i++) {
        if (jobs_list[i].command != NULL) {
            free(jobs_list[i].command);
            jobs_list[i].command = NULL;
        }
        // Liberamos el array de PIDs
        if (jobs_list[i].pids != NULL) {
            free(jobs_list[i].pids);
            jobs_list[i].pids = NULL;
        }
    }
    n_jobs = 0;
}