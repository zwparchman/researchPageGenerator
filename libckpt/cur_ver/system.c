/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Dorian Arnold
 * Institution: University of Tennessee, Knoxville
 * Date: July, 1998
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include <unistd.h>
#define __stdio_c 1
#include "libckpt.h"
#include "copyright.h"
#include <stdlib.h>

/**************************************************************
 * function: open and _open
 * args: filename -- name of file to be opened
 *       attribute -- describes reason for opening file
 *       mode -- permissions for file to be opened.
 * returns: file descriptor for new file
 * side effects: calls the system call open to attempt to open
 *               file and if successful, save some info about
 *               that file via file_insert()
 * called from: users code 
 ************************************************************/

int open(char * filename, int attribute, int mode)
{
  int fd;

  if(filename)
    debug(stderr, "DEBUG: opening file %s\n", filename);

  fd = OPEN(filename, attribute, mode);

  if(ckptflags.checkpoint && fd != -1){
    file_insert(filename, attribute, mode, fd);
  }

  return fd;
}

int _open(char * filename, int attribute, int mode)
{
  int fd;
 
  if(filename)
    debug(stderr, "DEBUG: opening file %s\n", filename);

  fd = OPEN(filename, attribute, mode);
 
  if(ckptflags.checkpoint && fd != -1){
    file_insert(filename, attribute, mode, fd);
  }
 
  return fd;
}

/**************************************************************
 * function: close and _close
 * args: fd -- file descriptor of file to be closed
 * returns: 0 on success, -1 otherwise
 * side effects: calls the system call close to attempt to close
 *               file and if successful, deletes some info about
 *               that file via file_delete()
 * called from: users code 
 ************************************************************/
int close(int fd)
{
  int result;

  if(ckptglobals.filetable[fd].inuse)
    debug(stderr, "DEBUG: closing file %s\n", ckptglobals.filetable[fd].filename);

  result = CLOSE(fd);

  if(ckptflags.checkpoint && result != -1){
    file_delete(fd);
  }

  return result;
}

int _close(int fd)
{
  int result;
 
  if(ckptglobals.filetable[fd].inuse)
    debug(stderr, "DEBUG: closing file %s\n", ckptglobals.filetable[fd].filename);
 
  result = CLOSE(fd);
 
  if(ckptflags.checkpoint && result != -1){
    file_delete(fd);
  }
 
  return result;
}
 
/**************************************************************
 * function: file_insert()
 * args: filename -- name of file to insert
 *       attribute -- attributes to open file with
 *       mode  -- permissions to open file with
 *       fd  -- file descriptor returned by open syscall
 * returns: none
 * side effects: stores all this info pertaining to the file so
 *               it can be used to restore file table status
 *               upon recovery.
 * called from: open and _open
 ************************************************************/

void file_insert(char * filename, int attribute, int mode, int fd)
{
  FILETABLEENTRY *newfile;

  if(ckptglobals.filetable[fd].inuse == FALSE){
    newfile = &(ckptglobals.filetable[fd]);
    newfile->filename = strdup(filename);
    newfile->attribute = attribute;
    newfile->mode = mode;
    newfile->fd = fd;
    newfile->inuse = TRUE;
  }
  else{
    debug(stderr, "DEBUG: WARNING fd %d already in use\n", fd);
  }
}

/**************************************************************
 * function: file_delete()
 * args: fd -- file descriptor to delete
 * returns: none
 * side effects: removes file information from data base to mark
 *               that the file descriptor is no longer in use.
 * called from: close and _close
 ************************************************************/

void file_delete(int fd)
{
  FILETABLEENTRY *file;

  if(ckptglobals.filetable[fd].inuse){
    file = &(ckptglobals.filetable[fd]);
    free(file->filename);
    file->inuse = FALSE;
  }
  else{
    debug(stderr, "DEBUG: WARNING fd %d not use\n", fd);
  }
}

/**************************************************************
 * function: ckptsbrk
 * args: incr -- the amount to increment the sbrkptr by
 * returns: old break value on success, -1 otherwise
 * side effects: calls "real sbrk" then includes extra memory
 *               for checkpointing and mprotects (if incremental)
 * called from: various places, esp. malloc
 ************************************************************/

void * ckptsbrk(int incr)
{
  int test;
  unsigned long size;
  caddr_t old_heap_end, start, stop;
  void * retval;

  if(!ckptflags.checkpoint)
    return (void *)sbrk(incr);

  if(incr == 0){
    return (void *) sbrk(0);
  }
  else{
    retval = (void *) sbrk(incr);  /*-- ASSUMPTION: incr > 0 --*/

    if((intptr_t)retval == -1)
      return (void *)-1;

    /*-- Include the new segment of memory --*/
    if((long)ckptglobals.sbrkptr < (long)sbrk(0)){
      old_heap_end = ckptglobals.sbrkptr;
      ckptglobals.sbrkptr = (caddr_t)sbrk(0);
      size = (unsigned long)ckptglobals.sbrkptr - (unsigned long)old_heap_end;

      if(ckptglobals.inc_list)
        include_bytes(old_heap_end, size);

  /*-- call mprotect if appropriate --*/
      if(old_heap_end && ckptflags.incremental){
        start = (void*)(((long)old_heap_end / PAGESIZE)*PAGESIZE);
        stop = (void*)((((long)ckptglobals.sbrkptr) / PAGESIZE) * PAGESIZE);

        debug(stderr, "DEBUG: mprotecting memory from 0x%p to 0x%p\n", start, stop);

        test = mprotect(start, (size_t)(stop - start), PROT_READ);
        if(test == -1){
          fprintf(stderr, "CKPT: cannot set memory from 0x%p to 0x%p "
                          "to read only\n", start, stop);
          perror("mprotect");
          ckptflags.checkpoint = FALSE;
          exit(-1);
        }
        mark_dirty(ckptglobals.sbrkptr);
      }
    }
    else{
      fprintf(stderr, "ERROR: sbrk(%d) called, but new break !> old break\n",
                                                                  incr);
      ckptflags.checkpoint = FALSE;
      exit(-1);
    }
  }
  return (void *)retval;
}
