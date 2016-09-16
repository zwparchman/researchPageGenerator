#ifndef __clubs_h
#define __clubs_h 1

void exit(int);

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <setjmp.h>
#include <signal.h>
//#include <siginfo.h>
#include <sys/ucontext.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <dlfcn.h>



#ifndef __stdio_c  /*-- exclude from stdio.c --*/
#include <fcntl.h>
#include <unistd.h>
#endif             /* __stdio_c */

#include "local.h"
#include "dlist.h"
#include "structs.h"
#include "externs.h"
#include "checkpoint.h"

#define debug if(DEBUG) fprintf

#define MAXLEN 512

#define max(x, y)		(((x) < (y)) ? (y) : (x))
#define min(x, y)		(((x) > (y)) ? (y) : (x))

#define CKPT_ALLOW      0
#define CKPT_DISALLOW   1
#define CKPT_DELAY      2

#define CKPT_MUTEX_GET { ckptglobals.mutex = \
                         CKPT_DISALLOW; }
#define CKPT_MUTEX_REL \
        { if (ckptglobals.mutex == CKPT_DELAY) \
            checkpoint_here(); \
          ckptglobals.mutex = CKPT_ALLOW; }

#define OPEN(fname, oflag, mode) syscall(SYS_open, (fname), (oflag), (mode))
#define CLOSE(fd)            syscall(SYS_close, (fd))
#define READ(fd, buf, size)	 syscall(SYS_read, (fd), (buf), (size))
#define WRITE(fd, buf, size) syscall(SYS_write, (fd), (buf), (size))

#endif /*-- __clubs_h --*/
