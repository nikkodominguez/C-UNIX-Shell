#ifndef P3_PROCESS_LIST_H
#define P3_PROCESS_LIST_H

#include <sys/types.h>
#include <time.h>

#ifndef MAX
#define MAX 1024
#endif

typedef enum {
    ACTIVO,
    TERMINADO,
    DETENIDO,
    SENALADO
} tEstado;

typedef struct ProcessNode {
    pid_t pid;
    char command_line[MAX];  // Comando que se ejecutó
    time_t launch_time;      // Hora de inicio
    tEstado state;           // Estado actual
    int info;                // Valor de retorno (exit) o señal (signal)
    struct ProcessNode *next;
} ProcessNode;

typedef struct {
    ProcessNode *head;
} tProcessList;

void createProcessList(tProcessList *list);
int insertProcess(tProcessList *list, pid_t pid, const char *cmd_line);
int removeProcess(tProcessList *list, pid_t pid);
void removeProcessesByState(tProcessList *list, tEstado state_filter);
void clearProcessList(tProcessList *list);
void updateProcessStatus(tProcessList *list);
void printProcessList(tProcessList *list);
char *NombreSenal(int sen);

#endif // P3_PROCESS_LIST_H