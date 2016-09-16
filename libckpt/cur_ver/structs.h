#ifndef __structs_h
#define __structs_h 1

#include "libckpt.h"

typedef enum {FALSE, TRUE} BOOL;

typedef struct {
  caddr_t STACKBOTTOM;
  caddr_t DATASTART;
  caddr_t TOPSTACK;
  caddr_t HEAPEND;
  caddr_t FILEAREA;
  unsigned long FILEAREASIZE;
} RESOURCES;

typedef struct {
  BOOL checkpoint;
  BOOL fork;
  BOOL verbose;
  BOOL incremental;
  BOOL exclude;
  BOOL enhanced_fork;
  unsigned long mintime;
  unsigned long maxtime;
  unsigned long maxfiles;
  char dir[256];
  char filename[256];
} CKPTFLAGS;

typedef struct {
  BOOL inuse;
  char * filename;
  int attribute;
  int mode;
  int fd;
  off_t seekptr;
} FILETABLEENTRY;

typedef struct {
  BOOL wait;
  BOOL enable;
  BOOL lang;

  char page_dirty[MAXPAGES];

  unsigned short mutex;
  unsigned short filenum;
  unsigned short ckpt_num;
  off_t df_seekptr;
  void* sbrkptr;

  Mlist inc_list;
  Mlist save_list;

  jmp_buf env;

  FILETABLEENTRY filetable[MAXFILES];
} CKPTGLOBALS;

#endif /*-- __structs_h --*/
