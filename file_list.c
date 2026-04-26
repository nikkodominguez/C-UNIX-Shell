#include "file_list.h"

void createFileList(tFileList *list){
    list->head = NULL;
}

void AnadirAFicherosAbiertos(tFileList *list, int df, const char *name, int flags){
    //Reservar memoria
    FileNode *newNode = (FileNode *) malloc (sizeof(FileNode));
    if(newNode == NULL){
        perror("(perror) Error al asignar memoria para el nuevo fichero");
        return;
    }

    //Rellenar datos
    newNode->descriptor = df;
    strncpy(newNode->filename, name, MAX_FILES);
    newNode->filename[MAX_FILES - 1] = '\0'; //añadimos manualmente el \0, ya que si strncpy copia name con
    newNode->open_flags = flags;
    newNode->next = NULL;

    //Insertar al final de la lista
    if(list->head == NULL)
        list->head = newNode;
    else{
        FileNode *current = list->head;
        while(current->next != NULL)
            current = current->next;
        current->next = newNode;
    }
}

int EliminarDeFicherosAbiertos(tFileList *list, int df){
    FileNode *current, *previous;

    if(list->head == NULL)
        return 0;

    //Nodo a borrar es el primero
    if(list->head->descriptor == df){
        current = list->head;
        list->head = list->head->next;
        free(current);
        return 1;
    }

    //Nodo en medio o en el final
    previous = list->head;
    current = list->head->next;

    while(current != NULL && current->descriptor != df){
        previous = current;
        current = current->next;
    }

    //Se llego al final de la lista sin encontrar descriptor
    if(current == NULL){
        return 0;
    }

    previous->next = current->next;
    free(current);

    return 1;
}

void ListarFicherosAbiertos(tFileList *list) {
    FileNode *current = list->head;
    if (current == NULL) {
        printf("No hay ficheros abiertos por el shell.\n");
        return;
    }

    printf("Ficheros abiertos por el shell:\n");
    while (current != NULL) {
        printf("  Descriptor: %d, Nombre: %s, Modo: 0%o\n",
               current->descriptor, current->filename, current->open_flags);
        current = current->next;
    }
}

void clearFileList(tFileList *list) {
    FileNode *current = list->head;
    FileNode *nextNode;

    while (current != NULL) {
        nextNode = current->next;
        free(current);
        current = nextNode;
    }
    list->head = NULL;
}

const char* NombreFicheroDescriptor(tFileList *list, int df) {
    FileNode *current = list->head;
    while (current != NULL) {
        if (current->descriptor == df) {
            return current->filename;
        }
        current = current->next;
    }
    return NULL; //No se encontró el descriptor en nuestra lista
}

int EliminarPorNombre(tFileList *list, const char *name) {
    FileNode *current, *previous;

    if (list->head == NULL) return 0;

    //El nodo a borrar es el primero
    if (strcmp(list->head->filename, name) == 0) {
        current = list->head;
        list->head = list->head->next;
        free(current);
        return 1;
    }

    //Buscar en el resto de la lista
    previous = list->head;
    current = list->head->next;
    while (current != NULL && strcmp(current->filename, name) != 0) {
        previous = current;
        current = current->next;
    }

    if (current == NULL)
        return 0; //No se encontró

    previous->next = current->next;
    free(current);
    return 1;
}
