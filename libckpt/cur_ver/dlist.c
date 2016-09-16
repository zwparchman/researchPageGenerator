#include <malloc.h>
#include "dlist.h"

Dlist make_dl()
{
  Dlist d;
 
  d = (Dlist) malloc (sizeof(struct dlist));
  d->flink = d;
  d->blink = d;
  return d;
}

dl_insert_b(node)   /* Inserts a new node before the given one */
Dlist node;
{
  Dlist last_node, new;
 
  new = (Dlist) malloc (sizeof(struct dlist));
 
  last_node = node->blink;
 
  node->blink = new;
  last_node->flink = new;
  new->blink = last_node;
  new->flink = node;
}

dl_delete_node(item)        /* Deletes an arbitrary iterm */
Dlist item;
{
  item->flink->blink = item->blink;
  item->blink->flink = item->flink;
  free(item);
}

Mlist make_ml()
{
  Mlist m;
  Dlist d;
 
  m = (Mlist) malloc(sizeof(struct mlist));
  m->main = make_dl();
  dl_insert_b(m->main);
  dl_insert_b(m->main);
  d = m->main->flink;
  d->addr = (char *) 0;
  d->stop = (char *) 0;
  d->size = 0;
  d = d->flink;

  d->addr = (char *) 0xffffffff;
  d->stop = (char *) 0xffffffff;

  d->size = 0;
 
  m->ptr = m->main->flink;
  return m;
}

ml_free_list(m)
Mlist m;
{
  Dlist d;
  d = m->main->flink;
  while (d != m->main) {
    d = d->flink;
    free(d->blink);
  }
  free(m->main);
  free(m);
}

ml_add(m, d, addr, size)
Mlist m;
Dlist d;
char *addr;
unsigned long size;
{
  d = d->flink;
  dl_insert_b(d);
  d = d->blink;
  d->addr = addr;
  d->size = size;
  d->stop = d->addr + d->size;
  m->ptr = d;
}
 
ml_delete(m, d)
Mlist m;
Dlist d;
{
  if (m->ptr == d) m->ptr = d->blink;
  dl_delete_node(d);
  if (m->ptr == m->main) m->ptr = m->main->flink;
}
 
Dlist ml_find(m, addr)
Mlist m;
char *addr;
{
  Dlist d;
 
  d = m->ptr;
  if (d->addr == addr) return d;
  while (d->addr <= addr) {
    d = d->flink;
    if (d->addr > addr) {
      m->ptr = d->blink;
      return m->ptr;
    }
  }
  while (d->addr > addr) {
    d = d->blink;
    if (d->addr <= addr) {
      m->ptr = d;
      return d;
    }
  }
}
