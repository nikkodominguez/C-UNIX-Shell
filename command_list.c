#include "command_list.h"

void createList(tList *list){
    list->head=NULL;
    list->cont=0;
}

void insertItem(tList *list, const char command[]){
    Node *newNode = (Node *)malloc(sizeof(Node));
    if(newNode == NULL){
        perror("(perror) Error al asignar memoria: ");
        return;
    }

    //Copiamos los datos del comando que nos pasan
    strncpy(newNode->command,command,MAX);
    newNode->command[MAX-1] = '\0';
    newNode->next=NULL;
    newNode->commandNumber = list->cont+1;

    if(list->head==NULL){
        list->head = newNode;
    }else{
        Node *current = list->head;
        while(current->next != NULL)
            current = current->next;
        current->next=newNode;
    }
    list->cont++;
}

void clearList(tList *list){
    Node *current = list->head;
    Node *nextNode;

    while(current != NULL){ //borramos la lista desde la cabeza hasta el ultimo elemento
        nextNode = current->next;
        free(current);
        current = nextNode;
    }

    //Una vez borrada la lista, reiniciamos los valores
    list->head = NULL;
    list->cont = 0;
}

void PrintHistory(tList *list) {
    Node *current = list->head;
    if (current == NULL) {
        printf("El historial de comandos está vacío.\n");
        return;
    }
    while (current != NULL) {
        printf("%d: %s\n", current->commandNumber, current->command);
        current = current->next;
    }
}

void PrintLastNCommands(tList *list, int n) {
    int start_pos;
    Node *current;

    if (n <= 0)
        return;

    start_pos = list->cont - n;
    if (start_pos < 0) {
        start_pos = 0; //Si piden más de los que hay, se muestran todos
    }

    current = list->head; //empieza en el primer elemento de la lista
    while (current != NULL) {
        if (current->commandNumber > start_pos) { //recorre desde el principio, sin imprimir nada hasta llegar al numero deseado
            printf("%d: %s\n", current->commandNumber, current->command);
        }
        current = current->next;
    }
}

char* FindCommandByNumber(tList *list, int n) {
    Node *current = list->head;
    while (current != NULL) {
        if (current->commandNumber == n) {
            return current->command; //Devuelve un puntero a la direccion de memoria del comando
        }
        current = current->next;
    }
    return NULL; //No se encontró el comando
}
