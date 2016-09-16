#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include "checkpoint.h"

#define  N        200
#define  MAXSIZE  100

void exit(int);
void free(void*);
void * malloc(size_t);

int MAT_SIZE;

void read_matrix(/* int * */);
void mult_matrix(/* int * */);

void read_matrix(A)
int A[][N];
{

  int i,j;

  srand(1);

  printf("initializing matrix A\n");

  for(i=0;i<N;i++)
    for(j=0;j<N;j++)
      A[i][j] = rand() % MAXSIZE;

  exclude_bytes((char *)A, MAT_SIZE, CKPT_RDONLY);
}

void mult_matrix(A, B)
int A[][N];
int B[][N];
{

  int i,j,k;
  int size;
  int fd;

  size = N * N;

  printf("initializing matrix B\n");

  for(i=0;i<N;i++)
    for(j=0;j<N;j++)
      B[i][j] = 0;

  printf("doing the multiplication B = A * A\n");

  for(i=0; i<N; i++) {
    for(j=0; j<N; j++)
      for(k=0; k<N; k++)
	    B[i][j] += A[i][k] * A[k][j];

    if ( i % 40 == 0 ) {
      checkpoint_here();
      printf("row %d\n",i);
    }
  }

  printf("moving the result from matrix B to matrix A\n");

  include_bytes((char *)A, MAT_SIZE);

  for(i=0;i<N;i++)
    for(j=0;j<N;j++)
      A[i][j] = B[i][j];

  fd = open("mult.output", O_WRONLY | O_CREAT, 0644);
  write(fd, A, size);
}

int ckpt_target(int argc, char **argv)
{

  int *A;
  int *B;
  int num;
  int size;
  caddr_t addr;

  if (argc != 1) {
    fprintf(stderr,"Usage: %s\n",argv[0]);
    exit(-1);
  }

  size = N * N;

  A = (int *)malloc(sizeof(int) * size);
  B = (int *)malloc(sizeof(int) * size);
  MAT_SIZE = sizeof(int) * size;

  if (!A || !B) {
    printf("not enough memory\n");
    exit(-1);
  }

  read_matrix(A);

  mult_matrix(A, B);

  free(A);
  free(B);
}

//for the fortran compiler
int ckpt_target_(int argc, char **argv){
    return ckpt_target(argc, argv);
}
