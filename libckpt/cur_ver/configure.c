/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Dorian Arnold
 * Institution: University of Tennessee, Knoxville
 * Date: July, 1998
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "copyright.h"
#include <unistd.h>

int main()
{
  int maxpages, pagesize;
  struct rlimit resource_limit;
  FILE * f;

  f = fopen("local.h", "w");

  pagesize = getpagesize();
  fprintf(f, "#define PAGESIZE %d\n", pagesize);
  fprintf(f, "#define OFFSET 100\n");
  fprintf(f, "#define POINTERSIZE %zu\n", sizeof(void*));
  fprintf(f, "#define LONGSIZE %zu\n", sizeof(long));

  getrlimit(RLIMIT_NOFILE, &resource_limit);
  fprintf(f, "#define MAXFILES %lu\n", resource_limit.rlim_cur);

  getrlimit(RLIMIT_DATA, &resource_limit);   /*-- find max size of heap --*/
  maxpages = (int)resource_limit.rlim_cur / pagesize;
  if((int)resource_limit.rlim_cur % pagesize)
    maxpages++;

  fprintf(f, "#define MAXPAGES %d\n", maxpages);

  return 0;
}
