#ifndef __externs_h
#define __externs_h 1

#include "libckpt.h"

	/*-- setup.c externs and prototypes --*/

extern int DEBUG;
extern RESOURCES sys;            /*--- sys contains the global state of 
                                       the systems resources ---*/
extern CKPTFLAGS ckptflags;      /*--- contains the global ckpt flags ---*/
extern CKPTGLOBALS ckptglobals;  /*--- contains the ckpt globals ---*/

void ckpt_setup(int *, char **);
void mark_dirty(caddr_t);
void set_alarm();
void set_defaults();
void read_dotfile();
void set_incremental();

static void force_dirty();
static void configure();
static void check_for_recovery(int *, char **);
static void check_files();
static void set_globals();
static void print_flags();
static int check_and_set(char *, char *);
static int isnum(char *);

	/*-- signal_handler.c prototypes --*/
void config_pgfaulthandler(/*long, long, struct sigcontext *, char * */);
void pg_faulthandler(/*long, long, struct sigcontext *, char * */);
void alrm_handler();
void child_handler();

	/*-- checkpoint.c prototypes --*/
int checkpoint_here();

static void open_files();
static int check_flags();
static int take_ckpt();
static void ckpt_seek();
static int ckpt_sync();
static int write_header(int);
static int write_stack(int, int);
static int write_heap(int, int);
static int write_chunk(char *, long, int, int);
static int cleanup(int);
static int isdirty(int);

	/*-- include.c prototypes --*/
int include_bytes(char *, unsigned long);
int exclude_bytes(char *, unsigned long, int);
int exclude(Mlist, char *, unsigned long);

static int include(Mlist, char *, unsigned long);
static void coalesce(Mlist, Dlist);

	/*-- recover.c prototypes --*/
void ckpt_recover();
static void recover_heap();
static void recover_stack();
static void restore_memory(caddr_t);

    /*-- coalesce.c proototypes --*/
void ckpt_coalesce();
static void coa_write_chunk(char *, long, int, off_t);
static void flush_buffers();

    /*-- system.c prototypes --*/
void set_fcnpointers();
void file_insert(char *, int, int, int);
void file_delete(int);
void * ckptsbrk(int);

#endif
