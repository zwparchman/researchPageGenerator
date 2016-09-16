/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Dorian Arnold
 * Institution: University of Tennessee, Knoxville
 * Date: July, 1998
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include "libckpt.h"
#include "copyright.h"

/**************************************************************
 * function: ckpt_recover()
 * args: none
 * returns: none
 * side effects: recovers the heap and stack from most recent
 *               checkpoint files and then resumes execution at
 *               last instruction at time of checkpoint. If more
 *               than one set of checkpoint files exist, it 
 *               coalesces them into a single checkpoint prior to
 *               recovery.
 * called from: check_for_recovery() and fmain.f
 ************************************************************/

void ckpt_recover_(){
  ckpt_recover();
}

void ckpt_recover()
{
  int *retval;

  ckpt_coalesce();
  recover_heap();
  recover_stack();
}

/**************************************************************
 * function: recover_heap()
 * args: none
 * returns: none
 * side effects: recovers the heap from a checkpoint file
 * called from: ckpt_recover()
 ************************************************************/

void recover_heap()
{
  int data_fd, map_fd, i, j;
  caddr_t addr, topstack, stop_addr, datastart;
  char datafile[256];
  char mapfile[256];
  int fn = 0;
  long no_chunks, size, seekptr;
  struct sigaction act;
  int page1; char buf[8192];

  debug(stderr, "DEBUG: Recovery beginning\n");

  sprintf(datafile, "%s/%s.%s.0", ckptflags.dir, ckptflags.filename, "data");
  data_fd = OPEN(datafile, O_RDONLY, 0644);

  if(data_fd == -1){
    fprintf(stderr, "RECOVER: Checkpoint file %s cannot be opened.\n"
                    "RECOVER: Recovery aborted\n", datafile);
    exit(-1);
  }

  sprintf(mapfile, "%s/%s.%s.0", ckptflags.dir, ckptflags.filename, "map");
  map_fd = OPEN(mapfile, O_RDONLY, 0644);

  if(map_fd == -1){
    fprintf(stderr, "RECOVER: Checkpoint file %s cannot be opened.\n"
                    "RECOVER: Recovery aborted\n", mapfile);
    exit(-1);
  }

  read(map_fd, (char*)&topstack, POINTERSIZE);
  read(map_fd, (char*)&datastart, POINTERSIZE);
  read(map_fd, (char*)&stop_addr, POINTERSIZE);
  brk(stop_addr);

  debug(stderr, "DEBUG: topstack = 0x%p\n"
                "DEBUG: datastart = 0x%p\n"
                "DEBUG: stopaddr = 0x%p\n", topstack, datastart, stop_addr);

  read(map_fd, (char*)&no_chunks, LONGSIZE);
  debug(stderr, "DEBUG: no_chunks = %lu\n", no_chunks);

  no_chunks--;  /*-- exclude the stack chunk --*/

#if 0
  printf("");  /*-- Needed to prevent printf after heap recovery from failure? --*/
#else 
  fflush(stdout);
#endif

  for(j=0; j < no_chunks; j++){
    read(map_fd, (char *)&addr, POINTERSIZE); 
    read(map_fd, (char *)&size, LONGSIZE); 
    read(map_fd, (char *)&seekptr, LONGSIZE); 

    lseek(data_fd, seekptr, SEEK_SET);

    read(data_fd, (char *)addr, size); 

    debug(stderr, "DEBUG: recovering from 0x%p to 0x%p -- offset = %lu\n",
            addr, addr+size, seekptr);
  }

  ckptglobals.df_seekptr = lseek(map_fd, 0, SEEK_CUR);

  CLOSE(data_fd);
  CLOSE(map_fd);
}

/**************************************************************
 * function: recover_stack()
 * args: none
 * returns: none
 * side effects: calls restore_memory to retrieve the stack.
 * called from: ckpt_recover()
 ************************************************************/

void recover_stack(){
  char mapfile[256];
  int map_fd;
  caddr_t topstack;

  sprintf(mapfile, "%s/%s.%s.0", ckptflags.dir, ckptflags.filename, "map");
  map_fd = OPEN(mapfile, O_RDONLY, 0644);

  read(map_fd, (char *)&topstack, POINTERSIZE);

  CLOSE(map_fd);

  restore_memory(topstack);
}

/**************************************************************
 * function: restore_memory()
 * args: topstack -- address of the top of the current stack
 * returns: does not return
 * side effects: recursively calls itself until the size of the 
 *               stack is greater than or equal to its size when
 *               the checkpoint was taken. It then retrieves the
 *               stack from the checkpoint file and resumes 
 *               execution at point of checkpoint via longjmp()
 * called from: recover_stack()
 ************************************************************/

void restore_memory(caddr_t topstack){
  int dummy;
  char * addr;
  long size, seekptr; 
  int map_fd, data_fd;
  char datafile[256], mapfile[256];
  caddr_t newtopstack;

  newtopstack = (caddr_t)&dummy;

  if(newtopstack >= topstack)
    restore_memory(topstack);

  sprintf(mapfile, "%s/%s.%s.0", ckptflags.dir, ckptflags.filename, "map");
  map_fd = OPEN(mapfile, O_RDONLY, 0644);
 
  sprintf(datafile, "%s/%s.%s.0", ckptflags.dir, ckptflags.filename, "data");
  data_fd = OPEN(datafile, O_RDONLY, 0644);
 
  lseek(map_fd, ckptglobals.df_seekptr, SEEK_SET);
  read(map_fd, (char *)&addr, POINTERSIZE);
  read(map_fd, (char *)&size, POINTERSIZE);
  read(map_fd, (char *)&seekptr, POINTERSIZE);

  lseek(data_fd, seekptr, SEEK_SET);
  if(size != read(data_fd, (char *)addr, size)){
    fprintf(stderr, "Data file corrupted\n"); exit(1);
  }
  debug(stderr, "DEBUG: Stack -- 0x%p to 0x%p -- offset = %lu\n",
                                              addr, addr+size, seekptr);

  CLOSE(map_fd);
  CLOSE(data_fd);

  longjmp(ckptglobals.env, 1);
}
