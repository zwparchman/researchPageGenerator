#ifndef DLIST_H
#define DLIST_H 1

typedef struct dlist {
  struct dlist *flink;
  struct dlist *blink;
  char *addr;
  char *stop;
  unsigned long size;
} *Dlist;

typedef struct mlist {
  Dlist main;
  Dlist ptr;
} *Mlist;

extern Mlist make_ml();

extern Dlist ml_find();
extern ml_delete();
extern ml_add();

/* These are the routines for manipluating lists */

extern Dlist make_dl();
extern dl_insert_b(/* node, val */); /* Makes a new node, and inserts it before
                                        the given node -- if that node is the 
                                        head of the list, the new node is 
                                        inserted at the end of the list */
#define dl_insert_a(n, val) dl_insert_b(n->flink, val)

extern dl_delete_node(/* node */);    /* Deletes and free's a node */

extern void *dl_val(/* node */);   /* Returns node->val (used to shut lint up) */

#define dl_traverse(ptr, list) \
  for (ptr = first(list); ptr != nil(list); ptr = next(ptr))
#define dl_empty(list) (list->flink == list)

#endif /* DLIST_H */
