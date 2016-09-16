#include <stdio.h>
#include <sys/syscall.h>

int ERROR = 1;
 
int open(const char * path, int oflag, ...)
{
  ERROR = 0;
  printf("In user defined open function -- great!\n");
  return syscall(SYS_open, path, oflag);
}

int _open(const char * path, int oflag, ...)
{
  ERROR = 0;
  printf("In user defined _open function -- great!\n");
  return syscall(SYS_open, path, oflag);
}

int close(int filedes)
{
  ERROR = 0;
  printf("In user defined close function -- great!\n");
  return syscall(SYS_close, filedes);
}

int _close(int filedes)
{
  ERROR = 0;
  printf("In user defined _close function -- great!\n");
  return syscall(SYS_close, filedes);
}

main(){
  FILE * f;

  puts("** This program tests if libckpt can intercept calls to\n"
       "** fopen/fclose before calling the system call open/close.\n");

  f = fopen("testprog.c", "r");
  if(f){
      puts("Call to fopen: success");
  } else {
      puts("Call to fopen: failure");
      goto finish;
  }

  if(fclose(f)){
      puts("Call to fclose: failure");
  } else {
      puts("Call to fclose: success");
  }

finish:
  if(ERROR){
    puts("Call to fopen/fclose failed to call user defined subroutines _open/_close, "
         "PLEASE SEE USERS GUIDE");
  }
}
