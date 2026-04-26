#ifndef P0_SO_MEMORY_LIST_H
#define P0_SO_MEMORY_LIST_H

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/mman.h>

typedef enum{
    MALLOC,
    SHARED,
    MAPPED
} AllocType;

typedef struct MemNode{
    void *addr; //puntero generico para guardar direccion de memoria del bloque de memoria
    size_t size; //tipo dato que devuelve sizeof, para almacenar bloques de memoria
    time_t alloc_time; //almacenar tiempo asignacion memoria
    AllocType type;
    key_t key;
    char filename[1024];
    int fd;
    int perms; //requerido para mmap
    struct MemNode *next;
}MemNode;

typedef struct{
    MemNode *head;
}tMemList;

//funciones
void createMemList(tMemList *list);
void clearMemList(tMemList *list);
int insertMallocBlock(tMemList *list, void *addr, size_t size);
int InsertarNodoShared(tMemList *list, void *addr, size_t size, key_t key);
int InsertarNodoMmap(tMemList *list, void *addr, size_t size, const char *filename, int fd, int perms);
int EliminarNodoDireccion(tMemList *list, void *addr);
MemNode* findMallocBlock(tMemList *list, size_t size);
void* DireccionNodoShared(tMemList *list, key_t key);
MemNode* findMappedBlock(tMemList *list, const char *filename);
void printMemList(const tMemList *list);
void printMallocList(const tMemList *list);
void printSharedList(const tMemList *list);
void printMappedList(const tMemList *list);
const char* permsToString(int perms);
MemNode* findBlockByAddr(tMemList *list, void *addr);
void * CadenaToPointer(char *cadena);
void LlenarMemoria(void *p, size_t cont, unsigned char byte);
#endif //P0_SO_MEMORY_LIST_H
