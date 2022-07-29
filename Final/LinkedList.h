#ifndef INC_344FINAL_LINKEDLIST_H
#define INC_344FINAL_LINKEDLIST_H

struct cityNode {
    char name[50];
    struct dateNode *entries;
    struct cityNode *next;
};

struct dateNode {
    char rev_date[9];
    char date[11];
    struct dateNode *next;
    struct transactionNode *firstTransaction;
};

struct transactionNode {
    int t_id;
    char type[30];
    char street_name[30];
    int square_meter;
    int price;
    struct transactionNode *next;
};

struct dateNode* insertFirstDateLL(struct dateNode *head, char *rev_date, char *date, struct transactionNode *firstTransaction);
struct dateNode* sortedInsertDateLL(struct dateNode* sorted, char *rev_date, char *date, struct transactionNode *firstTransaction);

struct transactionNode* insertFirstTransactionLL(struct transactionNode *head, int t_id, char *type, char *street_name, int square_meter, int price);
void reverseTransactionLL(struct transactionNode** head_ref);

#endif //INC_344FINAL_LINKEDLIST_H
