/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Dorian Arnold
 * Institution: University of Tennessee, Knoxville
 * Date: July, 1998
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include "copyright.h"
#include "libckpt.h"
#include <stdint.h>

/**************************************************************
 * function: config_pgfaulthandler()
 * args: sig -- signal being caught
 *       info -- additional info being passed to signal handler
 *       u    -- additional info being passed to signal handler
 * returns: none
 * side effects: marks the bottom of the stack
 * called from: installed for the setup's configuration only
 ************************************************************/
extern char * ptr;
void config_pgfaulthandler(int sig, siginfo_t * info, ucontext_t *u)
{
  if(sys.DATASTART == 0){
    ptr+=1024;
    siglongjmp(ckptglobals.env, 1);
  }
  else{
    sys.STACKBOTTOM = (caddr_t)(info->si_addr-1);
    siglongjmp(ckptglobals.env, 1);
  }
}

/**************************************************************
 * function: pgfaulthandler()
 * args: sig -- signal being caught
 *       info -- additional info being passed to signal handler
 *       u    -- additional info being passed to signal handler
 * returns: none
 * side effects: marks the faulting page as dirty and then sets
 *               the page allow reads AND writes.
 * called from: installed only during incremental checkpointing
 ************************************************************/

void pg_faulthandler(int sig, siginfo_t * info, ucontext_t *u)
{
  mark_dirty(info->si_addr);
  mprotect((caddr_t)(((uintptr_t)info->si_addr/PAGESIZE)*PAGESIZE), PAGESIZE,
           PROT_READ | PROT_EXEC | PROT_WRITE);
}

/**************************************************************
 * function: alrm_handler
 * args: none
 * side effects: sets the alarm to go of either in the mintime
 *               between checkpoints or maxtime between checkpoints
 *               depending on the scenario.
 * called from: set_alarm()
 ************************************************************/

void alrm_handler(){
  if(ckptglobals.enable == FALSE){ /*- min time expired, enable checkpointing --*/
    ckptglobals.enable = TRUE;
    if(ckptflags.verbose){
      fprintf(stderr, "CKPT: %li seconds since last checkpoint\n", ckptflags.mintime);
      fprintf(stderr, "CKPT: enabling checkpointing\n");

      if(ckptflags.maxtime != 0){
        fprintf(stderr, "CKPT: %li seconds until timer based checkpoint\n",
                ckptflags.maxtime - ckptflags.mintime);
      }
    }

    if(ckptflags.maxtime != 0){
      alarm(ckptflags.maxtime - ckptflags.mintime);
    }
  }
  else{  /*-- max time expired, time to checkpoint --*/
    if(ckptflags.verbose){
      fprintf(stderr, "CKPT: %li seconds since last checkpoint\n", ckptflags.maxtime);
      fprintf(stderr, "CKPT: now taking timer based checkpoint\n");
    }

    if(ckptglobals.mutex != CKPT_ALLOW){
      ckptglobals.mutex = CKPT_DELAY;
      if(ckptflags.verbose){
        fprintf(stderr, "CKPT: checkpoint must be delayed -- will checkpoint ASAP\n");
      }
      return;
    }

    checkpoint_here();
  }
}

/**************************************************************
 * function: child_handler()
 * args: none
 * returns: none
 * side effects: sets flag to denote that child has finished taking
 *               checkpoint and checks the exit status of that child.
 * called from: signal handler installed for forked checkpointing only
 ************************************************************/

void child_handler(){
  int status, time;

  ckptglobals.wait= FALSE;

  wait(&status);

  if(!WIFEXITED (status) || (WEXITSTATUS(status) != 0)){
    ckptflags.checkpoint = FALSE;
  }

  /*-- Check to see if an alarm was set. If not, call set_alarm since
       timer may have expired during ckpt. If an alarm was set, simply
       leave it installed. --*/
  time = alarm(0);
  if(!time)
    set_alarm();
  else
    alarm(time);
}
