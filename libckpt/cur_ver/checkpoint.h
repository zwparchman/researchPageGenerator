#ifndef __checkpoint_h
#define __checkpoint_h 1

#define ENOCKPT 201   /*- checkpointing disabled -*/
#define ETOOSOON 202  /*- checkpoint too soon -*/

#define CKPT_DEAD       1
#define CKPT_RDONLY     0

int checkpoint_here();
int include_bytes(char *, unsigned long);
int exclude_bytes(char *, unsigned long, int);

#endif /*-- __checkpoint_h --*/
