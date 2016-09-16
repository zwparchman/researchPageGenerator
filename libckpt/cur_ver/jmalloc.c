#include <stdio.h>
#include <stdint.h>
#include "libckpt.h"

extern void exit(int);

/* Each memory block has 8 extra 32-bit values associated with it.  If malloc
   returns the pointer p to you, the state really looks like:

jmal(p)------>  |------------------------------------------------------------|
                | flink (next malloc block in absolute order)                |
                |------------------------------------------------------------|
                | blink (prev malloc block in absolute order)                |
                |------------------------------------------------------------|
                | nextfree (next free malloc block - no particular order)    |
                |------------------------------------------------------------|
                | prevfree (next free malloc block - no particular order)    |
                |------------------------------------------------------------|
                | size (size of memory allocated)                            |
                |------------------------------------------------------------|
                | cs2 (pointer to the second checksum, which is right after  |
                |   the mem block)                                           |
                |------------------------------------------------------------|
                | cs (checksum right before mem block.  used to determine if |
                |   there is an error of writing around the memory block)    |
p------------>  |------------------------------------------------------------|
                | space: the memory block                                    |
                |                  ...                                       |
                |                  ...                                       |
                |------------------------------------------------------------|
	        | the second checksum                                        |
                |------------------------------------------------------------|
*/


typedef struct jmalloc {
  struct jmalloc *flink;
  struct jmalloc *blink;
  struct jmalloc *nextfree;
  struct jmalloc *prevfree;
  size_t size;
  int *cs2;
  int cs;
  //int dummy1; //commented out by zack since I'm assuming it is for padding
  char *space;
  //int dummy2; //commented out by zack since I'm assuming it is for padding
} *Jmalloc;

#define JMSZ (sizeof(struct jmalloc))
#define PTSZ (sizeof(double))  /* Also assuming its > sizeof int */
#define CHUNK_SIZE (16384 - JMSZ)	/* 16K */
#define MASK 0x17826a9b
#define JNULL ((Jmalloc) 0)

static struct jmalloc j_head;
static Jmalloc memlist;
static int nfree = 0;
static int nblocks = 0;
static int init = 0;
static Jmalloc start;
static int free_called = 0;
static int malloc_called = 0;
static int used_mem = 0;
static int free_mem = 0;
static int used_blocks = 0;
static int free_blocks = 0;

#define cksum(p) (((int32_t)(uintptr_t) &(p->cs)) - 1)
/* #define cksum(p) 10 */
#define jloc(l) ((char *) (&l->space))
#define jmal(l) ((Jmalloc) (((char *)l) - JMSZ + PTSZ))
#define isfree(l) (l->nextfree != JNULL)

#define do_init() \
  if (!init) {\
    memlist = &j_head;\
    memlist->flink = memlist;\
    memlist->blink = memlist;\
    memlist->nextfree = memlist;\
    memlist->prevfree = memlist;\
    memlist->size = 0;\
    memlist->cs = cksum(memlist);\
    memlist->cs2 = &memlist->cs;\
    memlist->space = (char *) 0;\
    start = memlist;\
    init = 1;\
  }

void dump_core()
{
  memlist->space[0] = 0;
}

char *set_used(Jmalloc l)
{
  start = l->nextfree;
  l->prevfree->nextfree = l->nextfree;
  l->nextfree->prevfree = l->prevfree;
  l->prevfree = JNULL;
  l->nextfree = JNULL;
  used_mem += l->size;
  free_mem -= l->size;
  used_blocks++;
  free_blocks--;

  return jloc(l);
}

void *malloc(size_t size)
{
  int redo;
  int done;
  Jmalloc l;
  char *tmp;
  Jmalloc newl;
  int newsize;
  
  do_init();
  malloc_called++;
  if (size <= 0) {
    fprintf(stderr, "Error: Malloc(%lu) called\n", size);
    /* Dump core */
    dump_core();
  }
    
  if (size % PTSZ != 0) size += PTSZ - (size % PTSZ);

  done = 0;
  l = start;
  while(!done) {
    if (l->size >= size) {
      done = 1;
      redo = 0;
    } else {
      l = l->nextfree;
      done = (l == start);
      redo = done;
    } 
  }
      
  if (redo) {
    if (size > CHUNK_SIZE) 
      newsize = size + JMSZ; 
      else newsize = CHUNK_SIZE + JMSZ;
    newl = (Jmalloc) ckptsbrk(newsize);
    while (newl == (Jmalloc) -1 && newsize > size + JMSZ) {
      newsize /= 2;
      if (newsize < size + JMSZ) newsize = size + JMSZ;
      newl = (Jmalloc) ckptsbrk(newsize);
    }
      
    if (newl == (Jmalloc) -1) {
      fprintf(stderr, "Jmalloc: out of memory\n");
      fprintf(stderr, "Used bytes = %d, Free bytes = %d\n", 
              used_mem, free_mem);
      fprintf(stderr, "Trying to get %lu bytes (chunk of %d)\n", 
              size, newsize);
      return NULL;
    }
    newl->flink = memlist;
    newl->blink = memlist->blink;
    newl->flink->blink = newl;
    newl->blink->flink = newl;
    newl->nextfree = memlist;
    newl->prevfree = memlist->prevfree;
    newl->nextfree->prevfree = newl;
    newl->prevfree->nextfree = newl;
    newl->size = ((char *) ckptsbrk(0)) - jloc(newl) - PTSZ;
    free_mem += newl->size;
    newl->cs = cksum(newl);
    newl->cs2 = ((int *) (jloc(newl) + newl->size));
    *(newl->cs2) = cksum(newl);
    if(newl->size < size) {
      fprintf(stderr, "Newl->size(%lu) < size(%lu)\n", newl->size, size);
      exit(1);
    }
    free_blocks++;
    l = newl;
  } 

  if (l->size - size < JMSZ) {
    return set_used(l);
  } else {
    tmp = jloc(l);
    newl = (Jmalloc) (tmp + size + PTSZ);
    newl->flink = l->flink;
    newl->blink = l;
    newl->flink->blink = newl;
    newl->blink->flink = newl;
    newl->nextfree = l->nextfree;
    newl->prevfree = l;
    newl->nextfree->prevfree = newl;
    newl->prevfree->nextfree = newl;
    newl->size = l->size - size - JMSZ;
    newl->cs = cksum(newl);
    newl->cs2 = (int *) (jloc(newl) + newl->size);
    *(newl->cs2) = cksum(newl);
    free_mem += size + newl->size - l->size;
    free_blocks++;
    l->size = size;
    l->cs2 = ((int *) (jloc(l) + l->size));
    *(l->cs2) = cksum(l);
    return set_used(l);
  }
}

jmalloc_print_mem()
{
  Jmalloc l;
  int done;
  char *bufs[100];
  int sizes[100];
  int mc;
  int fc;
  int i, j;

  do_init();
  mc = malloc_called;
  fc = free_called;
  if (jmal(jloc(memlist)) != memlist) {
    fprintf(stderr, "TROUBLE: memlist=0x%p, jmal(jloc(memlist))=0x%p)\n",
            memlist, jmal(jloc(memlist)));
    exit(1);
  }
  done = 0;
  l = start;
  i = 0;
  while (!done) {
    if (cksum(l) != l->cs) {
      printf("Memory location 0x%p corrupted\n", jloc(l));
      exit(1);
    } else if (cksum(l) != *(l->cs2)) {
      printf("Memory location 0x%p corrupted\n", jloc(l));
      exit(1);
    }
    
    bufs[i] = jloc(l);
    sizes[i] = l->size;
    if (l->nextfree == 0) sizes[i] = -sizes[i];
    i++;
    l = l->flink;
    done = ((l == start) || i >= 100);
  }
  printf("Malloc called %d times\n", mc);
  printf("Free called %d times\n", fc);
  for (j = 0; j < i; j++) {
    printf("Loc = 0x%p, size = %d, free = %d\n", bufs[j], 
           (sizes[j] > 0) ? sizes[j] : -sizes[j], (sizes[j] >= 0));
  }
}

void jmalloc_check_mem()
{
  Jmalloc l;
  int done;

  do_init();
  done = 0;

  l = start;

  while (!done) {
    if (cksum(l) != l->cs) {
      fprintf(stderr, "Memory chunk violated: 0x%p: %s 0x%x.  %s 0x%i\n", 
              jloc(l), "Checksum 1 is ", l->cs, "It should be", cksum(l));
      dump_core();
    } else if (cksum(l) != *(l->cs2)) {
      fprintf(stderr, "Memory chunk violated: 0x%p: %s 0x%x.  %s 0x%i\n", 
              jloc(l), "Checksum 2 is ", *(l->cs2), "It should be", cksum(l));
      dump_core();
    }
    if (l->size != 0 &&  ((char *) l->cs2) != jloc(l) + l->size) {
      fprintf(stderr, "Memory chunk violated: 0x%p:\n", jloc(l));
      fprintf(stderr, "  Second checksum pointer is at 0x%p, size is %lu:\n",
                         l->cs2, l->size);
      fprintf(stderr, "  Second checksum pointer should be at 0x%p\n",
                         jloc(l) + l->size);
      dump_core();
    }

    l = l->flink;
    done = (l == start);
  }
}

  
void free(void *loc)
{
  Jmalloc l;
  Jmalloc pl, nl;

  do_init();
  free_called++;
  l = jmal(loc); 

  if (isfree(l)) {
    fprintf(stderr, "Error on free: memory chunk 0x%p already freed\n", loc);
    dump_core();
  }

  if (cksum(l) != l->cs) {
    fprintf(stderr, "Error on free: memory chunk violated: 0x%p\n", loc);
    dump_core();
  } else if (cksum(l) != *(l->cs2)) {
    fprintf(stderr, "Error on free: memory chunk violated: 0x%p\n", loc);
    dump_core();
  }

  used_mem -= l->size;
  free_mem += l->size;
  free_blocks++;
  used_blocks--;

  pl = l->blink;
  nl = l->flink;
  if (isfree(pl) && (jloc(pl)+pl->size + PTSZ == (char *) l)) {
    free_mem += JMSZ;
    pl->size += l->size + JMSZ;
    pl->flink = nl;
    pl->flink->blink = pl;
    pl->cs2 = l->cs2;
    *(pl->cs2) = cksum(pl);
    l = pl;
    free_blocks--;
  } else {
    l->prevfree = start;
    l->nextfree = start->nextfree;
    l->nextfree->prevfree = l;
    l->prevfree->nextfree = l;
  }

  if (isfree(nl) && jloc(l)+l->size + PTSZ == (char *) nl) {
    free_mem += JMSZ;
    l->size += nl->size + JMSZ;
    l->flink = nl->flink;
    l->flink->blink = l;
    l->cs2 = nl->cs2;
    *(l->cs2) = cksum(l);
    free_blocks--;
    nl->nextfree->prevfree = nl->prevfree;
    nl->prevfree->nextfree = nl->nextfree;
  }
  start = l;
}

void *realloc(void * loc, size_t size)
{
  Jmalloc l;
  Jmalloc l2, nl;
  char *loc2;
  int i;
  Jmalloc newl;


  do_init();

  if (size <= 0) {
    fprintf(stderr, "Error: Malloc(%lu) called\n", size);
    /* Dump core */
    dump_core();
  }
    
  if (size % PTSZ != 0) size += PTSZ - (size % PTSZ);

  l = jmal(loc); 

  if (cksum(l) != l->cs) {
    fprintf(stderr, "Error on realloc: memory chunk violated: 0x%p\n", loc);
    dump_core();
  } else if (cksum(l) != *(l->cs2)) {
    fprintf(stderr, "Error on realloc: memory chunk violated: 0x%p\n", loc);
    dump_core();
  }

  if (size < l->size) {
    if (l->size - size < JMSZ + 4) return loc;
    newl = (Jmalloc) (loc + size + PTSZ);
    newl->flink = l->flink;
    newl->blink = l;
    newl->flink->blink = newl;
    newl->blink->flink = newl;
    newl->nextfree = start->nextfree;
    newl->prevfree = start;
    newl->nextfree->prevfree = newl;
    newl->prevfree->nextfree = newl;
    newl->size = l->size - size - JMSZ;
    newl->cs = cksum(newl);
    newl->cs2 = (int *) (jloc(newl) + newl->size);
    *(newl->cs2) = cksum(newl);
    used_mem += size - l->size;
    free_mem += newl->size;
    free_blocks++;
    l->size = size;
    l->cs2 = ((int *) (jloc(l) + l->size));
    *(l->cs2) = cksum(l);
    start = newl;
    return loc;
  }


  nl = l->flink;

  if (isfree(nl) && (jloc(l)+l->size + PTSZ == (char *) nl) &&
      l->size + JMSZ + nl->size >= size) {
    start = nl;
    i = size - l->size - JMSZ;
    if (i < 0) i = 4;
    loc2 = malloc(i);
    l2 = jmal(loc2);
    if (l2 != nl) {
      fprintf(stderr, "Realloc internal error: l2 != nl\n");
      dump_core();
    }

    nl->flink->blink = nl->blink;
    nl->blink->flink = nl->flink;
    free_mem -= nl->size;
    used_mem += nl->size + JMSZ;
    free_blocks--;
    l->size += nl->size + JMSZ;
    l->cs2 = ((int *) (jloc(l) + l->size));
    *(l->cs2) = cksum(l);
    return loc;
  } else {
    loc2 = (char*)malloc(size);
    memcpy(loc2, loc, l->size);
    free(loc);
    return loc2;
  }
}

void *calloc(size_t nelem, size_t elsize)
{
  int *iptr;
  char *ptr;
  int sz;
  int i;

  sz = nelem*elsize;
  ptr = (char*)malloc(sz);
  iptr = (int *) ptr;
  
  for (i = 0; (size_t)i < sz/sizeof(int); i++) iptr[i] = 0;
  for (i = i * sizeof(int); i < sz; i++) ptr[i] = 0;
  return ptr;
}

int mallopt(int cmd, int value)
{
  fprintf(stderr, "Mallopt is not defined...\n");
  exit(1);
}


void jmalloc_usage()
{
  fprintf(stderr, "Jmalloc: %d %s %d block%s. %d %s %d block%s\n",
                    used_mem, "bytes used in", used_blocks, 
                    (used_blocks == 1) ? "" : "s", 
                    free_mem, "bytes free in", free_blocks,
                    (free_blocks == 1) ? "" : "s");
}
