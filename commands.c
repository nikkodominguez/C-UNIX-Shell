#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>           //para date
#include <sys/utsname.h>    //para infosys
#include <sys/stat.h>       //mkdir
#include <dirent.h>         //para DIR en borrar_recursivo
#include <pwd.h>            // Para getpwuid (obtener nombre de usuario del UID)
#include <grp.h>            // Para getgrgid (obtener nombre de grupo del GID)
#include <sys/ipc.h>        //para shared
#include <sys/shm.h>
#include <sys/types.h>
#include <errno.h>          //para shared
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>   // para mmap, munmap, PROT_READ, PROT_WRITE, PROT_EXEC

#include "commands.h"

#include <sys/resource.h>

#define TAMANO 1024
#define MAXVAR 256  // para las funciones de ayuda de variables de entorno

int global_v1, global_v2, global_v3;
int global_init_v1 = 10, global_init_v2 = 20, global_init_v3 = 30;
extern char **environ;

typedef enum {
    RECURSE_NONE = 0,   // norec
    RECURSE_BEFORE,     // recb  (pre-order)
    RECURSE_AFTER       // reca  (post-order)
} RecurseMode;

static struct {
    int long_format;      // 1 = long, 0 = short
    int show_link_dest;   // 1 = link (mostrar destino), 0 = nolink
    int omit_hidden;      // 1 = hid (incluir), 0 = nohid (omitir ocultos)
    RecurseMode recurse;  // recursividad: NONE/BEFORE/AFTER
} g_dirparams = {
        0,0,0, RECURSE_NONE //valores por defecto
};

//--FUNCIONES AUXILIARES--

//delrec
int borrar_recursivo(const char *path){
    struct stat file_info;
    DIR *dir;
    struct dirent *entry;
    char full_path[PATH_MAX];

    //Se mete la información del archivo del path que le pasamos en file_info
    if(lstat(path, &file_info) == -1){
        perror(path);
        return -1;
    }

    //st_mode nos da el tipo de fichero que es
    if(!S_ISDIR(file_info.st_mode)){
        if(unlink(path) == -1){
            perror(path);
            return -1;
        }
        return 0;
    }

    dir = opendir(path);

    if(dir == NULL){
        perror(path);
        return -1;
    }

    //readdir(dir) lee la siguiente entrada del directorio, comprobando si aun no esta vacio
    while((entry = readdir(dir)) != NULL){
        //Se ignoran las entradas directorio actual y padre para evitar bucle infinito
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        //Construimos la ruta completa para la entrada actual
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        //LLamada recursiva
        if(borrar_recursivo(full_path) == -1){
            closedir(dir);
            return -1;
        }
    }
    closedir(dir);

    //Una vez directorio vacio, se borra con rmdir
    if(rmdir(path) == -1){
        perror(path);
        return -1;
    }
    return 0;
}

//dir
char* mode_to_string(mode_t mode) {
    static char str[11];
    strcpy(str, "----------");

    //Tipo de fichero
    if (S_ISDIR(mode)) str[0] = 'd';
    else if (S_ISLNK(mode)) str[0] = 'l';

    //Permisos del propietario (user)
    if(mode & S_IRUSR) str[1] = 'r';
    if(mode & S_IWUSR) str[2] = 'w';
    if(mode & S_IXUSR) str[3] = 'x';

    //Permisos del grupo
    if(mode & S_IRGRP) str[4] = 'r';
    if(mode & S_IWGRP) str[5] = 'w';
    if(mode & S_IXGRP) str[6] = 'x';

    //Permisos de otros
    if(mode & S_IROTH) str[7] = 'r';
    if(mode & S_IWOTH) str[8] = 'w';
    if(mode & S_IXOTH) str[9] = 'x';

    return str;
}

void imprimir_info_path(const char* path, const struct stat* file_info) {
    char time_str[20];
    char* perms;
    struct passwd* pwd;
    struct group* grp;
    char link_target[PATH_MAX];

    if(!g_dirparams.long_format) { //Formato corto
        printf("%s\t%ld\n", path, file_info->st_size);
    }else{ //Formato largo
        strftime(time_str, sizeof(time_str), "%b %d %H:%M", localtime(&file_info->st_mtime));
        perms = mode_to_string(file_info->st_mode);
        pwd = getpwuid(file_info->st_uid);
        grp = getgrgid(file_info->st_gid);

        printf("%s %ld %s %s %8ld %s %s",
               perms, file_info->st_nlink,
               (pwd ? pwd->pw_name : "unknown"),
               (grp ? grp->gr_name : "unknown"),
               file_info->st_size, time_str, path);

        if(S_ISLNK(file_info->st_mode) && g_dirparams.show_link_dest) {
            ssize_t len = readlink(path, link_target, sizeof(link_target) - 1);
            if(len != -1) {
                link_target[len] = '\0';
                printf(" -> %s", link_target);
            }
        }
        printf("\n");
    }
}

void listar_path(const char* path, int list_dir_content) {
    struct stat file_info;
    const char *base_name;
    DIR* dir;
    struct dirent* entry;
    char full_path[PATH_MAX];

    if(lstat(path, &file_info) == -1) {
        perror(path);
        return;
    }

    base_name = strrchr(path, '/');
    base_name = (base_name == NULL) ? path : base_name + 1;

    if (g_dirparams.omit_hidden && base_name[0] == '.' && strcmp(base_name, ".") != 0 && strcmp(base_name, "..") != 0) {
        return;
    }


    // Si NO es un directorio O SÍ lo es pero NO se pide listar su contenido
    if(!S_ISDIR(file_info.st_mode) || !list_dir_content) {
        imprimir_info_path(path, &file_info);
        return;
    }

    // RECB: Imprimir directorio ANTES de su contenido.
    if(g_dirparams.recurse == RECURSE_BEFORE) {
        imprimir_info_path(path, &file_info);
    }

    dir = opendir(path);
    if(dir == NULL) {
        perror(path);
        return;
    }

    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if(g_dirparams.recurse != RECURSE_NONE) {
            listar_path(full_path, 1); // Llamada recursiva, SIEMPRE listando contenido
        }else{
            listar_path(full_path, 0); // No recursivo, solo info del elemento
        }
    }
    closedir(dir);

    // RECA: Imprimir directorio DESPUÉS de su contenido.
    if(g_dirparams.recurse == RECURSE_AFTER) {
        imprimir_info_path(path, &file_info);
    }
}

// Función auxiliar para Cmd_mmap
void *MapearFichero(const char *fichero, int protection, tMemList *list) {
    int df, map = MAP_PRIVATE, modo = O_RDONLY;
    struct stat s;
    void *p;

    // Si queremos escribir, hay que abrir el fichero en O_RDWR
    if (protection & PROT_WRITE)
        modo = O_RDWR;

    // Obtenemos tamaño del fichero y lo abrimos
    if (stat(fichero, &s) == -1 || (df = open(fichero, modo)) == -1)
        return NULL;

    // Hacemos el mmap
    p = mmap(NULL, s.st_size, protection, map, df, 0);
    if (p == MAP_FAILED) {
        int aux = errno;
        close(df);
        errno = aux;
        return NULL;
    }

    // Guardamos el bloque en nuestra lista de memoria
    if (InsertarNodoMmap(list, p, s.st_size, fichero, df, protection) == -1) {
        // Si falla insertar en la lista, desmapeamos y cerramos
        munmap(p, s.st_size);
        close(df);
        return NULL;
    }

    return p;
}

//shared
void * ObtenerMemoriaShmget (key_t clave, size_t tam, tMemList *list) {
    void * p;
    int aux, id, flags = 0777;
    struct shmid_ds s;

    
    if (tam)
        flags = flags | IPC_CREAT | IPC_EXCL;

    
    if (clave == IPC_PRIVATE) {
        errno = EINVAL;
        return NULL;
    }

    //Obtener el ID de la memoria compartida (shmget) 
    if ((id = shmget(clave, tam, flags)) == -1)
        return (NULL);

    //Adjuntar la memoria a nuestro espacio de direcciones (shmat)
    if ((p = shmat(id, NULL, 0)) == (void*) -1) {
        aux = errno;
        if (tam)
            shmctl(id, IPC_RMID, NULL);
        errno = aux;
        return (NULL);
    }

    //Obtener información del bloque (para saber el tamaño real si no lo creamos nosotros)
    shmctl(id, IPC_STAT, &s);

    //Guardar en TU lista
    InsertarNodoShared(list, p, s.shm_segsz, clave);

    return (p);
}

void do_SharedDelkey (char *args[]) {
    key_t clave;
    int id;
    char *key = args[0];

    if (key == NULL || (clave = (key_t) strtoul(key, NULL, 10)) == IPC_PRIVATE){
        printf ("Error: delkey necesita clave_valida\n");
        return;
    }

    if ((id = shmget(clave, 0, 0666)) == -1){ //preguntamos que id del SO corresponde con esa clave
        perror ("shmget: imposible obtener memoria compartida");
        return;
    }

    if (shmctl(id, IPC_RMID, NULL) == -1) //con el id se borra la asociacion con cl y se marca con flag interno destruccion pendiente
        perror ("shmctl: imposible eliminar memoria compartida\n");
}

//mem
void Do_pmap (void) /*sin argumentos*/
{ pid_t pid;
    char elpid[32];
    char *argv[4]={"pmap",elpid,NULL};

    sprintf (elpid,"%d", (int) getpid());
    if ((pid=fork())==-1){
        perror ("Imposible crear proceso");
        return;
    }
    if (pid==0){
        if (execvp(argv[0],argv)==-1)
            perror("cannot execute pmap (linux, solaris)");

        argv[0]="procstat"; argv[1]="vm"; argv[2]=elpid; argv[3]=NULL;
        if (execvp(argv[0],argv)==-1)
            perror("cannot execute procstat (FreeBSD)");

        argv[0]="procmap",argv[1]=elpid;argv[2]=NULL;
        if (execvp(argv[0],argv)==-1)
            perror("cannot execute procmap (OpenBSD)");

        argv[0]="vmmap"; argv[1]="-interleave"; argv[2]=elpid;argv[3]=NULL;
        if (execvp(argv[0],argv)==-1) 
            perror("cannot execute vmmap (Mac-OS)");
        exit(1);
    }
    waitpid (pid,NULL,0);
}

//recurse
void Recursiva (int n){
    char automatico[TAMANO];
    static char estatico[TAMANO];

    printf ("parametro:%3d(%p) array %p, arr estatico %p\n",n,&n,automatico, estatico);

    if (n>0)
        Recursiva(n-1);
}

//readFile
ssize_t LeerFichero(char *f, void *p, size_t cont) {
    struct stat s;
    ssize_t n;
    int df, aux;

    if (stat(f, &s) == -1 || (df = open(f, O_RDONLY)) == -1) {
        return -1;
    }

    if (cont == -1) {
        //si pasamos -1 como bytes a leer lo leemos entero
        cont = s.st_size;
    }

    if ((n = read(df, p, cont)) == -1) {
        aux = errno;
        close(df);
        errno = aux;
        return -1;
    }

    close(df);
    return n;
}

//WriteFile
ssize_t EscribirFichero(char *f, void *p, size_t cont, int overwrite) {
    ssize_t n;
    int df, aux;
    int flags = O_CREAT | O_WRONLY;

    if (overwrite)
        flags |= O_TRUNC; // Sobrescribir si existe
    else
        flags |= O_EXCL;  // Fallar si ya existe

    if ((df = open(f, flags, 0666)) == -1) {
        return -1;
    }

    if ((n = write(df, p, cont)) == -1) {
        aux = errno;
        close(df);
        errno = aux;
        return -1;
    }

    close(df);
    return n;
}

int BuscarVariable (char * var, char *e[])  /*busca una variable en el entorno que se le pasa como parámetro*/
{                                           /*devuelve la posicion de la variable en el entorno, -1 si no existe*/
  int pos=0;
  char aux[MAXVAR];

  strcpy (aux,var);
  strcat (aux,"=");

  while (e[pos]!=NULL)
    if (!strncmp(e[pos],aux,strlen(aux)))
      return (pos);
    else
      pos++;
  errno=ENOENT;   /*no hay tal variable*/
  return(-1);
}

int CambiarVariable(char * var, char * valor, char *e[]) /*cambia una variable en el entorno que se le pasa como parámetro*/
{                                                        /*lo hace directamente, no usa putenv*/
  int pos;
  char *aux;

  if ((pos=BuscarVariable(var,e))==-1)
    return(-1);

  if ((aux=(char *)malloc(strlen(var)+strlen(valor)+2))==NULL) //+2 para = y \0
	return -1;
  strcpy(aux,var);
  strcat(aux,"=");
  strcat(aux,valor);
  e[pos]=aux;
  return (pos);
}

//--FUNCIONES COMANDOS--
void Cmd_authors(char *trozos[]) {
    if (trozos[1] == NULL) {
        //No hay argumentos
        printf("Nicolas Dominguez Souto (nicolas.dominguez@udc.es)\n");
    } else if (strcmp(trozos[1], "-l") == 0) {
        printf("nicolas.dominguez@udc.es\n");
    } else if (strcmp(trozos[1], "-n") == 0) {
        printf("Nicolás Dominguez Souto\n");
    } else {
        //Argumento no reconocido.
        fprintf(stderr, "Error: Argumento no válido para authors: %s\n", trozos[1]);
        fprintf(stderr, "Uso: authors [-l|-n]\n"); //stderr mensajes de error
    }
}

void Cmd_getpid(char *trozos[]) {
    if (trozos[1] == NULL) {
        printf("PID del shell: %d\n", getpid());
    } else if (strcmp(trozos[1], "-p") == 0) {
        //-p: imprime el PID del padre del shell
        printf("PID del padre del shell: %d\n", getppid());
    } else {
        //Argumento no reconocido
        fprintf(stderr, "Error: Argumento no válido para getpid: %s\n", trozos[1]);
        fprintf(stderr, "Uso: getpid [-p]\n");
    }
}

void Cmd_chdir(char *trozos[]) {
    char cwd[1024]; //Almacenar el directorio actual

    //Sin argumentos devuelve directorio actual
    if (trozos[1] == NULL) {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("Directorio actual: %s\n", cwd);
        } else {
            perror("Error al obtener el directorio actual");
        }
    }
    //Demasiados argumentos
    else if (trozos[2] != NULL) {
        fprintf(stderr, "Error: Demasiados argumentos para chdir\n");
        fprintf(stderr, "Uso: chdir [directorio]\n");
    }
    else {
        if (chdir(trozos[1]) != 0) { //Si chdir tiene exito se cambia el directorio (devuelve 0)
            perror("Error al cambiar de directorio");
        }
    }
}

void Cmd_getcwd(char *trozos[]) {
    char cwd[1024];
    if(trozos[1] == NULL){
        if(getcwd(cwd, sizeof(cwd)) != NULL){
            printf("El directorio actual es: %s \n", cwd);
        }
        else{
            perror("Error al obtener el cwd");
        }
    }
    else{
        fprintf(stderr, "Error: el comando 'cwd' no admite argumentos\n");
        fprintf(stderr, "Uso: getcwd\n");
    }
}

void Cmd_date(char *trozos[]) {
    time_t t;
    struct tm *tm_info;
    char buffer[100];

    time(&t);
    tm_info = localtime(&t);

    if (trozos[1] == NULL) {
        //Sin argumentos, mostrar fecha y hora
        strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", tm_info);
        printf("%s\n", buffer);
    } else if (strcmp(trozos[1], "-d") == 0) {
        //Argumento -d, solo fecha
        strftime(buffer, sizeof(buffer), "%d/%m/%Y", tm_info);
        printf("%s\n", buffer);
    } else if (strcmp(trozos[1], "-t") == 0) {
        //Argumento -t, solo hora
        strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);
        printf("%s\n", buffer);
    } else {
        fprintf(stderr, "Error: Argumento no válido para date: %s\n", trozos[1]);
        fprintf(stderr, "Uso: date [-d|-t]\n");
    }
}

void Cmd_hour(char *trozos[]) {
    time_t t;
    struct tm *tm_info;
    char buffer[100];

    //Comprobamos que no se pasen argumentos
    if (trozos[1] != NULL) {
        fprintf(stderr, "Error: El comando 'hour' no admite argumentos.\n");
        fprintf(stderr, "Uso: hour\n");
        return;
    }

    time(&t);
    tm_info = localtime(&t);

    //Mostrar hora
    strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);
    printf("%s\n", buffer);
}

char* Cmd_historic(char *trozos[], tList *historial){
    int n;
    char* command_to_run;

    //Mostramos toda la lista del historico
    if(trozos[1] == NULL){
        PrintHistory(historial);
        return NULL;
    }

    //Borramos la lista de historicos
    if(strcmp(trozos[1], "-clear") == 0){
        clearList(historial);
        printf("Historial de comandos borrado.\n");
        return NULL;
    }

    //Mostramos numero de comandos ejecutados hasta ahora
    if(strcmp(trozos[1], "-count") == 0){
        printf("Número de comandos en el historial: %d\n", historial->cont);
        return NULL;
    }

    //Comprobar si el argumento es un numero
    n = atoi(trozos[1]);
    if(n == 0 && strcmp(trozos[1], "0") != 0){//atoi devuelve 0 para texto no numerico
        fprintf(stderr, "Error: Argumento no válido para historic: %s\n", trozos[1]);
        fprintf(stderr, "Uso: historic [N|-N|-clear|-count]\n");
        return NULL;
    }

    if(n < 0){
        PrintLastNCommands(historial, -n);
    }else if (n > 0){
        command_to_run = FindCommandByNumber(historial, n);
        if(command_to_run == NULL){
            fprintf(stderr, "Error: No existe el comando número %d en el historial.\n", n);
            return NULL;
        }
        //Devolvemos una copia para no manipular la memoria original de la lista
        return strdup(command_to_run);
    }
    return NULL;
}

void Cmd_listopen(tFileList *file_list) {
    ListarFicherosAbiertos(file_list);
}

void Cmd_infosys(char *trozos[]) {
    struct utsname buffer;

    if(trozos[1] == NULL){
        if (uname(&buffer) != 0) {
            perror("uname");
            return;
        }
        printf("Nombre del sistema: %s\n", buffer.sysname);
        printf("Nombre del nodo: %s\n", buffer.nodename);
        printf("Release: %s\n", buffer.release);
        printf("Versión: %s\n", buffer.version);
        printf("Máquina: %s\n", buffer.machine);
    }
    else{
        fprintf(stderr,"Error: Demasiados argumentos\n");
    }
}

void Cmd_help(int num_trozos, char *trozos[]) {
    switch (num_trozos) {
        case 1: { // Ayuda General
            printf("Comandos disponibles:\n");
            printf("  authors [-l|-n]: Muestra los nombres y/o logins de los autores.\n");
            printf("  bye: Finaliza la shell (alias de quit).\n");
            printf("  chdir [dir]: Cambia el directorio actual o lo muestra si no se especifica.\n");
            printf("  close [df]: Cierra un descriptor de fichero (df).\n");
            printf("  create [-f] name: Crea un fichero (con -f) o un directorio.\n");
            printf("  date [-t|-d]: Muestra la fecha y/o la hora actual.\n");
            printf("  delrec name1 [name2 ...]: Borra recursivamente ficheros y directorios.\n");
            printf("  dir [-d] [path ...]: Lista información de ficheros/directorios.\n");
            printf("  dup [df]: Duplica un descriptor de fichero (df).\n");
            printf("  erase name1 [name2 ...]: Borra ficheros o directorios vacíos.\n");
            printf("  exit: Finaliza la shell (alias de quit).\n");
            printf("  free addr: Libera un bloque de memoria en la dirección addr.\n");
            printf("  getcwd: Muestra el directorio de trabajo actual.\n");
            printf("  getdirparams: Muestra los parámetros actuales para el comando 'dir'.\n");
            printf("  getpid [-p]: Muestra el PID del shell o de su proceso padre.\n");
            printf("  help [cmd]: Muestra ayuda sobre los comandos.\n");
            printf("  historic [N|-N|-clear|-count]: Gestiona o muestra el historial de comandos.\n");
            printf("  hour: Muestra la hora actual.\n");
            printf("  infosys: Muestra información del sistema operativo.\n");
            printf("  listopen: Lista los ficheros abiertos por el shell.\n");
            printf("  lseek df offset ref: Cambia la posición en un fichero abierto.\n");
            printf("  malloc [-free] [n]: Asigna o libera memoria malloc.\n");
            printf("  mem [-blocks|-funcs|-vars|-all|-pmap]: Muestra detalles de la memoria del proceso.\n");
            printf("  memdump addr cont: Vuelca contenidos de memoria a la pantalla.\n");
            printf("  memfill addr cont ch: Llena la memoria con un carácter.\n");
            printf("  mmap [-free] [fich] [perm]: Mapea ficheros en memoria.\n");
            printf("  open [file] [mode]: Abre un fichero y lo añade a la lista.\n");
            printf("  quit: Finaliza la ejecución del shell.\n");
            printf("  read df addr cont: Lee de un descriptor a memoria.\n");
            printf("  readfile file addr cont: Lee de un fichero a memoria.\n");
            printf("  recurse n: Ejecuta una función recursiva n veces.\n");
            printf("  setdirparams opt: Establece un parámetro para 'dir' (long, short, etc.).\n");
            printf("  shared [-create|-free|-delkey] cl [n]: Gestiona memoria compartida.\n");
            printf("  write df addr cont: Escribe de memoria a un descriptor.\n");
            printf("  writefile file addr cont: Escribe de memoria a un fichero.\n");
            printf("  writestr df text: Escribe texto en un fichero abierto.\n");
            break;
        }
        case 2: { // Ayuda Específica
            if (strcmp(trozos[1], "authors") == 0) {
                printf("  authors [-l|-n]: Imprime los nombres y/o logins de los autores del programa.\n");
            } else if (strcmp(trozos[1], "bye") == 0) {
                printf("  bye: Finaliza la shell.\n");
            } else if (strcmp(trozos[1], "chdir") == 0) {
                printf("  chdir [dir]: Cambia el directorio de trabajo actual de la shell a dir. Si se invoca sin argumentos, imprime el directorio actual.\n");
            } else if (strcmp(trozos[1], "close") == 0) {
                printf("  close [df]: Cierra un descriptor de archivo y elimina el elemento correspondiente de la lista.\n");
            } else if (strcmp(trozos[1], "create") == 0) {
                printf("  create [-f] name: Crea un fichero (con -f) o un directorio (sin -f).\n");
            } else if (strcmp(trozos[1], "date") == 0) {
                printf("  date [-t|-d]: Imprime la fecha y/o la hora.\n");
            } else if (strcmp(trozos[1], "delrec") == 0) {
                printf("  delrec name1 [name2 ...]: Borra recursivamente ficheros y directorios (incluso si no están vacíos).\n");
            } else if (strcmp(trozos[1], "dir") == 0) {
                printf("  dir [-d] [path ...]: Lista información de ficheros/directorios. '-d' lista el contenido de los directorios.\n");
            } else if (strcmp(trozos[1], "dup") == 0) {
                printf("  dup [df]: Duplica el descriptor de archivo df, creando la entrada correspondiente en la lista de archivos.\n");
            } else if (strcmp(trozos[1], "erase") == 0) {
                printf("  erase name1 [name2 ...]: Borra ficheros o directorios vacíos.\n");
            } else if (strcmp(trozos[1], "exit") == 0) {
                printf("  exit: Finaliza la shell.\n");
            } else if (strcmp(trozos[1], "free") == 0) {
                printf("  free addr: Libera el bloque de memoria en la dirección addr.\n");
            } else if (strcmp(trozos[1], "getcwd") == 0) {
                printf("  getcwd: Imprime el directorio de trabajo actual.\n");
            } else if (strcmp(trozos[1], "getdirparams") == 0) {
                printf("  getdirparams: Muestra los parámetros actuales para el listado con 'dir'.\n");
            } else if (strcmp(trozos[1], "getpid") == 0) {
                printf("  getpid [-p]: Imprime el pid del proceso que se esta ejecutando en la shell o el de su padre (-p).\n");
            } else if (strcmp(trozos[1], "help") == 0) {
                printf("  help [cmd]: Muestra ayuda sobre los comandos. Si se especifica uno en concreto aporta informacion solo de ese comando\n");
            } else if (strcmp(trozos[1], "historic") == 0) {
                printf("  historic [N|-N|-clear|-count]: Muestra, repite (N), o gestiona el historial de comandos.\n");
            } else if (strcmp(trozos[1], "hour") == 0) {
                printf("  hour: Imprime la hora en formato hh:mm:ss\n");
            } else if (strcmp(trozos[1], "infosys") == 0) {
                printf("  infosys: Imprime informacion del sistema\n");
            } else if (strcmp(trozos[1], "listopen") == 0) {
                printf("  listopen: Enumera los archivos abiertos del shell\n");
            } else if (strcmp(trozos[1], "lseek") == 0) {
                printf("  lseek df offset ref: Cambia la posición del puntero en el fichero con descriptor 'df'.\n\t'ref' puede ser SEEK_SET, SEEK_CUR, SEEK_END.\n");
            } else if (strcmp(trozos[1], "malloc") == 0) {
                printf("  malloc [n]: Asigna n bytes de memoria. 'malloc -free n': Libera un bloque de tamaño n.\n");
            } else if (strcmp(trozos[1], "mem") == 0) {
                printf("  mem [-blocks|-funcs|-vars|-all|-pmap]: Muestra información sobre la memoria del proceso.\n");
            } else if (strcmp(trozos[1], "memdump") == 0) {
                printf("  memdump addr cont: Vuelca a pantalla cont bytes desde la dirección addr.\n");
            } else if (strcmp(trozos[1], "memfill") == 0) {
                printf("  memfill addr cont ch: Llena cont bytes desde addr con el carácter ch.\n");
            } else if (strcmp(trozos[1], "mmap") == 0) {
                printf("  mmap fich perm: Mapea el fichero. 'mmap -free fich': Desmapea el fichero.\n");
            } else if (strcmp(trozos[1], "open") == 0) {
                printf("  open [file] [mode]: Abre un archivo y lo añade a la lista de archivos abiertos de la shell.\n");
            } else if (strcmp(trozos[1], "quit") == 0) {
                printf("  quit: Finaliza la shell.\n");
            } else if (strcmp(trozos[1], "read") == 0) {
                printf("  read df addr cont: Lee cont bytes del descriptor df a la dirección addr.\n");
            } else if (strcmp(trozos[1], "readfile") == 0) {
                printf("  readfile file addr cont: Lee cont bytes del fichero file a la dirección addr.\n");
            } else if (strcmp(trozos[1], "recurse") == 0) {
                printf("  recurse n: Invoca a la función recursiva n veces.\n");
            } else if (strcmp(trozos[1], "setdirparams") == 0) {
                printf("  setdirparams opt: Establece un parámetro para 'dir'. Opciones: long, short, link, nolink, hid, nohid, reca, recb, norec.\n");
            } else if (strcmp(trozos[1], "shared") == 0) {
                printf("  shared [-create|-free|-delkey] cl [n]: Gestiona memoria compartida con clave cl.\n");
            } else if (strcmp(trozos[1], "write") == 0) {
                printf("  write df addr cont: Escribe cont bytes desde la dirección addr al descriptor df.\n");
            } else if (strcmp(trozos[1], "writefile") == 0) {
                printf("  writefile file addr cont: Escribe cont bytes desde la dirección addr al fichero file.\n");
            } else if (strcmp(trozos[1], "writestr") == 0) {
                printf("  writestr df text: Escribe la cadena 'text' en el fichero con descriptor 'df'.\n");
            } else {
                fprintf(stderr, "Error: Comando '%s' desconocido. No se puede mostrar la ayuda.\n", trozos[1]);
                fprintf(stderr, "Uso: help [comando_valido]\n");
            }
            break;
        }
        default: {
            fprintf(stderr, "Error: Número de argumentos incorrecto para 'help'.\n");
            fprintf(stderr, "Uso: help [comando]\n");
            break;
        }
    }
}

void Cmd_open(char *tr[], tFileList *file_list) {
    int i, df, mode = 0;

    //Sin argumentos, listar ficheros abiertos
    if(tr[0] == NULL){
        ListarFicherosAbiertos(file_list);
        return;
    }

    //Componer el modo de apertura a partir de los argumentos
    for (i = 1; tr[i] != NULL; i++) {
        if      (!strcmp(tr[i], "cr")) mode |= O_CREAT;
        else if (!strcmp(tr[i], "ex")) mode |= O_EXCL;
        else if (!strcmp(tr[i], "ro")) mode |= O_RDONLY;
        else if (!strcmp(tr[i], "wo")) mode |= O_WRONLY;
        else if (!strcmp(tr[i], "rw")) mode |= O_RDWR;
        else if (!strcmp(tr[i], "ap")) mode |= O_APPEND;
        else if (!strcmp(tr[i], "tr")) mode |= O_TRUNC;
        else break;
    }
    //Por defecto, si no se especifica modo de lectura/escritura, se usa lectura-escritura
    if((mode & (O_RDONLY | O_WRONLY | O_RDWR)) == 0){
        mode |= O_RDWR;
    }

    // Llamada al sistema para abrir el fichero
    if((df = open(tr[0], mode, 0777)) == -1){
        perror("Imposible abrir fichero");
    }else{
        AnadirAFicherosAbiertos(file_list, df, tr[0], mode);
        printf("Añadido descriptor %d a la tabla de ficheros abiertos.\n", df);
    }
}

void Cmd_close(char *tr[], tFileList *file_list) {
    int df;

    if(tr[0] == NULL){
        fprintf(stderr, "Error: se necesita un descriptor de fichero.\n");
        ListarFicherosAbiertos(file_list);
        return;
    }

    if(tr[1] != NULL){
        fprintf(stderr, "Error: Demasiados argumentos.\n");
        fprintf(stderr, "Uso: close <descriptor>\n");
        return;
    }

    df = atoi(tr[0]);
    if(df == 0 && strcmp(tr[0], "0") != 0){
        fprintf(stderr, "Error: El descriptor de fichero '%s' no es un número válido.\n", tr[0]);
        return;
    }

    if(df < 0){
        fprintf(stderr, "Error: El descriptor de fichero no puede ser negativo.\n");
        return;
    }

    if(close(df) == -1){
        perror("Imposible cerrar descriptor");
    }else{
        if(EliminarDeFicherosAbiertos(file_list, df)){
            printf("Descriptor %d eliminado de la lista.\n", df);
        }else{
            printf("Descriptor %d no estaba en la lista del shell.\n", df);
        }
    }
}

void Cmd_dup(char *tr[], tFileList *file_list) {
    int df, duplicado;
    char aux_name[MAX_FILES + 20];
    const char *original_name;

    if(tr[0] == NULL || (df = atoi(tr[0])) < 0){
        printf("Error: se necesita un descriptor de fichero válido.\n");
        ListarFicherosAbiertos(file_list);
        return;
    }

    if(tr[1] != NULL){
        fprintf(stderr, "Error: Demasiados argumentos.\n");
        fprintf(stderr, "Uso: dup <descriptor>\n");
        return;
    }

    if((duplicado = dup(df)) == -1){
        perror("Imposible duplicar descriptor");
        return;
    }

    original_name = NombreFicheroDescriptor(file_list, df);
    if(original_name == NULL){
        original_name = "desconocido";
    }

    snprintf(aux_name, sizeof(aux_name), "dup %d (%s)", df, original_name);

    AnadirAFicherosAbiertos(file_list, duplicado, aux_name, fcntl(duplicado, F_GETFL));
    printf("Descriptor %d duplicado en el descriptor %d.\n", df, duplicado);
}

void Cmd_create(char *trozos[]) {
    int fd;

    if(trozos[1] != NULL && strcmp(trozos[1], "-f") == 0){
        if(trozos[2] == NULL){
            fprintf(stderr, "Error: Se esperaba un nombre de fichero después de '-f'\n");
            fprintf(stderr, "Uso: create -f <nombre_fichero>\n");
            return;
        }
        if(trozos[3] != NULL){
            fprintf(stderr, "Error: Demasiados argumentos para 'create -f'\n");
            fprintf(stderr, "Uso: create -f <nombre_fichero>\n");
            return;
        }

        fd = open(trozos[2], O_CREAT | O_WRONLY | O_EXCL, 0644);

        if (fd == -1)
            perror("Error al crear el fichero");
        else
            close(fd);
    }else if(trozos[1] != NULL){
        if(trozos[2] != NULL){
            fprintf(stderr, "Error: Demasiados argumentos para 'create'\n");
            fprintf(stderr, "Uso: create <nombre_directorio>\n");
            return;
        }
        if (mkdir(trozos[1], 0755) == -1)
            perror("Error al crear el directorio");
    }
    else{
        fprintf(stderr, "Error: Faltan argumentos\n");
        fprintf(stderr, "Uso: create [-f] <nombre>\n");
    }
}

void Cmd_lseek(char *trozos[]){
    int df, offset, whence;

    // Comprobamos que tenemos exactamente 3 argumentos
    if (trozos[1] == NULL || trozos[2] == NULL || trozos[3] == NULL) {
        fprintf(stderr, "Error: Faltan argumentos para lseek\n");
        fprintf(stderr, "Uso: lseek <descriptor> <desplazamiento> <referencia>\n");
        return;
    }
    if (trozos[4] != NULL) {
        fprintf(stderr, "Error: Demasiados argumentos para lseek\n");
        fprintf(stderr, "Uso: lseek <descriptor> <desplazamiento> <referencia>\n");
        return;
    }

    //Los argumentos nos llegan como texto. Los convertimos a los tipos de datos que necesita lseek()
    df = atoi(trozos[1]);
    offset = atoi(trozos[2]);

    //La llamada al sistema no entiende "SEEK_SET" como texto
    //Necesitamos convertir esa cadena a la constante numérica correspondiente
    if(strcmp(trozos[3], "SEEK_SET") == 0){
        whence = SEEK_SET; //Principio del fichero
    }else if(strcmp(trozos[3], "SEEK_CUR") == 0){
        whence = SEEK_CUR; //Posición actual
    }else if(strcmp(trozos[3], "SEEK_END") == 0){
        whence = SEEK_END; //Final del fichero
    }else{
        fprintf(stderr, "Error: Referencia '%s' no válida para lseek.\n", trozos[3]);
        fprintf(stderr, "Use SEEK_SET, SEEK_CUR, o SEEK_END.\n");
        return;
    }

    //Intentamos posicionar el puntero y guardamos el resultado
    //lseek() devuelve la nueva posición (un número >= 0) si tiene éxito, o -1 si falla.
    if(lseek(df, offset, whence) == -1){
        perror("Error al reposicionar el puntero del fichero");
    }
}

void Cmd_writestr(char *trozos[]) {
    int df;
    ssize_t bytes_escritos;
    int i;

    if(trozos[1] == NULL || trozos[2] == NULL){
        fprintf(stderr, "Error: Faltan argumentos para writestr\n");
        fprintf(stderr, "Uso: writestr <descriptor> <cadena>\n");
        return;
    }

    df = atoi(trozos[1]);
    //Comprobamos que atoi dio error, y no se escribio manualmente descriptor "0"
    if(df == 0 && strcmp(trozos[1], "0") != 0){
        fprintf(stderr, "Error: El descriptor de fichero '%s' no es un número válido.\n", trozos[1]);
        return;
    }

    for(i = 2; trozos[i] != NULL; i++){
        //Escribimos la palabra actual
        bytes_escritos = write(df, trozos[i], strlen(trozos[i]));
        if (bytes_escritos == -1){
            perror("Error al escribir en el fichero");
            return;
        }

        //Si no es la última palabra, escribimos un espacio después
        if(trozos[i + 1] != NULL){
            bytes_escritos = write(df, " ", 1); //Escribimos el carácter de espacio
            if(bytes_escritos == -1){
                perror("Error al escribir en el fichero");
                return;
            }
        }
    }
}

void Cmd_erase(char *trozos[], tFileList *file_list) {

    struct stat file_info; //Donde lstat guardará la información.
    const char *path;
    int i;

    if(trozos[1] == NULL) {
        fprintf(stderr, "Error: Faltan argumentos para erase\n");
        fprintf(stderr, "Uso: erase <fichero1> [fichero2] ...\n");
        return;
    }
    //Se procesan los ficheros uno por uno
    for(i = 1; trozos[i] != NULL; i++) {
        path = trozos[i]; //El nombre del elemento a borrar en esta iteración.

        if(lstat(path, &file_info) == -1) {
            perror(path); //perror imprimirá el nombre del fichero y el error.
            continue;
        }

        if(S_ISDIR(file_info.st_mode)) {
            // Si ES un directorio, usamos rmdir().
            if(rmdir(path) == -1) {
                perror(path);
            }
        }else{
            // Si NO es un directorio usamos unlink().
            if(unlink(path) == -1) {
                perror(path);
            }else{
                //Si unlink es exitoso, se borra el fichero de lista de ficheros abiertos
                EliminarPorNombre(file_list, path);
            }
        }
    }
}

void Cmd_delrec(char *trozos[]){
    int i;

    if(trozos[1] == NULL){
        fprintf(stderr, "Error: Faltan argumentos para delrec\n");
        fprintf(stderr, "Uso: delrec <fichero1> [fichero2] ...\n");
        return;
    }

    for(i = 1; trozos[i] != NULL; i++)
        borrar_recursivo(trozos[i]);
}

void Cmd_getdirparams(char *trozos[]) {

    if(trozos[1] != NULL) {
        fprintf(stderr, "Error: Demasiados argumentos.\nUso: getdirparams\n");
        return;
    }

    printf("Parámetros de 'dir': %s, %s, %s, %s\n",
           g_dirparams.long_format ? "long" : "corto",
           g_dirparams.show_link_dest ? "link" : "nolink",
           g_dirparams.omit_hidden ? "nohid" : "hid",
           g_dirparams.recurse == RECURSE_AFTER ? "reca" : (g_dirparams.recurse == RECURSE_BEFORE ? "recb" : "norec"));
}

void Cmd_setdirparams(char *trozos[]) {
    if(trozos[1] == NULL) {
        fprintf(stderr, "Error: Faltan argumentos.\nUso: setdirparams <opcion>\n");
        return;
    }
    if(trozos[2] != NULL) {
        fprintf(stderr, "Error: Demasiados argumentos.\nUso: setdirparams <opcion>\n");
        return;
    }

    const char *param = trozos[1];
    if      (strcmp(param, "long") == 0)   g_dirparams.long_format = 1;
    else if (strcmp(param, "short") == 0)  g_dirparams.long_format = 0;
    else if (strcmp(param, "link") == 0)   g_dirparams.show_link_dest = 1;
    else if (strcmp(param, "nolink") == 0) g_dirparams.show_link_dest = 0;
    else if (strcmp(param, "hid") == 0)    g_dirparams.omit_hidden = 0;
    else if (strcmp(param, "nohid") == 0)  g_dirparams.omit_hidden = 1;
    else if (strcmp(param, "reca") == 0)   g_dirparams.recurse = RECURSE_AFTER;
    else if (strcmp(param, "recb") == 0)   g_dirparams.recurse = RECURSE_BEFORE;
    else if (strcmp(param, "norec") == 0)  g_dirparams.recurse = RECURSE_NONE;
    else {
        fprintf(stderr, "Error: Parámetro '%s' no reconocido.\n", param);
    }
}

void Cmd_dir(char *trozos[]) {
    int list_content_flag = 0;
    int first_path_idx = 1;
    int i;

    //Comprobamos si el primer argumento es el flag -d
    if(trozos[1] != NULL && strcmp(trozos[1], "-d") == 0) {
        list_content_flag = 1;
        first_path_idx = 2;
    }

    //Si no se especifican rutas, se lista el directorio actual "."
    if(trozos[first_path_idx] == NULL) {
        listar_path(".", list_content_flag);
        return;
    }

    //Se itera sobre todas las rutas proporcionadas
    for(i = first_path_idx; trozos[i] != NULL; i++) {
        listar_path(trozos[i], list_content_flag);
    }
}

void Cmd_malloc(char *trozos[], tMemList *list){
    size_t tam;
    void *p;
    void *addr_free;
    MemNode *node;

    //malloc
    if(trozos[1] == NULL){
        printMallocList(list);
        return;
    }

    //malloc -free n
    if(strcmp(trozos[1], "-free") == 0){
        if (trozos[2] == NULL) {
            fprintf(stderr, "Error: Faltan argumentos para malloc -free\n");
            fprintf(stderr, "Uso: malloc -free <tamano>\n");
            return;
        }

        tam = (size_t) strtoul(trozos[2], NULL, 10);
        if (tam == 0) {
            fprintf(stderr, "Error: No se asignan bloques de 0 bytes\n");
            return;
        }

        node = findMallocBlock(list, tam);
        if (node == NULL) {
            fprintf(stderr, "Error: No hay bloque de ese tamaño asignado con malloc\n");
            return;
        }

        addr_free = node->addr;

        EliminarNodoDireccion(list, addr_free); //perdemos referencia a esa dirMem
        printf("Liberados %lu bytes en %p\n", (unsigned long)tam, addr_free);
        free(addr_free); //vaciamos el bloque en la dirMem addr

        return;
    }

    //malloc n
    tam = (size_t) strtoul(trozos[1], NULL, 10);

    if (tam == 0) {
        fprintf(stderr, "Error: No se asignan bloques de 0 bytes\n");
        return;
    }

    p = malloc(tam);

    if (p == NULL) {
        perror("Imposible asignar memoria malloc");
    } else {
        if (insertMallocBlock(list, p, tam) == -1) {
            free(p);
        } else {
            printf("Asignados %lu bytes en %p\n", (unsigned long) tam, p);
        }
    }
}

void Cmd_mmap(char *trozos[], tMemList *list) {

    //mmap
    if (trozos[1] == NULL) {
        printMappedList(list);
        return;
    }

    //mmap -free fich
    if (strcmp(trozos[1], "-free") == 0) {

        if (trozos[2] == NULL || trozos[3] != NULL) {
            fprintf(stderr, "Error: Faltan argumentos para mmap -free\n");
            fprintf(stderr, "Uso: mmap -free <fichero>\n");
            return;
        }

        const char *filename = trozos[2];

        // Buscar un bloque mapeado con ese nombre
        MemNode *node = findMappedBlock(list, filename);
        if (node == NULL) {
            fprintf(stderr, "Error: El fichero '%s' no está mapeado.\n", filename);
            return;
        }

        // Desmapear el bloque
        if (munmap(node->addr, node->size) == -1) {
            perror("Error al desmapear fichero");
            return;
        }

        printf("Fichero %s desmapeado de %p\n", filename, node->addr);

        // Cerrar descriptor asociado
        if (node->fd != -1)
            close(node->fd);

        // Eliminar nodo de la lista
        EliminarNodoDireccion(list, node->addr);

        return;
    }

    // CASO 3: mmap fich perm
    const char *filename = trozos[1];
    char *perm = trozos[2];
    int protection = 0;

    // Si no se dan permisos, por defecto lectura
    if (perm == NULL) {
        protection = PROT_READ;
    } else {
        if (strchr(perm, 'r')) protection |= PROT_READ;
        if (strchr(perm, 'w')) protection |= PROT_WRITE;
        if (strchr(perm, 'x')) protection |= PROT_EXEC;
    }

    // Mapear fichero
    void *p = MapearFichero(filename, protection, list);

    if (p == NULL) {
        perror("Imposible mapear fichero");
        return;
    }

    printf("Fichero %s mapeado en %p con permisos '%s'\n",
           filename, p, perm ? perm : "r");

}

void Cmd_shared(char *trozos[], tMemList *list) {
    key_t cl;
    size_t tam;
    void *p;

    //shared
    if (trozos[1] == NULL) {
        printSharedList(list);
        return;
    }

    if (strcmp(trozos[1], "-create") == 0) {
        if (trozos[2] == NULL || trozos[3] == NULL) {
            printSharedList(list); // Si faltan args, listamos
            return;
        }

        cl = (key_t) strtoul(trozos[2], NULL, 10);
        tam = (size_t) strtoul(trozos[3], NULL, 10);

        if (tam == 0) {
            fprintf(stderr, "Error: No se asignan bloques de 0 bytes\n");
            return;
        }

        if ((p = ObtenerMemoriaShmget(cl, tam, list)) != NULL)
            printf("Asignados %lu bytes en %p\n", (unsigned long) tam, p);
        else
            fprintf(stderr, "Error: Imposible asignar memoria compartida clave %lu: %s\n",
                    (unsigned long) cl, strerror(errno));
        return;
    }

    if (strcmp(trozos[1], "-free") == 0) {
        if (trozos[2] == NULL) {
            fprintf(stderr, "Error: shared -free necesita una clave\n");
            return;
        }

        cl = (key_t) strtoul(trozos[2], NULL, 10);

        p = DireccionNodoShared(list, cl);
        if (p == NULL) {
            fprintf(stderr, "Error: No hay bloque de esa clave mapeado en el proceso\n");
            return;
        }

        //Desvinculamos la memoria compartida
        shmdt(p);

        //La quitamos de la lista (pasamos la dirección p)
        if (EliminarNodoDireccion(list, p) == -1) {
            fprintf(stderr, "Error: Imposible quitar de lista: %s\n", strerror(errno));
        } else {
            printf("Memoria compartida de clave %lu desvinculada en %p\n", (unsigned long)cl, p);
        }
        return;
    }

    if (strcmp(trozos[1], "-delkey") == 0) {
        if (trozos[2] == NULL) {
            fprintf(stderr, "Error: shared -delkey necesita una clave\n");
            return;
        }
        //Llamamos a la función auxiliar pasando el puntero al argumento de la clave
        do_SharedDelkey(&trozos[2]);
        return;
    }


    cl = (key_t) strtoul(trozos[1], NULL, 10);

    if ((p = ObtenerMemoriaShmget(cl, 0, list)) != NULL)
        printf("Asignada memoria compartida de clave %lu en %p\n", (unsigned long) cl, p);
    else
        fprintf(stderr, "Error: Imposible asignar memoria compartida clave %lu: %s\n",
                (unsigned long) cl, strerror(errno));
}

void Cmd_free(char *trozos[], tMemList *list) {
    void *addr;
    MemNode *node;

    if (trozos[1] == NULL || trozos[2] != NULL) {
        fprintf(stderr, "Error: Uso incorrecto de free\n");
        fprintf(stderr, "Uso: free <addr>\n");
        return;
    }

    addr = CadenaToPointer(trozos[1]);
    if (addr == NULL) {
        perror("Error al convertir la dirección");
        return;
    }

    node = findBlockByAddr(list, addr);
    if (node == NULL) {
        fprintf(stderr,
                "Error: La dirección %s no corresponde a ningún bloque gestionado por el shell\n",
                trozos[1]);
        return;
    }

    int ok = 1;
    switch (node->type) {
        case MALLOC:
            free(node->addr);
            printf("Liberado bloque malloc de %zu bytes en %p\n",
                   node->size, node->addr);
            break;

        case SHARED:
            if (shmdt(node->addr) == -1) {
                perror("Error al desvincular memoria compartida");
                ok = 0;
            } else {
                printf("Desvinculado bloque shared (key %d) de %zu bytes en %p\n",
                       node->key, node->size, node->addr);
            }
            break;

        case MAPPED:
            if (munmap(node->addr, node->size) == -1) {
                perror("Error al desmapear memoria");
                ok = 0;
            } else {
                printf("Desmapeado bloque mmap de %zu bytes en %p (fichero %s)\n",
                       node->size, node->addr, node->filename);
            }
            break;

        default:
            fprintf(stderr, "Error: tipo de bloque desconocido\n");
            ok = 0;
            break;
    }

    if (ok) {
        if (!EliminarNodoDireccion(list, addr)) {
            fprintf(stderr, "Aviso: no se pudo eliminar el nodo de la lista (dirección %p)\n", addr);
        }
    }
}

void Cmd_memfill(char *trozos[], tMemList *list) {
    void *addr;
    size_t cont;
    unsigned char ch;

    // Comprobar argumentos
    if (trozos[1] == NULL || trozos[2] == NULL || trozos[3] == NULL) {
        fprintf(stderr, "Error: Faltan argumentos para memfill\n");
        fprintf(stderr, "Uso: memfill <addr> <cont> <ch>\n");
        return;
    }


    addr = CadenaToPointer(trozos[1]);
    if (addr == NULL) {
        perror("Error: dirección no válida");
        return;
    }


    cont = (size_t) strtoul(trozos[2], NULL, 10);
    if (cont == 0) {
        fprintf(stderr, "Error: cont debe ser mayor que 0\n");
        return;
    }


    ch = (unsigned char) trozos[3][0];



    LlenarMemoria(addr, cont, ch);

    printf("Memoria desde %p rellenada con %zu bytes del carácter '%c'.\n",
           addr, cont, ch);
}

void Cmd_memdump(char *trozos[], tMemList *list) {
    void *address;
    size_t nbytes;
    unsigned char *ptr;


    if (trozos[1] == NULL || trozos[2] == NULL || trozos[3] != NULL) {
        fprintf(stderr, "Error: Faltan argumentos para memdump\n");
        fprintf(stderr, "Uso: memdump <addr> <cont>\n");
        return;
    }

    address = CadenaToPointer(trozos[1]);
    if (address == NULL) {
        perror("Error al convertir la direccion");
        return;
    }

    nbytes = (size_t) strtoul(trozos[2], NULL, 10);
    if (nbytes == 0) {
        fprintf(stderr, "Error: nbytes debe ser mayor que 0\n");
        return;
    }

    ptr = (unsigned char *) address;

    printf("****** Dump de memoria desde %p (%zu bytes) ******\n", address, nbytes);

    for (size_t offset = 0; offset < nbytes; offset += 16) {
        size_t line_bytes = (nbytes - offset >= 16) ? 16 : (nbytes - offset);

        // Hex
        for (size_t i = 0; i < line_bytes; i++) {
            printf("%02X ", ptr[offset + i]);
        }
        for (size_t i = line_bytes; i < 16; i++) {
            printf("   ");
        }

        printf(" | ");

        // ASCII
        for (size_t i = 0; i < line_bytes; i++) {
            unsigned char c = ptr[offset + i];
            if (c == '\n') {
                printf("\\n"); //imprime \n
            } else if (c == '\t') {
                printf("\\t"); //imprime \t
            } else if (c == '\r') {
                printf("\\r"); //imprime \r
            } else if (isprint(c)) {
                printf(" %c", c); //espacio + caracter para alinear
            } else {
                printf(" ."); //espacio + punto para alinear
            }
        }
        printf("\n");
    }
}

void Cmd_mem(char *trozos[], tMemList *list){
    int auto_v1 = 1, auto_v2 = 2, auto_v3 = 3;
    static int static_v1, static_v2, static_v3;
    static int static_init_v1 = 100, static_init_v2 = 200, static_init_v3 = 300;

    int do_vars = 0;
    int do_funcs = 0;
    int do_blocks = 0;
    int do_pmap = 0;

    if (trozos[1] == NULL || strcmp(trozos[1], "-all") == 0) {
        do_vars = 1;
        do_funcs = 1;
        do_blocks = 1;
    }
    else if (strcmp(trozos[1], "-blocks") == 0) {
        do_blocks = 1;
    }
    else if (strcmp(trozos[1], "-funcs") == 0) {
        do_funcs = 1;
    }
    else if (strcmp(trozos[1], "-vars") == 0) {
        do_vars = 1;
    }
    else if (strcmp(trozos[1], "-pmap") == 0) {
        do_pmap = 1;
    }
    else {
        printf("Opción no reconocida: %s\n", trozos[1]);
        return;
    }

    if (do_vars) {
        printf("Variables locales (automáticas): \t%p, %p, %p\n", &auto_v1, &auto_v2, &auto_v3);
        printf("Variables globales (externas):   \t%p, %p, %p\n", &global_v1, &global_v2, &global_v3);
        printf("Var (externas) inicializadas:    \t%p, %p, %p\n", &global_init_v1, &global_init_v2, &global_init_v3);
        printf("Variables estáticas:             \t%p, %p, %p\n", &static_v1, &static_v2, &static_v3);
        printf("Var (estáticas) inicializadas:   \t%p, %p, %p\n", &static_init_v1, &static_init_v2, &static_init_v3);
    }

    if (do_funcs) {
        printf("Funciones programa: \t%p (Cmd_malloc), %p (Cmd_mem), %p (Cmd_help)\n", Cmd_malloc, Cmd_mem, Cmd_help);
        printf("Funciones librería: \t%p (printf), %p (malloc), %p (strcmp)\n", printf, malloc, strcmp);
    }

    if (do_blocks) {
        // Usamos la función de tu lista
        printMemList(list);
    }

    if (do_pmap) {
        Do_pmap();
    }
}
void Cmd_recurse(char *trozos[]) {
    int n;

    // Validar argumentos
    if (trozos[1] == NULL) {
        fprintf(stderr, "Error: Faltan argumentos. Uso: recurse <n>\n");
        return;
    }

    // Convertir argumento a entero
    n = atoi(trozos[1]);

    if (n < 0) {
        fprintf(stderr, "Error: El número de recursiones debe ser >= 0\n");
        return;
    }

    // Llamar a la función recursiva
    Recursiva(n);
}

void Cmd_readfile(char *trozos[]) {
    void *p;
    size_t cont = -1;
    ssize_t n;

    // Validar argumentos
    if (trozos[1] == NULL || trozos[2] == NULL) {
        fprintf(stderr, "Error: Faltan argumentos. Uso: readfile <fichero> <addr> [cont]\n");
        return;
    }

    // Convertir dirección
    p = CadenaToPointer(trozos[2]);
    if (p == NULL) {
        fprintf(stderr, "Error: Dirección de memoria inválida\n");
        return;
    }

    // Leer tamaño
    if (trozos[3] != NULL) {
        cont = (size_t) strtoul(trozos[3], NULL, 10);
    }

    // Llamar a la función auxiliar
    if ((n = LeerFichero(trozos[1], p, cont)) == -1) {
        perror("Error al leer fichero");
    } else {
        printf("Leídos %zd bytes de %s en %p\n", n, trozos[1], p);
    }
}

void Cmd_writefile(char *trozos[]) {
    void *p;
    size_t cont;
    ssize_t n;
    int overwrite = 1;

    // Validar argumentos
    if (trozos[1] == NULL || trozos[2] == NULL || trozos[3] == NULL) {
        fprintf(stderr, "Error: Faltan argumentos. Uso: writefile <fichero> <addr> <cont>\n");
        return;
    }

    // Convertir dirección
    p = CadenaToPointer(trozos[2]);
    if (p == NULL) {
        fprintf(stderr, "Error: Dirección de memoria inválida\n");
        return;
    }

    // Leer tamaño
    cont = (size_t) strtoul(trozos[3], NULL, 10);
    if (cont == 0) {
        fprintf(stderr, "Error: No se pueden escribir 0 bytes\n");
        return;
    }

    // Llamar a la función auxiliar
    if ((n = EscribirFichero(trozos[1], p, cont, overwrite)) == -1) {
        perror("Error al escribir fichero");
    } else {
        printf("Escritos %zd bytes desde %p en %s\n", n, p, trozos[1]);
    }
}

void Cmd_read(char *trozos[]) {
    void *p;
    size_t cont = -1;
    ssize_t n;
    int df;
    struct stat s;

    if (trozos[1] == NULL || trozos[2] == NULL || trozos[3] == NULL) {
        fprintf(stderr, "Error: Faltan argumentos. Uso: read <df> <addr> <cont>\n");
        return;
    }

    df = atoi(trozos[1]);

    p = CadenaToPointer(trozos[2]);
    if (p == NULL) {
        fprintf(stderr, "Error: Dirección de memoria inválida\n");
        return;
    }

    cont = (size_t) strtoul(trozos[3], NULL, 10);

    if (fstat(df, &s) == -1) {
        perror("Error al acceder al descriptor");
        return;
    }

    if ((n = read(df, p, cont)) == -1) {
        perror("Error al leer del descriptor");
    } else {
        printf("Leídos %zd bytes del descriptor %d en %p\n", n, df, p);
    }
}

void Cmd_write(char *trozos[]) {
    void *p;
    size_t cont;
    ssize_t n;
    int df;

    if (trozos[1] == NULL || trozos[2] == NULL || trozos[3] == NULL) {
        fprintf(stderr, "Error: Faltan argumentos. Uso: write <df> <addr> <cont>\n");
        return;
    }

    df = atoi(trozos[1]);

    p = CadenaToPointer(trozos[2]);
    if (p == NULL) {
        fprintf(stderr, "Error: Dirección de memoria inválida\n");
        return;
    }

    cont = (size_t) strtoul(trozos[3], NULL, 10);

    if ((n = write(df, p, cont)) == -1) {
        perror("Error al escribir en descriptor");
    } else {
        printf("Escritos %zd bytes al descriptor %d desde %p\n", n, df, p);
    }
}

void Cmd_ejecutar(char *trozos[], tProcessList *list, char *envp[]) {
    pid_t pid;
    int background = 0;
    int j, k, i = 0;
    int prioridad = 0;
    int camb_prio = 0;

    //-1. detectar si hay '&' al final para ejecución en segundo plano
    while (trozos[i] != NULL)
        i++; // Contar trozos
    if (i > 0 && strcmp(trozos[i-1], "&") == 0) {
        background = 1;
        trozos[i-1] = NULL; //eliminamos el '&' para que execvp no se confunda
    }

    for (j = 0; trozos[j] != NULL; j++) {
        if (trozos[j][0] == '@') {
            prioridad = atoi(trozos[j] + 1);
            camb_prio = 1;
            k = j;
            while (trozos[k] != NULL) {
                trozos[k] = trozos[k+1];
                k++;
            }
            j--;
        }
    }

    //-2.hacer el fork
    pid = fork();

    if (pid == -1) {
        perror("Error en fork");
        return;
    }

    if (pid == 0) { //proceso hijo
        if (camb_prio) {
            if (setpriority(PRIO_PROCESS, 0, prioridad) == -1)
                perror("Error al cambiar prioridad");
        }
        if (execvp(trozos[0], trozos) == -1) { //execvp busca el programa en el PATH
            perror("Error execvp: No se pudo ejecutar el comando");
            exit(EXIT_FAILURE); //si execvp falla salir
        }
    } else {
        if (background) { //proceso padre (shell)
            //si es background no esperamos, lo añadimos a la lista y seguimos
            printf("Proceso en segundo plano lanzado: %d\n", pid);
            insertProcess(list, pid, trozos[0]);
        } else {
            //si es foreground, el shell espera a que termine
            waitpid(pid, NULL, 0);
        }
    }
}

void Cmd_uid(char *trozos[]){
    struct passwd *p_info, *p;
    uid_t real = getuid();
    uid_t efect = geteuid();
    int res, es_login = 0;   //flag para -l (setuid)
    char *id_str;       //puntero al string que contiene el ID
    uid_t nuevo_uid;

    //uid, uid -get
    if(trozos[1] == NULL || strcmp(trozos[1], "-get") == 0){
        printf("Credencial Real: %d, (%s)\n", real, (p_info = getpwuid(real)) ? p_info->pw_name : "???");
        printf("Credencial Efectiva: %d, (%s)\n", efect, (p_info = getpwuid(efect)) ? p_info->pw_name : "???");
        return;
    }

    //uid -set
    if (strcmp(trozos[1], "-set") == 0) {
        if (trozos[2] == NULL) {
            fprintf(stderr, "Error: Faltan argumentos. Uso: uid -set [-l] id\n");
            return;
        }

        // Detectar si la opción -l (login) está presente
        if (strcmp(trozos[2], "-l") == 0) {
            if (trozos[3] == NULL) {
                fprintf(stderr, "Error: Faltan argumentos. Uso: uid -set [-l] id\n");
                return;
            }
            es_login = 1;
            id_str = trozos[3];
        } else {
            //no hay -l, el ID está en trozos[2]
            id_str = trozos[2];
        }

        if (es_login) {
            //opción -l: Usamos setuid(). Cambia REAL, EFECTIVA y GUARDADA (login)
            //permite poner también el usuario
            p = getpwnam(id_str);

            if (p!=NULL)
                nuevo_uid = p->pw_uid;
            else
                nuevo_uid = (uid_t) atoi(id_str);
            res = setuid(nuevo_uid);
        } else {
            // Opción normal: Usamos seteuid(). Cambia solo la EFECTIVA.
            nuevo_uid = (uid_t) atoi(id_str);
            res = seteuid(nuevo_uid);
        }

        if (res == -1) {
            perror("Error al cambiar credencial");
        } else {
            if (es_login)
                printf("Credencial (Login) cambiada a %d\n", nuevo_uid);
            else
                printf("Credencial (Efectiva) cambiada a %d\n", nuevo_uid);
        }
    } else {
        printf("Opción no válida para uid: %s\n", trozos[1]);
        printf("Uso: uid [-get] | uid -set [-l] id\n");
    }
}

void Cmd_envvar(char *trozos[], char *envp[]) {
    extern char **environ;

    if (trozos[1] == NULL) {
        fprintf(stderr, "Uso: envvar -show VAR | envvar -change [-a|-e|-p] var valor\n");
        return;
    }

    //nvvar -show VAR
    if (strcmp(trozos[1], "-show") == 0) {
        char *var;
        int pos;
        char *val;

        if (trozos[2] == NULL || trozos[3] != NULL) {
            fprintf(stderr, "Uso: envvar -show VAR\n");
            return;
        }

        var = trozos[2];

        //buscar en envp, el tercer argumento de main
        pos = BuscarVariable(var, envp);
        if (pos != -1)
            printf("envp     : %s  (addr=%p)\n", envp[pos], (void *)envp[pos]);
        else
            printf("envp     : %s no encontrada\n", var);

        //buscar en environ
        pos = BuscarVariable(var, environ);
        if (pos != -1)
            printf("environ  : %s  (addr=%p)\n", environ[pos], (void *)environ[pos]);
        else
            printf("environ  : %s no encontrada\n", var);

        //getenv
        val = getenv(var);
        if (val != NULL)
            printf("getenv   : %s  (addr=%p)\n", val, (void *)val);
        else
            printf("getenv   : %s no encontrada\n", var);

        return;
    }

    //envvar -change [-a|-e|-p] VAR VAL
    if (strcmp(trozos[1], "-change") == 0) {
        char metodo;
        char *var, *valor;

        //validar que tenemos todos los argumentos
        if (trozos[2] == NULL || trozos[3] == NULL || trozos[4] == NULL) {
            fprintf(stderr, "Uso: envvar -change [-a|-e|-p] var valor\n");
            return;
        }

        //validar metodo
        if (strcmp(trozos[2], "-a") == 0) {
            metodo = 'a';
        } else if (strcmp(trozos[2], "-e") == 0) {
            metodo = 'e';
        } else if (strcmp(trozos[2], "-p") == 0) {
            metodo = 'p';
        } else {
            fprintf(stderr, "Uso: envvar -change [-a|-e|-p] var valor\n");
            return;
        }

        var   = trozos[3];
        valor = trozos[4];

        if (metodo == 'a') {
            if (CambiarVariable(var, valor, envp) == -1)
                fprintf(stderr, "envvar -change -a: variable %s no encontrada\n", var);
        } else if (metodo == 'e') {
            if (CambiarVariable(var, valor, environ) == -1)
                fprintf(stderr, "envvar -change -e: variable %s no encontrada\n", var);
        } else { // metodo == 'p'
            char *aux = malloc(strlen(var) + strlen(valor) + 2);
            if (aux == NULL) {
                perror("malloc en envvar -change -p");
                return;
            }
            strcpy(aux, var);
            strcat(aux, "=");
            strcat(aux, valor);
            if (putenv(aux) != 0) {
                perror("putenv");
            }
        }
        return;
    }

    fprintf(stderr, "Uso: envvar -show VAR | envvar -change [-a|-e|-p] VAR VAL\n");
}

void Cmd_showenv(char *trozos[], char *envp[]) {
    extern char **environ;
    int i;

    if (trozos[1] == NULL) {
        //showenv: usar tercer parámetro de main envp
        for (i = 0; envp[i] != NULL; i++) {
            printf("%s\n", envp[i]);
        }
    } else if (strcmp(trozos[1], "-environ") == 0 && trozos[2] == NULL) {
        //showenv -environ: usar variable global environ
        char **p = environ;
        while (*p != NULL) {
            printf("%s\n", *p);
            p++;
        }
    } else if (strcmp(trozos[1], "-addr") == 0 && trozos[2] == NULL) {
        //showenv -addr: mostrar direcciones
        printf("envp      = %p (almacenado en %p)\n", (void *)envp, (void *)&envp);
        printf("environ   = %p (almacenado en %p)\n", (void *)environ, (void *)&environ);
    } else {
        fprintf(stderr, "Uso: showenv [-environ|-addr]\n");
    }
}

void Cmd_jobs(char *trozos[], tProcessList *list) {
    if (trozos[1] != NULL) {
        fprintf(stderr, "Uso: jobs\n");
        return;
    }
    printProcessList(list);
}

void Cmd_deljobs(char *trozos[], tProcessList *list){

    if(trozos[1] == NULL){
        fprintf(stderr, "Error: Faltan argumentos\n");
        printf("Uso: deljobs -term | -sig\n");
        return;
    }
    //comprobar antes de borrar si algun proceso cambio de estado
    updateProcessStatus(list);

    if(strcmp(trozos[1], "-term") == 0)
        removeProcessesByState(list, TERMINADO);
    else if(strcmp(trozos[1], "-sig") == 0)
        removeProcessesByState(list, SENALADO);
    else
        printf("Opción no válida. Uso: deljobs -term | -sig\n");
}

void Cmd_fork(tProcessList *list){
    pid_t pid;

    if((pid = fork()) == 0){ //hay 2 procesos, en el padre pid = pid del hijo y en el hijo pid = 0
        clearProcessList(list); //vaciamos la lista clonada del padre
        printf("ejecutando proceso %d\n", getpid());
        exit(0); //finalizamos hijo para que no salga de la función como otro proceso shell
    }
    else if(pid != -1){
        waitpid(pid, NULL, 0); //NULL pq nos da igual como murio, solo saber cuando;
                                      // 0 para que el padre se bloquee hasta que el hijo no muera
                                      //pid = pidHijo, mientras el otro proceso no mate al hijo, el proceso padre espera
    }
    else
        perror("Error en fork");
}

void Cmd_exec(char *trozos[]) {
    int i, j;
    int prioridad = 0;
    int camb_prio = 0;

    if (trozos[1] == NULL) {
        fprintf(stderr, "Error: Faltan argumentos\n");
        fprintf(stderr, "Uso: exec <prog> [args...] [@pri]\n");
        return;
    }
    // eliminando "@pri" y tratando "&"
    char *argv_exec[MAX / 2];
    j = 0;

    for (i = 1; trozos[i] != NULL; i++) {

        // Ignorar '&' si aparece (exec no puede ir a background)
        if (strcmp(trozos[i], "&") == 0) {
            fprintf(stderr, "Aviso: 'exec' reemplaza el shell; ignorando '&'.\n");
            continue;
        }

        // Detectar @pri
        if (trozos[i][0] == '@' && trozos[i][1] != '\0') {
            prioridad = atoi(trozos[i] + 1);
            camb_prio = 1;
            continue; // no lo metemos en argv
        }
        argv_exec[j++] = trozos[i];
    }
    argv_exec[j] = NULL;
    if (argv_exec[0] == NULL) {
        fprintf(stderr, "Error: No se indicó programa a ejecutar.\n");
        return;
    }
    // Cambiar prioridad ANTES del execvp (afecta al proceso actual)
    if (camb_prio) {
        if (setpriority(PRIO_PROCESS, 0, prioridad) == -1) {
            perror("Error al cambiar prioridad");
        }
    }
    execvp(argv_exec[0], argv_exec);
    perror("Fallo en execvp");
}