#include "memory_list.h"
#include <errno.h>
void createMemList(tMemList *list){
    list->head = NULL;
}

void clearMemList(tMemList *list){
    MemNode *current = list->head;
    MemNode *nextNode;

    while(current != NULL){
        nextNode = current->next;

        switch (current->type) {
            case MALLOC:
                free(current->addr);
                break;
            case SHARED:
                shmdt(current->addr);
                break;
            case MAPPED:
                munmap(current->addr, current->size);
                break;
        }
        free(current);
        current = nextNode;
    }
    list->head = NULL;
}

int insertMallocBlock(tMemList *list, void *addr, size_t size){
    MemNode *newNode = (MemNode *)malloc(sizeof(MemNode));
    MemNode *current;

    if(newNode == NULL){
        perror("Error al asignar memoria para el nodo de lista");
        return -1;
    }

    newNode->addr = addr;
    newNode->size = size;
    newNode->type = MALLOC;
    newNode->alloc_time = time(NULL); //obtenemos hora actual

    //inicializamos a 0 campos que no se usan en MALLOC
    newNode->key = 0;
    newNode->fd = -1;
    newNode->perms = 0;
    newNode->filename[0] = '\0';

    newNode->next = NULL;

    if(list->head == NULL)
        list->head = newNode;
    else{
        current = list->head;
        while(current->next != NULL)
            current = current->next;
        current->next = newNode;
    }
    return 0;
}

int InsertarNodoShared(tMemList *list, void *addr, size_t size, key_t key){
    MemNode *newNode = (MemNode *)malloc(sizeof(MemNode));
    MemNode *current;

    if(newNode == NULL){
        perror("Error al asignar memoria para el nodo de lista");
        return -1;
    }

    newNode->addr = addr;
    newNode->size = size;
    newNode->type = SHARED;
    newNode->alloc_time = time(NULL);
    newNode->key = key;

    newNode->fd = -1;
    newNode->perms = 0;
    newNode->filename[0] = '\0';

    newNode->next = NULL;

    if(list->head == NULL)
        list->head = newNode;
    else{
        current = list->head;
        while(current->next != NULL)
            current = current->next;
        current->next = newNode;
    }
    return 0;
}

int InsertarNodoMmap(tMemList *list, void *addr, size_t size, const char *filename, int fd, int perms){
    MemNode *newNode = (MemNode *)malloc(sizeof(MemNode));
    MemNode *current;

    if(newNode == NULL){
        perror("Error al asignar memoria para el nodo de lista");
        return -1;
    }

    newNode->addr = addr;
    newNode->size = size;
    newNode->type = MAPPED;
    newNode->alloc_time = time(NULL);
    newNode->fd = fd;
    newNode->perms = perms;
    strncpy(newNode->filename, filename, sizeof(newNode->filename) - 1);
    newNode->filename[sizeof(newNode->filename) - 1] = '\0';
    newNode->key = 0;
    newNode->next = NULL;

    if(list->head == NULL)
        list->head = newNode;
    else{
        current = list->head;
        while(current->next != NULL)
            current = current->next;

        current->next = newNode;
    }
    return 0;
}

int EliminarNodoDireccion(tMemList *list, void *addr){
    MemNode *current, *previous;

    //lista vacia
    if(list->head == NULL)
        return 0;
    //eliminar primer nodo
    if(list->head->addr == addr){
        current = list->head;
        list->head = list->head->next;
        free(current);
        return 1;
    }

    //eliminar por el medio/final
    previous = list->head;
    current = list->head->next;

    while(current != NULL && current->addr != addr){
        previous = current;
        current = current->next;
    }

    //no se encontro
    if(current == NULL)
        return 0;

    //actualizar punteros
    previous->next = current->next;
    free(current);
    return 1;
}

MemNode* findMallocBlock(tMemList *list, size_t size){
    MemNode *current = list->head;

    while(current!= NULL){
        if(current->type == MALLOC && current->size == size)
            return current;
        current = current->next;
    }
    return NULL;
}

void* DireccionNodoShared(tMemList *list, key_t key) {
    MemNode *current = list->head;
    while (current != NULL) {
        if (current->type == SHARED && current->key == key) {
            return current->addr;
        }
        current = current->next;
    }
    return NULL;
}

MemNode* findMappedBlock(tMemList *list, const char *filename){
    MemNode *current = list->head;

    while(current != NULL){
        if(current->type == MAPPED && strcmp(current->filename, filename) == 0)
            return current;
        current = current->next;
    }
    return NULL;
}

static void printAux(const MemNode *node){
    char time_str[64];
    struct tm *tm_info = localtime(&node->alloc_time);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("    %p  %zu bytes  %s", node->addr, node->size, time_str);
    switch (node->type) {
        case MALLOC:
            printf("  (malloc)\n");
            break;
        case SHARED:
            printf("  (shared)  key: %d\n", node->key);
            break;
        case MAPPED:
            printf("  (mapped)  fichero: %s  perm: %s  fd: %d\n",
                   node->filename, permsToString(node->perms), node->fd);
            break;
    }
}

void printMemList(const tMemList *list){
    MemNode *current = list->head;
    if(current == NULL){
        printf("No hay bloques de memoria asignados por el shell.\n");
        return;
    }
    printf("Bloques de memoria asignados:\n");
    while(current != NULL){
        printAux(current);
        current = current->next;
    }
}

void printMallocList(const tMemList *list){
    MemNode *current = list->head;
    int found = 0;
    while(current != NULL){
        if(current->type == MALLOC){
            if(!found){
                printf("Bloques 'malloc' asignados:\n");
                found = 1;
            }
            printAux(current);
        }
        current = current->next;
    }
    if(!found)
        printf("No hay bloques 'malloc' asignados por el shell.\n");
}

void printSharedList(const tMemList *list){
    MemNode *current = list->head;
    int found = 0;
    while(current != NULL){
        if(current->type == SHARED){
            if(!found){
                printf("Bloques 'shared' asignados:\n");
                found = 1;
            }
            printAux(current);
        }
        current = current->next;
    }
    if(!found)
        printf("No hay bloques 'shared' asignados por el shell.\n");
}

void printMappedList(const tMemList *list) {
    MemNode *current = list->head;
    int found = 0;
    while (current != NULL) {
        if (current->type == MAPPED) {
            if (!found) {
                printf("Bloques 'mmap' asignados:\n");
                found = 1;
            }
            printAux(current);
        }
        current = current->next;
    }
    if (!found) {
        printf("No hay bloques 'mmap' asignados por el shell.\n");
    }
}

const char* permsToString(int perms) {
    static char str[4];
    strcpy(str, "---");
    if (perms & PROT_READ) str[0] = 'r';
    if (perms & PROT_WRITE) str[1] = 'w';
    if (perms & PROT_EXEC) str[2] = 'x';
    return str;
}
MemNode* findBlockByAddr(tMemList *list, void *addr) {
    MemNode *curr = list->head;
    while (curr != NULL) {
        if (curr->addr == addr) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

void * CadenaToPointer(char *cadena) {
    void *p;

    if (sscanf(cadena, "%p", &p) != 1) {//if adicional, p contiene basura, si sscanf falla p puede no ser NULL
                                        // no entrar en el if(p==NULL) e imprimirse algo incorrecto
        errno = EFAULT;
        return NULL;
    }
    if (p == NULL) {
        errno = EFAULT;
    }
    return p;
}

void LlenarMemoria(void *p, size_t cont, unsigned char byte) {
    unsigned char *arr = (unsigned char *) p;
    for (size_t i = 0; i < cont; i++) {
        arr[i] = byte;
    }
}


