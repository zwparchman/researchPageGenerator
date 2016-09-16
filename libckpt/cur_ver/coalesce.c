/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Dorian Arnold
 * Institution: University of Tennessee, Knoxville
 * Date: July, 1998
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#include <stdlib.h>
#include "libckpt.h"
#include "copyright.h"

#define BUFSIZE 8192

caddr_t DATASTART=0;
caddr_t FILEAREA=0;
off_t df_seekptr=0;
BOOL verbose;
char * filename;
int DEBUG;

char mapbuf[BUFSIZE];
char databuf[BUFSIZE];
int datanochars, mapnochars, maptemp_fd, datatemp_fd;
const int MAPCHUNKSIZE = POINTERSIZE + 2*LONGSIZE;

typedef struct clist_node{
  struct clist_node *flink;
  struct clist_node *blink;
  caddr_t addr;
  caddr_t stop;
  unsigned long size;
  int file_no;
  off_t offset;
} *CLIST_NODE;

typedef struct clist{
  CLIST_NODE head;
  CLIST_NODE cur;
} * CLIST;

static void cl_insert(CLIST_NODE, CLIST_NODE);
static CLIST make_clist(caddr_t);
static void destroy_clist(CLIST);
static CLIST_NODE findpos(CLIST, caddr_t);
static void clist_insertchunk(CLIST, caddr_t, int, int, off_t);
static int coa_transfer_chunks(CLIST);

/**************************************************************
 * function: cl_insert
 * args: node to use as reference,
 *       new_node to insert after reference
 * returns: none
 * side effects: places new node into list
 * called from: make_clist, 
 *************************************************************/
static void cl_insert(CLIST_NODE node, CLIST_NODE new_node)
{
  node->flink->blink = new_node;
  new_node->flink = node->flink;
  new_node->blink = node;
  node->flink = new_node;
}

/**************************************************************
 * function: make_clist
 * args: current sbrk ptr
 * returns: a pointer to the newly allocated list
 * side effects: creates a list and initializes its first nodes
 * called from: ckpt_coalesce()
 *************************************************************/
static CLIST make_clist(caddr_t stop_addr)
{
  CLIST list;
  CLIST_NODE ptr;

  list = (CLIST)malloc(sizeof(struct clist));

  list->head = (CLIST_NODE)malloc(sizeof(struct clist_node));
  list->head->flink = list->head;
  list->head->blink = list->head;

  ptr = (CLIST_NODE)malloc(sizeof(struct clist_node));
  ptr->addr = DATASTART;
  ptr->stop = DATASTART;
  ptr->size = 0;
  ptr->file_no = -1;
  ptr->offset = -1;
  cl_insert(list->head, ptr);

  ptr = (CLIST_NODE)malloc(sizeof(struct clist_node));
  ptr->addr = stop_addr;
  ptr->stop = stop_addr;
  ptr->size = 0;
  ptr->file_no = -1;
  ptr->offset = -1;
  cl_insert(list->head->flink, ptr);

  list->cur = list->head->flink;
  return list;
}

/**************************************************************
 * function: destroy_clist
 * args: list -- list to destroy
 * returns: none
 * side effects: destroys list and frees up all allocated memory
 * called from: ckpt_coalesce()
**************************************************************/
static void destroy_clist(CLIST list)
{
  CLIST_NODE ptr;

  ptr = list->head->flink;
  while(ptr != list->head){
    ptr = ptr->flink;
    free(ptr->blink);
  }

  free(list->head);
  free(list);
}

/**************************************************************
 * function: findpos()
 * args: list -- list to search
 *       addr -- addr to look for
 * returns: returns the node of the list with nearest address "<=" addr
 * called from: check_list()
 *************************************************************/
static CLIST_NODE findpos(CLIST list, caddr_t addr)
{
  CLIST_NODE l;

  l = list->cur;
  if(l->addr == addr) return l;

  while(l->addr <= addr){
    l = l->flink;
    if(l->addr > addr){
      list->cur = l->blink;
      return list->cur;
    }
  }
  while(l->addr > addr){
    l = l->blink;
    if(l->addr <= addr){ 
      list->cur = l;
      return list->cur;
    }
  }
}

/**************************************************************
 * function: clist_insertchunk()
 * args: list -- list to search
 *       addr -- addr of memory chunk
 *       size -- size of memory chunk
 * returns: none
 * called from: ckpt_coalesce()
 *************************************************************/
static void 
clist_insertchunk(CLIST list, caddr_t addr, int size, int file_no, off_t offset)
{
  CLIST_NODE ptr, new_node;
  caddr_t stop, orig_addr;

  stop = (caddr_t)addr+size;
  ptr = findpos(list, addr);

  if(addr > ptr->stop){
    new_node = (CLIST_NODE)malloc(sizeof(struct clist_node));
    new_node->addr = addr;
    new_node->offset = offset;
    new_node->stop = (caddr_t)min(ptr->flink->addr, stop);
    new_node->size = (unsigned long)(new_node->stop) - (unsigned long)(new_node->addr);
    new_node->file_no = file_no;
    cl_insert(ptr, new_node);
    offset += new_node->size;
    addr = (caddr_t)min(ptr->flink->addr, stop);
    ptr = ptr->flink;
  }

  while(stop > ptr->stop){
    if(ptr->stop != ptr->flink->addr){
      orig_addr = addr;
      addr = ptr->stop;
      offset += addr - orig_addr;
      new_node = (CLIST_NODE)malloc(sizeof(struct clist_node));
      new_node->addr = addr;
      new_node->offset = offset;
      new_node->stop = (caddr_t)min(ptr->flink->addr, stop);
      new_node->size = (unsigned long)(new_node->stop) -
                       (unsigned long)(new_node->addr);
      new_node->file_no = file_no;
      cl_insert(ptr, new_node);
      offset += new_node->size;
      addr = (caddr_t)min(ptr->flink->addr, stop);
    }

    ptr=ptr->flink;
  }
}

/**************************************************************
 * function: coa_transfer_chunk
 * args: list -- contains pertinent info to write to checkpoint
 * returns: no. of chunks written
 * side effects: calls write_chunk in order to write data in list
 *               to checkpoint
 * called from ckpt_coalesce()
 *************************************************************/
static int coa_transfer_chunks(CLIST list)
{
  CLIST_NODE ptr;
  char datafile[256];
  int retval=0, data_fd;

  for(ptr=list->head->flink; ptr != list->head; ptr = ptr->flink){
    if(ptr->size > 0){
      sprintf(datafile, "%s.%s.%d", filename, "data", ptr->file_no);
      data_fd = OPEN(datafile, O_RDONLY, 0644);

      coa_write_chunk(ptr->addr, ptr->size, data_fd, ptr->offset);

      debug(stderr, "DEBUG: writing 0x%p - Ox%p from ckptfile %d\n", ptr->addr, 
                                         ptr->addr+ptr->size, ptr->file_no);

      CLOSE(data_fd);
      retval++;
    }
  }
  return retval;
}

/**************************************************************
 * function: coa_write_chunk
 * args: addr -- start address of memory chunk
 *       size -- size of memory chunk
 *       pn  --  corresponding page number
 *       data_fd -- file desc. for data file to read from
 *       offset -- offset into data file
 * returns: none
 * side effects: uses a global buffer to write out a chunk to the
 *               appropriate data and map files during coalescing
 * called from: ckpt_coalesce()
 *************************************************************/
static void coa_write_chunk(char * addr, long size, int data_fd, off_t offset)
{
  char buf[BUFSIZE];
  long orig_size;

  orig_size = size;
  /*-- writing map portion --*/
  if(MAPCHUNKSIZE + mapnochars <= BUFSIZE){
    memcpy(mapbuf+mapnochars, &addr, POINTERSIZE);
    mapnochars+=POINTERSIZE;
    memcpy(mapbuf+mapnochars, &size, LONGSIZE);
    mapnochars+=LONGSIZE;
    memcpy(mapbuf+mapnochars, &df_seekptr, LONGSIZE);
    mapnochars+=LONGSIZE;
  }
  else {
    write(maptemp_fd, mapbuf, mapnochars);
    mapnochars=0;
    memcpy(mapbuf+mapnochars, &addr, POINTERSIZE);
    mapnochars+=POINTERSIZE;
    memcpy(mapbuf+mapnochars, &size, LONGSIZE);
    mapnochars+=LONGSIZE;
    memcpy(mapbuf+mapnochars, &df_seekptr, LONGSIZE);
    mapnochars+=LONGSIZE;
  }

  /*-- writing data portion --*/
  lseek(data_fd, offset, SEEK_SET);

  if(size == BUFSIZE){
    if(datanochars == 0){
      read(data_fd, buf, BUFSIZE);
      write(datatemp_fd, buf, BUFSIZE);
    }
    else{
      write(datatemp_fd, databuf, datanochars);
      read(data_fd, buf, BUFSIZE);
      write(datatemp_fd, buf, BUFSIZE);
      datanochars=0;
    }
  }
  else if(size < BUFSIZE){
    if(datanochars+size <= BUFSIZE){
      read(data_fd, buf, size);
      memcpy(databuf+datanochars, buf, size);
      datanochars+=size;
    }
    else{
      write(datatemp_fd, databuf, datanochars);
      read(data_fd, buf, size);
      memcpy(databuf, buf, size);
      datanochars=size;
    }
  }
  else{
    write(datatemp_fd, databuf, datanochars);
    while(size >= BUFSIZE){
      read(data_fd, buf, BUFSIZE);
      write(datatemp_fd, buf, BUFSIZE);
      size -= BUFSIZE;
    }
    if(size > 0){
      read(data_fd, buf, size);
      memcpy(databuf, buf, size);
      datanochars = size;
    }
    else{
      datanochars=0;
    }
  }

  df_seekptr += orig_size;
}

/**************************************************************
 * function: flush_buffers()
 * args: none
 * returns: none
 * side effects: dumps the global buffers to data and map files
 * called from: ckpt_coalesce()
 *************************************************************/
static void flush_buffers(){
  if(mapnochars > 0){
    write(maptemp_fd, mapbuf, mapnochars);
  }
  if(datanochars > 0){
    write(datatemp_fd, databuf, datanochars);
  }
}

/**************************************************************
 * function: main()
 * args: none
 * returns: none
 * side effects: coalesces all existing checkpoint files into a 
 *               single map and a single data file.
 * called from: ckpt_recover(), take_ckpt()
 *************************************************************/
void main(int argc, char ** argv)
{
  int i=0, j, no_chunks, total_chunks=0, no_files;
  int map_fd, data_fd;
  char mapfile[256], datafile[256], maptemp[256], datatemp[256];
  char * addr;
  caddr_t stopaddr, topstack;
  long size, pn;
  off_t seekptr;
  CLIST list;
  int page1; char buf[BUFSIZE];

  if(argc != 5){
    fprintf(stderr, "Error: Coalesce called with wrong number of args\n");
    exit(-1);
  }

  FILEAREA = (caddr_t)atol(argv[1]);
  filename = strdup(argv[2]);
  verbose = (BOOL)atoi(argv[3]);
  DEBUG = atoi(argv[4]);

  /*-- checking for the number of checkpoint files --*/
  while(1){
    sprintf(datafile, "%s.%s.%d", filename, "data", i);
    data_fd = OPEN(datafile, O_RDONLY, 0644);

    sprintf(mapfile, "%s.%s.%d", filename, "map", i);
    map_fd = OPEN(mapfile, O_RDONLY, 0644);

    if(data_fd == -1 || map_fd == -1){
      CLOSE(data_fd); CLOSE(map_fd);
      break;
    }
    CLOSE(data_fd); CLOSE(map_fd);
    i++;
  }

  no_files = i;

  if(no_files == 0){
    fprintf(stderr, "Error: Recovery specified, but no checkpoint files found\n");
    exit(-1);
  }
  else if(no_files == 1){
    return;
  }

  /*-- reset buffer counts --*/
  datanochars=0;
  mapnochars=0;

  if(verbose)
    fprintf(stderr, "CKPT: coalescing %d sets of checkpoint files\n", no_files);

  sprintf(datatemp, "%s.%s.temp", filename, "data");
  remove(datatemp);
  datatemp_fd = OPEN(datatemp, O_WRONLY | O_CREAT, 0644);
 
  sprintf(maptemp, "%s.%s.temp", filename, "map");
  remove(maptemp);
  maptemp_fd = OPEN(maptemp, O_WRONLY | O_CREAT, 0644);

  /*-- transfer data to the relevant temp file (most recent to least recent) --*/
  for(i=no_files-1; i>=0; i--){
    sprintf(mapfile, "%s.%s.%d", filename, "map", i);
    map_fd = OPEN(mapfile, O_RDONLY, 0644);

    if(i==no_files-1){                  /*-- read this info from last file only --*/
      read(map_fd, (char *)&topstack, POINTERSIZE);
      read(map_fd, (char *)&DATASTART, POINTERSIZE);
      read(map_fd, (char *)&stopaddr, POINTERSIZE);
      read(map_fd, (char *)&no_chunks, LONGSIZE);
      write(maptemp_fd, (char *)&topstack, POINTERSIZE);
      write(maptemp_fd, (char *)&DATASTART, POINTERSIZE);
      write(maptemp_fd, (char *)&stopaddr, POINTERSIZE);
      write(maptemp_fd, (char *)&no_chunks, POINTERSIZE);
      list = make_clist(stopaddr);
    }
    else{
      lseek(map_fd, POINTERSIZE*3, SEEK_SET);
      read(map_fd, (char *)&no_chunks, LONGSIZE);
    }

    no_chunks--;  /*-- exclude the stack's chunk --*/
 
    /*-- exclude filearea, if present --*/
    if(FILEAREA != 0){
      no_chunks--;
    }
 
    for(j=0; j<no_chunks; j++){
      read(map_fd, (char *)&addr, POINTERSIZE);
      read(map_fd, (char *)&size, LONGSIZE);
      read(map_fd, (char *)&seekptr, LONGSIZE);

      clist_insertchunk(list, addr, size, i, seekptr);
    }

    CLOSE(map_fd);
  }

  total_chunks = coa_transfer_chunks(list);
  destroy_clist(list);

  sprintf(datafile, "%s.%s.%d", filename, "data", no_files-1);
  data_fd = OPEN(datafile, O_RDONLY, 0644);

  sprintf(mapfile, "%s.%s.%d", filename, "map", no_files-1);
  map_fd = OPEN(mapfile, O_RDONLY, 0644);

  /*-- We must advance the seekptr in the map file appropriately --*/
  lseek(map_fd, POINTERSIZE*3, SEEK_SET);
  read(map_fd, (char *)&no_chunks, LONGSIZE);
  no_chunks--;         /*-- exclude the stack's chunk --*/
  if(FILEAREA != 0){   /*-- exclude filearea, if present --*/
    no_chunks--;
  }
  lseek(map_fd, no_chunks*MAPCHUNKSIZE, SEEK_CUR);


  if(FILEAREA != 0){
    read(map_fd, (char *)&addr, POINTERSIZE);
    read(map_fd, (char *)&size, POINTERSIZE);
    read(map_fd, (char *)&seekptr, POINTERSIZE);
    coa_write_chunk(addr, size, data_fd, seekptr);

    debug(stderr, "DEBUG: writing 0x%p - Ox%p from ckptfile %d\n", addr, 
                                          addr+size, no_files-1);

    total_chunks++;
  }

  read(map_fd, (char *)&addr, POINTERSIZE);
  read(map_fd, (char *)&size, POINTERSIZE);
  read(map_fd, (char *)&seekptr, POINTERSIZE);
  coa_write_chunk(addr, size, data_fd, seekptr);

  debug(stderr, "DEBUG: writing 0x%p - Ox%p from ckptfile %d\n", addr, 
                                          addr+size, no_files-1);

  total_chunks++;
  
  CLOSE(map_fd);
  CLOSE(data_fd);

  flush_buffers();
  lseek(maptemp_fd, POINTERSIZE*3, SEEK_SET);
  write(maptemp_fd, (char *)&total_chunks, LONGSIZE);
  CLOSE(maptemp_fd);
  CLOSE(datatemp_fd);

  sprintf(datafile, "%s.%s.0", filename, "data");
  sprintf(mapfile, "%s.%s.0", filename, "map");
  rename(datatemp, datafile);
  rename(maptemp, mapfile);

  for(i=no_files-1; i>0; i--){
    sprintf(datafile, "%s.%s.%d", filename, "data", i);
    sprintf(mapfile, "%s.%s.%d", filename, "map", i);
    remove(datafile);
    remove(mapfile);
  }

  if(verbose)
    fprintf(stderr, "CKPT: coalescing complete\n");
}
