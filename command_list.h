#ifndef P0_COMMAND_LIST_H
#define P0_COMMAND_LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define MAX 256

typedef struct Node{
    char command[MAX];
    int commandNumber;
    struct Node *next;
} Node;

typedef struct{
    Node *head;
    int cont;
}tList;

//Funciones
void createList(tList *list);
void insertItem(tList *list, const char command[]);
void clearList(tList *list);
void PrintHistory(tList *list);
void PrintLastNCommands(tList *list, int n);
char* FindCommandByNumber(tList *list, int n);

#endif //P0_COMMAND_LIST_H