#ifndef P0_COMMANDS_H
#define P0_COMMANDS_H

#include "file_list.h"
#include "command_list.h"
#include "memory_list.h"
#include "process_list.h"

#include <sys/stat.h> //imprimir_info_path

//Funciones comandos
void Cmd_authors(char *trozos[]);
void Cmd_getpid(char *[]);
void Cmd_chdir(char *trozos[]);
void Cmd_getcwd(char *trozos[]);
void Cmd_date(char *trozos[]);
void Cmd_hour(char *trozos[]);
char* Cmd_historic(char *trozos[], tList *);
void Cmd_infosys(char *trozos[]);
void Cmd_help(int num_trozos,char *trozos[]);

//Comandos de ficheros
void Cmd_listopen(tFileList*);
void Cmd_open(char *trozos[], tFileList *);
void Cmd_close(char *trozos[], tFileList *);
void Cmd_dup(char *trozos[], tFileList *);
void Cmd_create(char *trozos[]);
void Cmd_lseek(char *trozos[]);
void Cmd_writestr(char *trozos[]);
void Cmd_erase(char *trozos[], tFileList *);
void Cmd_delrec(char *trozos[]);
void Cmd_setdirparams(char *trozos[]);
void Cmd_getdirparams(char *trozos[]);
void Cmd_dir(char *trozos[]);

//funciones memoria
void Cmd_malloc(char *trozos[], tMemList *list);
void Cmd_mmap(char *trozos[], tMemList *list);
void Cmd_shared(char *trozos[], tMemList *list);
void Cmd_free(char *trozos[], tMemList *list);
void Cmd_memfill(char *trozos[], tMemList *list);
void Cmd_memdump(char *trozos[], tMemList *list);
void Cmd_mem(char *trozos[], tMemList *list);
void Cmd_recurse(char *trozos[]);
void Cmd_readfile(char *trozos[]);
void Cmd_writefile(char *trozos[]);
void Cmd_read(char *trozos[]);
void Cmd_write(char *trozos[]);
void Cmd_uid(char *trozos[]);
void Cmd_envvars(char *trozos[]);

//Funciones procesos
void Cmd_uid(char *trozos[]);
void Cmd_showenv(char *trozos[], char *envp[]);
void Cmd_envvar(char *trozos[], char *envp[]);
void Cmd_jobs(char *trozos[], tProcessList *list);
void Cmd_deljobs(char *trozos[], tProcessList *list);
void Cmd_fork(tProcessList *list);
void Cmd_exec(char *trozos[]);
void Cmd_ejecutar(char *trozos[], tProcessList *list, char *envp[]);

//Funciones aux
int borrar_recursivo(const char *path);
char* mode_to_string(mode_t mode);
void imprimir_info_path(const char* path, const struct stat* file_info);
void listar_path(const char* path, int list_dir_content);


#endif //P0_COMMANDS_H
