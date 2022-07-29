#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "LinkedList.h"

struct dateNode* insertFirstDateLL(struct dateNode *head, char *rev_date, char *date, struct transactionNode *firstTransaction) {
    //create a link
    struct dateNode *link = (struct dateNode*) calloc(1,sizeof(struct dateNode));

    strcpy(link->rev_date, rev_date);
    strcpy(link->date, date);
    link->firstTransaction = firstTransaction;

    //point it to old first node
    link->next = head;

    //point first to new first node
    head = link;

    return head;
}

struct dateNode* sortedInsertDateLL(struct dateNode* sorted, char *rev_date, char *date, struct transactionNode *firstTransaction) {
    struct dateNode *newnode = (struct dateNode*) calloc(1,sizeof(struct dateNode));

    strcpy(newnode->rev_date, rev_date);
    strcpy(newnode->date, date);
    newnode->firstTransaction = firstTransaction;

    /* Special case for the head end */
    if (sorted == NULL || strcmp(sorted->rev_date, newnode->rev_date) >= 0 ) {
        newnode->next = sorted;
        sorted = newnode;
    }
    else {
        struct dateNode* current = sorted;
        /* Locate the node before the point of insertion */
        while (current->next != NULL && strcmp(current->next->rev_date, newnode->rev_date) < 0) {
            current = current->next;
        }
        newnode->next = current->next;
        current->next = newnode;
    }
    return sorted;
}

struct transactionNode* insertFirstTransactionLL(struct transactionNode *head, int t_id, char *type, char *street_name, int square_meter, int price) {
    //create a link
    struct transactionNode *link = (struct transactionNode*) calloc(1,sizeof(struct transactionNode));

    link->t_id = t_id;
    strcpy(link->type, type);
    strcpy(link->street_name, street_name);
    link->square_meter = square_meter;
    link->price = price;

    //point it to old first node
    link->next = head;

    //point first to new first node
    head = link;

    return head;
}

void reverseTransactionLL(struct transactionNode** head_ref) {
    struct transactionNode* prev   = NULL;
    struct transactionNode* current = *head_ref;
    struct transactionNode* next;

    while (current != NULL) {
        next  = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }

    *head_ref = prev;
}