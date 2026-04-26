#include "process_list.h"
#include <errno.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct SEN {
    char *nombre;
    int senal;
};

static struct SEN sigstrnum[] = {
        {"HUP", SIGHUP},
        {"INT", SIGINT},
        {"QUIT", SIGQUIT},
        {"ILL", SIGILL},
        {"TRAP", SIGTRAP},
        {"ABRT", SIGABRT},
        {"IOT", SIGIOT},
        {"BUS", SIGBUS},
        {"FPE", SIGFPE},
        {"KILL", SIGKILL},
        {"USR1", SIGUSR1},
        {"SEGV", SIGSEGV},
        {"USR2", SIGUSR2},
        {"PIPE", SIGPIPE},
        {"ALRM", SIGALRM},
        {"TERM", SIGTERM},
        {"CHLD", SIGCHLD},
        {"CONT", SIGCONT},
        {"STOP", SIGSTOP},
        {"TSTP", SIGTSTP},
        {"TTIN", SIGTTIN},
        {"TTOU", SIGTTOU},
        {"URG", SIGURG},
        {"XCPU", SIGXCPU},
        {"XFSZ", SIGXFSZ},
        {"VTALRM", SIGVTALRM},
        {"PROF", SIGPROF},
        {"WINCH", SIGWINCH},
        {"IO", SIGIO},
        {"SYS", SIGSYS},
    /*senales que no hay en todas partes*/
#ifdef SIGPOLL
        {"POLL", SIGPOLL},
#endif
#ifdef SIGPWR
        {"PWR", SIGPWR},
#endif
#ifdef SIGEMT
        {"EMT", SIGEMT},
#endif
#ifdef SIGINFO
        {"INFO", SIGINFO},
#endif
#ifdef SIGSTKFLT
        {"STKFLT", SIGSTKFLT},
#endif
#ifdef SIGCLD
        {"CLD", SIGCLD},
#endif
#ifdef SIGLOST
        {"LOST", SIGLOST},
#endif
#ifdef SIGCANCEL
        {"CANCEL", SIGCANCEL},
#endif
#ifdef SIGTHAW
        {"THAW", SIGTHAW},
#endif
#ifdef SIGFREEZE
        {"FREEZE", SIGFREEZE},
#endif
#ifdef SIGLWP
        {"LWP", SIGLWP},
#endif
#ifdef SIGWAITING
        {"WAITING", SIGWAITING},
#endif
        {NULL, -1},
};

char *NombreSenal(int sen)  /*devuelve el nombre senal a partir de la senal*/
{			/* para sitios donde no hay sig2str*/
    int i;
    for (i=0; sigstrnum[i].nombre!=NULL; i++)
        if (sen==sigstrnum[i].senal)
            return sigstrnum[i].nombre;
    return ("SIGUNKNOWN");
}

void createProcessList(tProcessList *list) {
    list->head = NULL;
}

int insertProcess(tProcessList *list, pid_t pid, const char *cmd_line) {
    ProcessNode *current, *newNode = (ProcessNode *)malloc(sizeof(ProcessNode));
    if (newNode == NULL) {
        perror("Error al asignar memoria para nodo de proceso");
        return -1;
    }

    newNode->pid = pid;
    strncpy(newNode->command_line, cmd_line, MAX - 1);
    newNode->command_line[MAX - 1] = '\0';
    newNode->state = ACTIVO;
    newNode->launch_time = time(NULL);
    newNode->info = 0;
    newNode->next = NULL;

    // Insertar al final para mantener orden cronológico
    if (list->head == NULL) {
        list->head = newNode;
    } else {
        current = list->head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }
    return 0;
}

int removeProcess(tProcessList *list, pid_t pid) {
    ProcessNode *current = list->head;
    ProcessNode *prev = NULL;

    while (current != NULL) {
        if (current->pid == pid) {
            if (prev == NULL) { // Es el primero
                list->head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return 1; // Eliminado correctamente
        }
        prev = current;
        current = current->next;
    }
    return 0; // No encontrado
}

void removeProcessesByState(tProcessList *list, tEstado state_filter) {
    ProcessNode *current = list->head;
    ProcessNode *prev = NULL;

    while (current != NULL) {
        if (current->state == state_filter) {
            ProcessNode *to_delete = current;

            if (prev == NULL) {
                list->head = current->next;
                current = list->head; // Avanzamos current al nuevo head
            } else {
                prev->next = current->next;
                current = prev->next; // Avanzamos current
            }
            free(to_delete);
            // No avanzamos prev porque acabamos de borrar el nodo actual
        } else {
            prev = current;
            current = current->next;
        }
    }
}

void clearProcessList(tProcessList *list) {
    ProcessNode *current = list->head;
    while (current != NULL) {
        ProcessNode *next = current->next;
        free(current);
        current = next;
    }
    list->head = NULL;
}

void updateProcessStatus(tProcessList *list) {
    ProcessNode *current = list->head;
    int status;
    pid_t res;

    while (current != NULL) {
        if (current->state != TERMINADO && current->state != SENALADO) { //evitamos mirar el estado de procesos muertos o señalados para morir

            res = waitpid(current->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

            if (res == current->pid) {
                if (WIFEXITED(status)) {
                    current->state = TERMINADO;
                    current->info = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    current->state = SENALADO;
                    current->info = WTERMSIG(status);
                } else if (WIFSTOPPED(status)) {
                    current->state = DETENIDO;
                    current->info = WSTOPSIG(status);
                } else if (WIFCONTINUED(status)) {
                    current->state = ACTIVO;
                    current->info = 0;
                }
            } else if (res == -1 && errno == ECHILD) { //si el sistema borro un proceso pero no se actualizo en la lista
                current->state = TERMINADO;
                current->info = 255;
            }
        }
        current = current->next;
    }
}

void printProcessList(tProcessList *list) {
    updateProcessStatus(list);

    ProcessNode *current = list->head;
    char time_str[64];
    int priority;
    struct tm *tm_info;

    if (current == NULL) {
        return;
    }

    while (current != NULL) {
        // Formatear hora
        tm_info = localtime(&current->launch_time);
        strftime(time_str, sizeof(time_str), "%Y/%m/%d %H:%M:%S", tm_info);

        errno = 0; //limpiamos errno para no confundir lo que devuelva getpriority (-1 por ejemplo) con un error
        priority = getpriority(PRIO_PROCESS, current->pid);
        if (errno != 0) priority = 0; // Valor dummy si falla, no tiene prioridad real porque ya murio, es simplemente estetico

        printf("%d\t%s\t%s\t", current->pid, "shell", time_str);

        switch (current->state) {
            case ACTIVO:
                printf("ACTIVE (%d)\t", current->info);
                break;
            case TERMINADO:
                printf("FINISHED (%d)\t", current->info);
                break;
            case SENALADO:
                printf("SIGNALED (%s)\t", NombreSenal(current->info));
                break;
            case DETENIDO:
                printf("STOPPED (%s)\t", NombreSenal(current->info));
                break;
        }

        printf("Pr=%d\t%s\n", priority, current->command_line);
        current = current->next;
    }
}