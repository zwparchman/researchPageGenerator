/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Dorian Arnold
 * Institution: University of Tennessee, Knoxville
 * Date: July, 1998
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include "libckpt.h"
#include "copyright.h"
#include <stdint.h>

#define CHECK(x) if( (x) == -1){                   \
                   ckptflags.checkpoint = FALSE;   \
                   if(ckptflags.fork == TRUE){     \
                     _exit(1);                     \
                   }                               \
                   else{                           \
                     return -1;                    \
                   }                               \
                 }

extern end;

void exit(int);

/**************************************************************
 * function: checkpoint_here
 * args: none
 * returns: -1 on failure, and 1 on checkpoint, 0 on recovery
 * side effects: see inline comments
 * called from: user code or signal handler
 *************************************************************/

int checkpoint_here_(){
  checkpoint_here();
}

int checkpoint_here(){
  caddr_t start, stop;
  struct sigaction act;
  int ret_val;

  /*-- check if checkpoint should be taken --*/
  ret_val = check_flags();

  if(ret_val == -1){
    return -1;
  }

  ckptglobals.ckpt_num++;

  if(ckptflags.verbose){
    fprintf(stderr, "CKPT: Checkpoint %i beginnning at time %li\n",
                            ckptglobals.ckpt_num, (long)time(0));
  }

  /*-- cancel timeout --*/
  alarm(0);

  if(ckptflags.fork == TRUE){  /*- initialize for fork() -*/
    ckptglobals.wait = TRUE;
    sigemptyset(&act.sa_mask);
    act.sa_handler = child_handler;
    act.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &act, 0);
  }

  if(ckptflags.fork == FALSE){  /*-- parent takes ckpt --*/
    ret_val = take_ckpt();
  }
  else{  /*-- child takes ckpt --*/
    start = sys.DATASTART;
    stop = (caddr_t)sbrk(0);
    if(ckptflags.incremental)
      ret_val = mprotect(start, (size_t)(stop-start),
                            PROT_READ | PROT_WRITE | PROT_EXEC);
    if(ret_val == -1){
      fprintf(stderr, "CKPT: cannot set memory from 0x%p to 0x%p "
                      "to read/write\n", start, stop);
      perror("mprotect");
      ckptflags.checkpoint = FALSE;
      exit(-1);
    }

    ret_val = fork();
    if(ret_val == 0){    /*-- forked child --*/
      ret_val = take_ckpt();
    }
    else{
      ret_val = FALSE;
    }
  }
  CHECK(ret_val);
  CHECK(cleanup(ret_val));

  return ret_val;
}

/**************************************************************
 * function: check_flags()
 * args: none
 * returns: 0 on success, -1 on failure
 * side effects: checks to see if a checkpoint can be taken and
 *               sets errno appropriately
 * called from: checkpoint_here()
 *************************************************************/

static int check_flags(){
  int now = time(0);
  int rtime;

  if(ckptflags.checkpoint == FALSE){
    errno = ENOCKPT;
    if(ckptflags.verbose){
      fprintf(stderr, "CKPT: checkpointing has been disabled\n");
    }
    return -1;
  }
  if(ckptglobals.wait== TRUE){
    errno = ECHILD;
    if(ckptflags.verbose){
      fprintf(stderr, "CKPT: checkpoint currently in progress\n");
    }
    return -1;
  }
  if(ckptglobals.enable == FALSE){
    errno = ETOOSOON;
    if(ckptflags.verbose){
      fprintf(stderr, "CKPT: too soon between checkpoints\n");
    }
    return -1;
  }
  return 0;
}
    
/**************************************************************
 * function: take_ckpt()
 * args: none
 * returns: -1 on failure, 0 on checkpoint, 1 on recovery
 * side effects: saves the stack via setjmp
 *               writes the heap and stack to a ckpt file
 * called from: checkpoint_here()
 *************************************************************/
static int take_ckpt(){
  char tempdata[256];
  char tempmap[256];
  char filename[256];
  long no_chunks;
  int map_fd, data_fd;
  int ret_val;
  BOOL rflag = FALSE;

  if (!setjmp(ckptglobals.env)){
    CHECK(ckpt_sync());  /*-- dump all buffers for open files   --*/
    ckpt_seek();  /*-- save the seek ptrs for open files --*/

    sprintf(tempdata, "%s/%s.%s", ckptflags.dir, ckptflags.filename, "data_temp");
    remove(tempdata);
    data_fd = OPEN(tempdata, O_WRONLY | O_CREAT, 0644);
    CHECK(data_fd)
    ckptglobals.df_seekptr = 0;  /*-- reset data file seekptr --*/

    sprintf(tempmap, "%s/%s.%s", ckptflags.dir, ckptflags.filename, "map_temp");
    remove(tempmap);
    map_fd = OPEN(tempmap, O_WRONLY | O_CREAT, 0644);
    CHECK(map_fd)

    CHECK(write_header(map_fd))

    ret_val = write_heap(map_fd, data_fd);
    CHECK(ret_val);
    no_chunks = ret_val;

    ret_val = write_stack(map_fd, data_fd);
    CHECK(ret_val)
    no_chunks += ret_val;

    lseek(map_fd, POINTERSIZE*3, SEEK_SET);
    CHECK(write(map_fd, (char *)&no_chunks, LONGSIZE))
    debug(stderr, "DEBUG: no_chunks = %li\n", no_chunks);

    fsync(data_fd);
    CLOSE(data_fd);
    fsync(map_fd);
    CLOSE(map_fd);

    sprintf(filename, "%s/%s.%s.%d", ckptflags.dir, ckptflags.filename, "data",
                                  ckptglobals.filenum);
    rename(tempdata, filename);

    sprintf(filename, "%s/%s.%s.%d", ckptflags.dir, ckptflags.filename, "map",
                                  ckptglobals.filenum);
    rename(tempmap, filename);

    if(ckptflags.maxfiles > 1 && ckptglobals.filenum == ckptflags.maxfiles){
      ckpt_coalesce();
      ckptglobals.filenum = 0;
    }
      
    if(ckptflags.fork == TRUE){
      if(ckptflags.verbose == TRUE)
        fprintf(stderr, "CKPT: checkpoint complete, child exiting\n");
      _exit(0);
    }
  }
  else{         /*-- recovery complete --*/
    ckptglobals.wait = FALSE;
    rflag = TRUE;
    open_files();
  }
  return rflag;
}

/**************************************************************
 * function: ckpt_seek
 * arguments: none
 * returns: none
 * side effects: saves all seek ptrs for "inuse" files.
 * called from: take_ckpt()
 *************************************************************/
static void ckpt_seek()
{
  int fd;

  for(fd=0; fd < MAXFILES; fd++){
    if(ckptglobals.filetable[fd].inuse){
      ckptglobals.filetable[fd].seekptr = 
             lseek(ckptglobals.filetable[fd].fd, 0, SEEK_CUR);
    }
  }
}
  
/**************************************************************
 * function: ckpt_sync
 * arguments: none
 * returns: 0 on success, -1 otherwise
 * side effects: "syncs" the open files
 * called from: take_ckpt()
 *************************************************************/
static int ckpt_sync()
{
  int fd;
 
  for(fd=0; fd < MAXFILES; fd++){
    if(ckptglobals.filetable[fd].inuse && fsync(fd) == -1){
      return -1;
    }
  }

  return 0;
}

/**************************************************************
 * function: open_files()
 * arguments: none
 * returns: none
 * side effects: restores open file state by reopening files that were 
 *               open when checkpoint was taken and setting lseek ptr.
 * called from: take_ckpt (upon recovery)
 *************************************************************/
static void open_files()
{
  FILETABLEENTRY *f;
  int i, fd;

  for(i=0; i<MAXFILES; i++){
    f = &ckptglobals.filetable[i];
 
    if(f->inuse){
      /*-- The "anding" of the attribute field is to remove the O_TRUNC flag --*/
      fd = OPEN(f->filename, f->attribute & 0xfffffbff, f->mode);
      debug(stderr, "DEBUG: opening file %s\n", f->filename);
 
      if(fd == -1){
        fprintf(stderr, "CKPT: unable to re-open file %s during recovery \n",
                 f->filename);
        exit(-1);
      }

      if(fd != f->fd){
        if(dup2(fd, f->fd) == -1){
            fprintf(stderr, "CKPT: unable to duplicate filedes\n");
            perror("dup2");
            exit(-1);
          }
        CLOSE(fd);
      }
      lseek(f->fd, f->seekptr, SEEK_SET);
    }
  }
}

/**************************************************************
 * function: write_header
 * arguments: fd -- file descriptor for open ckpt "map file"
 * returns: 0 on success, -1 otherwise
 * side effects: writes top of stack and end of heap to "map file"
 * called from: take_ckpt();
 * misc.: uses TOPSTACK macro and address of dummy_first to figure
          out top of stack.
 *************************************************************/

static int write_header(int fd)
{
  caddr_t topstack, stopaddr;
  int no_chunks, no_files=1;
  int dummy_last;

  sys.TOPSTACK = (caddr_t)(&dummy_last - OFFSET);
  sys.TOPSTACK = (caddr_t)((unsigned long)sys.TOPSTACK - 
                               ((unsigned long)sys.TOPSTACK % PAGESIZE));
    
  stopaddr = ckptglobals.sbrkptr;

  if(write(fd, (char *)&sys.TOPSTACK, POINTERSIZE) != POINTERSIZE){
    ckptflags.checkpoint = FALSE;
    return -1;
  }

  if(write(fd, (char *)&sys.DATASTART, POINTERSIZE) != POINTERSIZE){
    ckptflags.checkpoint = FALSE;
    return -1;
  }

  if(write(fd, (char *)&stopaddr, POINTERSIZE) != POINTERSIZE){
    ckptflags.checkpoint = FALSE;
    return -1;
  }

  if(write(fd, (char *)&no_chunks, LONGSIZE) != LONGSIZE){
    ckptflags.checkpoint = FALSE;
    return -1;
  }

  debug(stderr, "DEBUG: topstack = 0x%p\n"
                "DEBUG: datastart = 0x%p\n"
                "DEBUG: stopaddr = 0x%p\n", 
                    sys.TOPSTACK, sys.DATASTART, stopaddr);

  return 0;
}

/**************************************************************
 * function: write_heap
 * args: map_fd  -- file descriptor for map file
 *       data_fd -- file descriptor for data file
 * returns: no. of chunks written on success, -1 on failure
 * side effects: writes all included segments of the heap to ckpt files
 * misc.: If we are forking and copyonwrite is set, we will write the
          heap from bottom to top, moving the brk pointer up each time
          so that we don't get a page copied if the
 * called from: take_ckpt()
 *************************************************************/
static int write_heap(int map_fd, int data_fd)
{
  Dlist curptr, endptr;
  int no_chunks=0, pn;
  long size;
  caddr_t stop, addr;

  if(ckptflags.incremental){       /*-- incremental checkpointing on? --*/
    endptr = ckptglobals.inc_list->main->flink;
 
    /*-- for each included chunk of the heap --*/
    for(curptr = ckptglobals.inc_list->main->blink->blink;
        curptr != endptr; curptr = curptr->blink){

      /*-- write out the last page in the included chunk --*/
      stop = curptr->addr;
      pn = ((long)curptr->stop - (long)sys.DATASTART) / PAGESIZE;

      if(isdirty(pn)){ 
        addr = (caddr_t)max((long)curptr->addr,
                            (long)((pn * PAGESIZE) + sys.DATASTART));
        size = (long)curptr->stop - (long)addr;

        debug(stderr, "DEBUG: Writing heap from 0x%p to 0x%p, pn = %d\n",
                                                   addr, addr+size, pn);
        if(write_chunk(addr, size, map_fd, data_fd) == -1){
          return -1;
        }

        if((uintptr_t)addr > (uintptr_t)(&end) && ckptflags.enhanced_fork){
          brk(addr);
        }
        no_chunks++;
      }

      /*-- write out all the whole pages in the middle of the chunk --*/
      for(pn--; pn * PAGESIZE + sys.DATASTART >= stop; pn--){
        if(isdirty(pn)){
          addr = (caddr_t)((pn * PAGESIZE) + sys.DATASTART);
          debug(stderr, "DEBUG: Writing heap from 0x%p to 0x%p, pn = %d\n",
                                                 addr, addr+PAGESIZE, pn);
          if(write_chunk(addr, PAGESIZE, map_fd, data_fd) == -1){
            return -1;
          }
          if((uintptr_t)addr > (uintptr_t)(&end) && ckptflags.enhanced_fork){
            brk(addr);
          }
          no_chunks++;
        }
      }

      /*-- write out the first page in the included chunk --*/
      addr = curptr->addr;
      size = ((pn+1) * PAGESIZE + sys.DATASTART) - addr;

      if(size > 0 && (isdirty(pn))){
        debug(stderr, "DEBUG: Writing heap from 0x%p to 0x%p\n", addr, addr+size);
        if(write_chunk(addr, size, map_fd, data_fd) == -1){
          return -1;
        }

        if((uintptr_t)addr > (uintptr_t)(&end) && ckptflags.enhanced_fork){
          brk(addr);
        }
        no_chunks++;
      }
    }
  }
  else{  /*-- incremental checkpointing off! --*/
    endptr = ckptglobals.inc_list->main->blink;

    /*-- for each included chunk of the heap --*/
    for(curptr = ckptglobals.inc_list->main->flink->flink;
        curptr != endptr; curptr = curptr->flink){
      debug(stderr, "DEBUG: saving memory from 0x%p to 0x%p\n", 
                         curptr->addr, curptr->addr+curptr->size);

      if(write_chunk(curptr->addr, curptr->size, map_fd, data_fd) == -1){
        return -1;
      }
      if((uintptr_t)addr > (uintptr_t)(&end) && ckptflags.enhanced_fork){
        brk(addr);
      }
      no_chunks++;
    }
  }

  return no_chunks;
}

/**************************************************************
 * function: write_stack
 * args: map_fd  -- file descriptor for map file
 *       data_fd -- file descriptor for data file
 * returns: no. of chunks written on success, -1 on failure
 * side effects: writes stack to ckpt files
 * called from: take_ckpt()
 *************************************************************/
static int write_stack(int map_fd, int data_fd)
{
  int no_chunks = 0;
  int dummy_last;

  if(sys.FILEAREA != 0){
    debug(stderr, "DEBUG: FILEAREA -- 0x%p to 0x%p\n",
                         sys.FILEAREA, sys.FILEAREA+sys.FILEAREASIZE);

    if(write_chunk(sys.FILEAREA, sys.FILEAREASIZE, map_fd, data_fd) == -1){
      return -1;
    }
    else{
      no_chunks++;
    }
  }

  debug(stderr, "DEBUG: STACK -- 0x%p to 0x%p\n", sys.TOPSTACK, sys.STACKBOTTOM);

  if(write_chunk((char *)sys.TOPSTACK, (long)sys.STACKBOTTOM - (long)sys.TOPSTACK,
                  map_fd, data_fd) == -1){
    return -1;
  }
  else
    no_chunks++;

  return no_chunks;
}

/**************************************************************
 * function: write_chunk
 * args: addr  -- address of chunk to be written to data file
 *       size  -- size of chunk to be written to data file
 *       pn  -- the corresponding number of the page in the heap
 *       seekptr -- pos. of chunk in data file
 *       map_fd  -- file descriptor for map file
 *       data_fd -- file descriptor for data file
 * returns: 0 on success, -1 otherwise
 * side effects: writes header to map file and data to data file
 * misc: if chunk is not from the heap, pn will be 0
 * called from: write_heap(), write_stack()
 *************************************************************/
static int write_chunk(char * addr, long size, int map_fd, int data_fd)
{
  if(write(map_fd, (char *)&addr, POINTERSIZE) != POINTERSIZE){
    ckptflags.checkpoint = FALSE;
    return -1;
  }
  
  if(write(map_fd, (char *)&size, LONGSIZE) != LONGSIZE){
    ckptflags.checkpoint = FALSE;
    return -1;
  }
  
  if(write(map_fd, (char *)&ckptglobals.df_seekptr, LONGSIZE) != LONGSIZE){
    ckptflags.checkpoint = FALSE;
    return -1;
  }
  if(write(data_fd, addr, size) != size){
    ckptflags.checkpoint = FALSE;
    return -1;
  }
  
  ckptglobals.df_seekptr += size;  /*-- update data file seekptr --*/
  return 0;
}

static int cleanup(int rflag){
  Dlist curptr, endptr;

  if(ckptflags.incremental == TRUE){
    set_incremental();
  }

  if(ckptflags.exclude == TRUE){
    curptr = ckptglobals.save_list->main->flink->flink;
    endptr = ckptglobals.save_list->main->blink;

    while(curptr != endptr){
      exclude_bytes(curptr->addr, curptr->size, CKPT_DEAD);
      curptr = curptr->flink;
      ml_delete(ckptglobals.save_list, curptr->blink);
    }
  }
    
  if(ckptflags.maxfiles == 1){
    ckptglobals.filenum = 0;
  }
  else if(ckptflags.maxfiles > 1 && ckptflags.maxfiles == ckptglobals.filenum || rflag){
    ckptglobals.filenum = 1;
  }
  else{
    ckptglobals.filenum++;
  }

  if(ckptflags.verbose){
    if(rflag == 1){
      fprintf(stderr, "CKPT: Recovery complete\n");
    }
    else if(ckptflags.fork == TRUE){
      fprintf(stderr, "CKPT: Child taking checkpoint; Parent resuming execution\n");
    }
    else{
      fprintf(stderr, "CKPT: Checkpoint complete, resuming execution\n");
    }
  }
  set_alarm();
}

static int isdirty(int pn){
  return (int)ckptglobals.page_dirty[pn];
}

/**************************************************************
 * function: ckpt_coalesce
 * args: none
 * returns: none
 * side effects: forks a child which executes the coalesce process
 *               The parent waits for the child before resuming execution
 * called from: take_ckpt() and ckpt_recover()
 ************************************************************/

void ckpt_coalesce(){
  int retval;
  char arg1[256], arg2[256], arg3[256], arg4[256];

  sprintf(arg1, "%p", sys.FILEAREA);
  sprintf(arg2, "%s/%s", ckptflags.dir, ckptflags.filename);
  sprintf(arg3, "%d", ckptflags.verbose);
  sprintf(arg4, "%d", DEBUG);

  if(!vfork()){
    execlp("coalesce", "coalesce", arg1, arg2, arg3, arg4, NULL);
    perror("coalesce");
    _exit(1);
  }
  else{
    wait(&retval);
  }
}
