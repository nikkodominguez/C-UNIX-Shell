#ifndef P0_FILE_LIST_H
#define P0_FILE_LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define MAX_FILES 100

typedef struct FileNode{
    int descriptor;
    char filename[MAX_FILES];
    int open_flags;
    struct FileNode *next;
}FileNode;

typedef struct{
    FileNode *head;
}tFileList;

//Funciones
void createFileList(tFileList *list);
void AnadirAFicherosAbiertos(tFileList *list, int df, const char *name, int flags);
int EliminarDeFicherosAbiertos(tFileList *list, int df);
void ListarFicherosAbiertos(tFileList *list);
void clearFileList(tFileList *list);
const char* NombreFicheroDescriptor(tFileList *list, int df);
//Necesaria para erase (eliminar por nombre)
int EliminarPorNombre(tFileList *list, const char *name);

#endif //P0_FILE_LIST_H
