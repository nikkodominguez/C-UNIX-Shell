#include <stdio.h>
#include <string.h>

#include "command_list.h"
#include "file_list.h"
#include "commands.h"
#include "process_list.h"

void imprimirPrompt();
void leerEntrada(char entrada[]);
int trocearCadena(char *cadena, char *trozos[]);
void procesarEntrada(char entrada[], tList *historial, tFileList *archivos,tMemList *mem_list, tProcessList *process_list, int *terminado, char *envp[]);

int main(int argc, char *argv[], char *envp[]){

    char entrada[MAX];
    int terminado = 0;

    tList historial;
    tFileList open_files;
    tMemList mem_list;
    tProcessList process_list;

    createList(&historial);
    createFileList(&open_files);
    createMemList(&mem_list);
    createProcessList(&process_list);

    while (!terminado) {
        imprimirPrompt();
        leerEntrada(entrada);

        if (strlen(entrada) > 0) {
            char *trozos_check[MAX/2];
            char entrada_check[MAX];
            strncpy(entrada_check, entrada, MAX);
            trocearCadena(entrada_check, trozos_check);
            if (trozos_check[0] != NULL &&
                !(strcmp(trozos_check[0], "historic") == 0 && trozos_check[1] != NULL && atoi(trozos_check[1]) > 0)) {
                insertItem(&historial, entrada);
            }
        }
        procesarEntrada(entrada, &historial, &open_files, &mem_list, &process_list, &terminado, envp);    }

    printf("Saliendo del shell.\n");
    clearList(&historial);
    clearFileList(&open_files);
    clearProcessList(&process_list);
    return 0;
}

void imprimirPrompt() {
    printf("$> ");
    fflush(stdout);
}

void leerEntrada(char entrada[]) {
    if (fgets(entrada, MAX, stdin) == NULL) {
        strcpy(entrada, "exit");
    }
    entrada[strcspn(entrada, "\n")] = 0;
}

int trocearCadena(char *cadena, char *trozos[]) { //numero de trozos del comando
    int i = 1;
    if ((trozos[0] = strtok(cadena, " \n\t")) == NULL)
        return 0;
    while ((trozos[i] = strtok(NULL, " \n\t")) != NULL)
        i++;
    return i;
}

void procesarEntrada(char entrada[], tList *historial, tFileList *archivos,tMemList *mem_list, tProcessList *process_list, int *terminado, char *envp[]) {
    char *trozos[MAX / 2];
    int num_trozos;
    char entrada_copia[MAX];
    strncpy(entrada_copia, entrada, MAX);
    char *comando_a_repetir;

    num_trozos = trocearCadena(entrada_copia, trozos);
    if (num_trozos == 0) return;

    if (strcmp(trozos[0], "exit") == 0 || strcmp(trozos[0], "quit") == 0 || strcmp(trozos[0], "bye") == 0)
        *terminado = 1;
    else if(strcmp(trozos[0], "authors") == 0)
        Cmd_authors(trozos);
    else if(strcmp(trozos[0], "getpid") == 0)
        Cmd_getpid(trozos);
    else if(strcmp(trozos[0], "chdir") == 0)
        Cmd_chdir(trozos);
    else if(strcmp(trozos[0], "getcwd") == 0)
        Cmd_getcwd(trozos);
    else if(strcmp(trozos[0], "date") == 0)
        Cmd_date(trozos);
    else if(strcmp(trozos[0], "hour") == 0)
        Cmd_hour(trozos);
    else if(strcmp(trozos[0], "historic") == 0) {
        comando_a_repetir = Cmd_historic(trozos, historial);
        if(comando_a_repetir != NULL){
            printf("Ejecutando: %s\n", comando_a_repetir);

            //se añade la repeticion del comando al historic
            insertItem(historial, comando_a_repetir);
            //llamada recursiva para que ejecute el comando correspondiente
            procesarEntrada(comando_a_repetir, historial, archivos, mem_list, process_list, terminado, envp);
            free(comando_a_repetir);
        }
    }
    else if (strcmp(trozos[0], "listopen") == 0)
        Cmd_listopen(archivos);
    else if (strcmp(trozos[0], "infosys") == 0)
        Cmd_infosys(trozos);
    else if(strcmp(trozos[0], "help") == 0)
        Cmd_help(num_trozos, trozos);
    else if (strcmp(trozos[0], "open") == 0)
        Cmd_open(trozos + 1, archivos);
    else if (strcmp(trozos[0], "close") == 0)
        Cmd_close(trozos + 1, archivos);
    else if (strcmp(trozos[0], "dup") == 0)
        Cmd_dup(trozos + 1, archivos);
    else if(strcmp(trozos[0], "create") == 0)
        Cmd_create(trozos);
    else if(strcmp(trozos[0], "lseek") == 0)
        Cmd_lseek(trozos);
    else if(strcmp(trozos[0], "writestr") == 0)
        Cmd_writestr(trozos);
    else if(strcmp(trozos[0], "erase") == 0)
        Cmd_erase(trozos, archivos);
    else if(strcmp(trozos[0], "delrec") == 0)
        Cmd_delrec(trozos);
    else if(strcmp(trozos[0], "setdirparams") == 0)
        Cmd_setdirparams(trozos);
    else if (strcmp(trozos[0], "getdirparams") == 0)
        Cmd_getdirparams(trozos);
    else if (strcmp(trozos[0], "dir") == 0)
        Cmd_dir(trozos);
    else if(strcmp(trozos[0], "malloc") == 0)
        Cmd_malloc(trozos, mem_list);
    else if (strcmp(trozos[0], "mmap") == 0)
        Cmd_mmap(trozos, mem_list);
    else if(strcmp(trozos[0], "shared") == 0)
        Cmd_shared(trozos, mem_list);
    else if (strcmp(trozos[0], "free") == 0)
        Cmd_free(trozos, mem_list);
    else if (strcmp(trozos[0], "memfill") == 0)
        Cmd_memfill(trozos, mem_list);
    else if (strcmp(trozos[0], "memdump") == 0)
        Cmd_memdump(trozos, mem_list);
    else if (strcmp(trozos[0], "mem") == 0)
        Cmd_mem(trozos, mem_list);
    else if(strcmp(trozos[0], "recurse") == 0)
        Cmd_recurse(trozos);
    else if(strcmp(trozos[0], "readfile") == 0)
        Cmd_readfile(trozos);
    else if(strcmp(trozos[0], "writefile") == 0)
        Cmd_writefile(trozos);
    else if(strcmp(trozos[0], "read") == 0)
        Cmd_read(trozos);
    else if(strcmp(trozos[0], "write") == 0)
        Cmd_write(trozos);
    else if (strcmp(trozos[0], "uid") == 0)
        Cmd_uid(trozos);
    else if (strcmp(trozos[0], "showenv") == 0)
        Cmd_showenv(trozos, envp);
    else if (strcmp(trozos[0], "envvar") == 0)
        Cmd_envvar(trozos, envp);
    else if (strcmp(trozos[0], "jobs") == 0)
        Cmd_jobs(trozos, process_list);
    else if (strcmp(trozos[0], "deljobs") == 0)
        Cmd_deljobs(trozos, process_list);
    else if (strcmp(trozos[0], "fork") == 0)
        Cmd_fork(process_list);
    else if (strcmp(trozos[0], "exec") == 0)
        Cmd_exec(trozos);
    else
        Cmd_ejecutar(trozos, process_list, envp);
}