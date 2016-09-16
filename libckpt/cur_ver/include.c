#include "libckpt.h"

/**************************************************************
 *  function:        include_bytes()
 *  arguments:       addr -- The address of the chunk to include
 *                   size -- The size of the chunk to include
 *  returns:         0 on success, -1 on failure
 *  local variables: result -- holds return value
 *  side effects:    adds the indicated chunk to _ckpt_inc
 *  calls:           include()
 *  called from:     include_bytes_()
 *                   user's code
 *************************************************************/

int include_bytes_(char *addr, unsigned long * sizep)
{
  include_bytes(addr, *sizep);
}

int include_bytes(char *addr, unsigned long size)
{
  int result;

  if ( ! ckptflags.checkpoint ) {
    errno = ENOCKPT;
    return - 1;
  }

  if ( (long)addr < (long)sys.DATASTART || (uintptr_t)addr + size > (uintptr_t)sbrk(0) ) {
    errno = EFAULT;
    return -1;
  }

  if(ckptflags.verbose){
    fprintf(stderr, "CKPT: Including %lu bytes from 0x%p to 0x%p\n",
                                                       size, addr, addr+size);
  }

  CKPT_MUTEX_GET
  result = include(ckptglobals.inc_list, addr, size);
  CKPT_MUTEX_REL

  return result;
}


/**************************************************************
 *  function:        include()
 *  arguments:       list -- the list to add to (_ckpt_inc or _ckpt_sav)
 *                   addr -- the address of the chunk to include
 *                   size -- the size of the chunk to include
 *  returns:         0 on success, -1 on flure
 *  local variables: list_ptr -- a pointer into the list
 *                   stop -- the address of the end of the chunk
 *  side effects:    adds the indicated chunk to the list
 *  calls:           ml_find()
 *                   coaalesce()
 *                   ml_add()
 *  called from:     include_bytes()
 *                   exclude_bytess()
 *************************************************************/

static int include(Mlist list, char * addr, unsigned long size)
{
  Dlist list_ptr;
  char *stop;

  stop = (char *)((long)addr + size);

  list_ptr = ml_find(list, addr);

  if ( list_ptr -> stop >= addr ) {
     list_ptr -> stop = max(list_ptr -> stop, stop);
     list_ptr -> size = list_ptr -> stop - list_ptr -> addr;
     coalesce( list, list_ptr );
  } else {
     ml_add(list, list_ptr, addr, size);
     list_ptr = list_ptr -> flink;
     coalesce( list, list_ptr);
  }
  return 0;
}


/**************************************************************
 *  function:        coalesce()
 *  arguments:       list -- one of _ckpt_save or _ckpt_inc
 *                   list_ptr -- a pointer into the list
 *  returns:         void
 *  local variables: none
 *  side effects:    "coalesces" the list pointed to by list,
 *                   beginning at list_ptr
 *  calls:           ml_delete()
 *  called from:     include()
 *************************************************************/

static void coalesce(Mlist list, Dlist list_ptr)
{

  while ( list_ptr -> flink -> stop <= list_ptr -> stop) {
    ml_delete(list, list_ptr -> flink);
  }

  if ( list_ptr -> flink -> addr <= list_ptr -> stop ) {
    list_ptr -> stop = list_ptr -> flink -> stop;
    list_ptr -> size = list_ptr -> stop - list_ptr -> addr;
    ml_delete(list, list_ptr -> flink);
  }

}

/**************************************************************
 *  function:        exclude_bytes()
 *  arguments:       addr      -- The address of the chunk to exclude
 *                   size      -- The size of the chunk to exclude
 *                   start_now -- If TRUE, chunk is removed from  _ckpt_inc,
 *                                if FALSE, chunk is added to _ckpt_sav.
 *  returns:         0 on success, -1 on failure.
 *  local variables: result -- holds return value
 *  side effects:    adds to _ckpt_sav or removes from _ckpt_inc depending
 *                   on the value of the start_now parameter.
 *  calls:           exclude()
 *                   include()
 *  called from:     exclude_bytes_()
 *                   user code
 *************************************************************/

int exclude_bytes_(char * addr, unsigned long * sizep, int * start_nowp)
{
  exclude_bytes(addr, *sizep, *start_nowp);
}

int exclude_bytes(char *addr, unsigned long size, int start_now)
{

  int result;

  if ( !ckptflags.checkpoint ) {
    errno = ENOCKPT;
    return -1;
  }
 
  if (!ckptflags.exclude) return -1;

  if ( (long)addr < (long)sys.DATASTART || (uintptr_t)addr + size > (uintptr_t)ckptglobals.sbrkptr ) {
    errno = EFAULT;
    return -1;
  }

  CKPT_MUTEX_GET

  if ( start_now ){
    if(ckptflags.verbose){
      fprintf(stderr, "CKPT: Excluding %lu bytes from 0x%p to 0x%p (DEAD)\n",
                                                       size, addr, addr+size);
    }
    result = exclude(ckptglobals.inc_list, addr, size);
  }
  else{
    if(ckptflags.verbose){
      fprintf(stderr, "CKPT: Excluding %lu bytes from 0x%p to 0x%p (READ ONLY)\n",
                                                       size, addr, addr+size);
    }
    result = include(ckptglobals.save_list, addr, size);
  }

  CKPT_MUTEX_REL

  return result;

}

/*
 *  function:        exclude()
 *  arguments:       list -- the list (_ckpt_inc or _ckpt_sav) to modify
 *                   addr -- the address of the chunk to exclude
 *                   size -- the size of the chunk to exclude
 *  returns:         0 on success, -1 on failure
 *  local variables: list_ptr  -- a pointer into the list
 *                   stop      -- the address of the "end" of the chunk
 *                   temp_addr -- used to call include() if the break has moved.
 *  side effects:    "excludes" a chunk of memory by possibly removing info
 *                   from the list passed in as a first argument
 *  calls:           include()
 *                   ml_find()
 *                   ml_delete()
 *                   ml_add()
 *                   exclude()
 *  called from:     exclude() -- recursive
 *                   exclude_bytes()
 */

int exclude(Mlist list, char * addr, unsigned long size)
{

  Dlist list_ptr;
  char *stop;

  stop = (char *)((long)addr + size);

  list_ptr = ml_find(list, addr);

  if ( addr == list_ptr -> addr ) { /* case 2.1 */
    if ( stop < list_ptr -> stop ) { /* case 2.1.1 */
      list_ptr -> addr = stop;
      list_ptr -> size = list_ptr -> stop - list_ptr -> addr;
    } else if ( stop == list_ptr -> stop )  { /* case 2.1.2 */
      ml_delete(list, list_ptr);
    } else { /* case 2.1.3 */
      ml_delete(list, list_ptr);
      exclude(list, addr, size);
    }
  } else if ( addr < list_ptr -> stop) { /* case 2.2 */
    if ( stop < list_ptr -> stop ) { /* case 2.2.1 */
      ml_add(list, list_ptr, stop, list_ptr -> stop - stop);
      list_ptr -> stop = addr;
      list_ptr -> size = list_ptr -> stop - list_ptr -> addr;
    }
    else if ( stop == list_ptr -> stop ) { /* case 2.2.2 */
      list_ptr -> stop = addr;
      list_ptr -> size = list_ptr -> stop - list_ptr -> addr;
    }
    else { /* case 2.2.3 */
      list_ptr -> stop = addr;
      list_ptr -> size = list_ptr -> stop - list_ptr -> addr;
      exclude(list, addr, size);
    }
  }
  else { /* case 2.3 */
    addr = list_ptr -> flink -> addr;
    if ( addr < stop ) {
      exclude(list, addr, stop - addr);
    }
  }

  return 0;
}
