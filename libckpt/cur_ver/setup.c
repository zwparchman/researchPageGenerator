/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Dorian Arnold
 * Institution: University of Tennessee, Knoxville
 * Date: July, 1998
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include "libckpt.h"
#include "copyright.h"
#include <stdint.h>

void * malloc( size_t );
void free(void*);
void exit(int);

int DEBUG=0;
RESOURCES sys; /*--- contains global state of the systems resources ---*/
CKPTFLAGS ckptflags; /*--- contains the global ckpt flags ---*/
CKPTGLOBALS ckptglobals; /*--- contains the ckpt globals ---*/

/**************************************************************
 * function: ckpt_setup() & ckpt_setup_
 * args: argc and argv
 * returns: void
 * side effects: see comments next to function calls
 *               NB: ckpt_setup_ simply sets flag to denote fortran
 *                   program and then calls ckpt_setup
 * called from: ckpt_setup called from c program main
 *              ckpt_setup_ called from fortran program main
 *************************************************************/

void ckpt_setup_(int * argc, char ** argv){
  ckptglobals.lang = TRUE;  /*-- fortran program --*/
  ckpt_setup(argc, argv);
}

void ckpt_setup(int *argc, char **argv){
  caddr_t addr;
  ckptglobals.sbrkptr = 0;
  ckptglobals.inc_list = 0;

  /*-- uncomment this line for extra debugging info --*/
  //DEBUG = 1;

  set_defaults();  /*-- sets default parameters --*/

  read_dotfile();  /*-- reads the ./.ckptrc parameter file --*/

  if(ckptflags.checkpoint){
    check_for_recovery(argc, argv);  /*-- checks for recovery flag --*/

    if(ckptglobals.lang && *argc == 1) /*-- recover from Fortran prog. --*/
      ckpt_recover();

    check_files();   /*-- checks the "ckptdir" for old checkpoints --*/

    configure();     /*-- sets variables to distinguish the environment --*/

    if(ckptflags.incremental){
      set_incremental();
      force_dirty(); /*-- mark the previously dirty pages as such --*/
    }

    set_globals();   /*-- initializes the global struct --*/

                     /*-- include all of data and heap portions of mem. --*/
    include_bytes(sys.DATASTART, (long)ckptglobals.sbrkptr - (long)sys.DATASTART);

                     /*-- exclude the pagelist from checkpoints --*/
    addr = ckptglobals.page_dirty;
    debug(stderr, "DEBUG: excluding pagelist from 0x%p to 0x%p\n",
                              addr, (addr + (sizeof(char)*MAXPAGES)));

    exclude(ckptglobals.inc_list, addr, (long)(addr + (sizeof(char)*MAXPAGES)) -
                                        (long)addr);

    set_alarm();     /*-- sets up synchronous checkpointing --*/

    print_flags();   /*-- self-explanatory --*/
  }
}

/**************************************************************
 * function: configure
 * args: none
 * return: void
 *
 * side effects: checks different system resources and initializes
 *               the global struct sys to appropriate values.
 *               It finds: max # of files, pagesize, pointersize,
 *               bottom of the stack, the "filearea" and its size if
 *               any.
 *
 * called from: ckpt_setup()
 *************************************************************/

extern etext;
char * ptr;

static void configure(){
  int stack_dummy;
  long top, bottom;
  int i, j, flag=0;
  FILE ** f;
  struct sigaction act;

  /*-- Find where the data section of memory layout starts --*/
  ptr = (char *)&etext;
  ptr = (caddr_t)(((long)ptr / PAGESIZE + 1) * PAGESIZE);

  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO;
  act.sa_handler = config_pgfaulthandler;
  sigaction(SIGSEGV, &act, 0);

  sigsetjmp(ckptglobals.env, 1);
  *ptr = *ptr;

  sys.DATASTART = ptr;
  debug(stderr, "DEBUG: The data segment starts at 0x%p\n", sys.DATASTART);

  /*-- Find the bottom of the stack --*/
  sys.STACKBOTTOM = 0;

  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO | SA_RESETHAND;
  act.sa_handler = config_pgfaulthandler;
  sigaction(SIGSEGV, &act, 0);

  ptr = (char *)&stack_dummy;

  /*--- this loop will raise a segfault when ptr runs off the
        bottom of the stack. the sighandler will act appropriately ---*/
  sigsetjmp(ckptglobals.env, 1);

  while (!sys.STACKBOTTOM) {
    *ptr = *ptr;
    ptr++;
  }
 
  /*--- finding the area where file_ptrs are stored ---*/

  f = (FILE **)malloc(sizeof(FILE *) * MAXFILES);
  f[0] = fopen("setup.c", "r");

  /*--- sometimes the area is just above the stack, and
        sometimes it is in the heap ---*/

  if((char*)(f[0]) > (char *)sbrk(0)){  /*--- if above the stack ---*/
    sys.FILEAREA = (caddr_t)f[0];
  }
  else{                /*--- else (in the heap) ---*/
    sys.FILEAREA = 0;
    sys.FILEAREASIZE = 0;
  }

  debug(stderr, "DEBUG: FILEAREA = 0x%p\n"
         "SETUP: f[0] = 0x%p\n", sys.FILEAREA, f[0]);

  if(sys.FILEAREA != 0){    /*-- need size of filearea --*/

/*--- keep opening file_ptrs till the "filearea" is full and the
      ptrs are being allocated from the heap ---*/

    for(i=1; i<MAXFILES; i++){
      f[i] = fopen("setup.c", "r");
      if(f[i] < f[i-1]){
        bottom = (long)f[i-1];
        top = (long)f[0];
        sys.FILEAREASIZE = bottom - top + ((long)f[1] - (long)f[0]);
        flag = 1;
        break;
      }
    }

    if(flag != 1){
      sys.FILEAREASIZE = ((long)f[1] - (long)f[0]) * MAXFILES;
    }

    for(j=0; j<=i; j++){  /*--- close all opened file_ptrs ---*/
      fclose(f[j]);
    }
  }
  else{
    fclose(f[0]);
  }
  free(f);
}

/**************************************************************
 * function: check_for_recovery()
 * args: argc and argv
 * returns: void
 * side effects:  checks to see if the recovery flag has been
 *                set on the command line and if so, it initiates
 *                the process, otherwise it returns to the caller.
 *
 * called from: ckpt_setup()
 *************************************************************/

static void check_for_recovery(int *argc, char ** argv){
  int i;

  if(ckptglobals.lang == FALSE){   /*-- if C program --*/
    for(i=1; i< *argc; i++){
      if(!strcmp(argv[i], "=recover")){
        if(*argc == 2){
          ckpt_recover();
        }
        else{
          fprintf(stderr, "=recover flag, if used, must be "
                          "the only argument\n");
          exit(-1);
        }
      }
    }
  }
}

/**************************************************************
 * function: set_defaults()
 * args: none
 * returns: void
 * side effects: sets the global ckpt flags in the global struct
 *
 * called from: ckpt_setup()
 *************************************************************/

void set_defaults(){
  ckptflags.checkpoint = TRUE;
  ckptflags.fork = FALSE;
  ckptflags.enhanced_fork = FALSE;
  ckptflags.verbose = FALSE;
  ckptflags.incremental = FALSE;
  ckptflags.mintime = 0;
  ckptflags.maxtime = 600;
  ckptflags.maxfiles = 0;

  strcpy(ckptflags.dir, ".");
  strcpy(ckptflags.filename, "ckpt");
}

/**************************************************************
 * function: read_dotfile
 * args: none
 * returns: void
 * side effects: reads info found in parameter file, ".ckptrc",
 *               if exists and sets appropriate globals. If there
 *               is an error in the file, it says so and exits.
 * called from: ckpt_setup()
 *
 * Note: .ckptrc must be in the current directory.
 *************************************************************/

void read_dotfile(){
  int line_num =0, fd;
  BOOL error=FALSE;
  FILE * f;
  char buf[MAXLEN];
  char field[MAXLEN];
  char value[MAXLEN];

  fd = OPEN(".ckptrc", O_RDONLY, 0644);
  f = fdopen(fd, "r");

  if(f != NULL){
    while(fgets(buf, MAXLEN, f)){
      line_num++;
      field[0] = value[0] = 0;
      sscanf(buf, "%s %s", field, value);

      error = check_and_set(field, value);

      if(error){
        fprintf(stderr, "Error: line %d of parameter file .ckptrc\n",
                         line_num);
        exit(-1);
      }
    }
    if(ckptflags.incremental && ckptflags.maxfiles == 1){
      fprintf(stderr, "Error: If incremental checkpointing specified, "
                      "maxfiles must be greater than 1\n");
      exit(-1);
    }
    if(!ckptflags.fork){
      if(ckptflags.enhanced_fork){
        fprintf(stderr, "Error: Enhanced fork can only be used if "
                        "if the fork option is set\n");
        exit(-1);
      }
    }
    fclose(f);
    CLOSE(fd);
  }
}

/**************************************************************
 * function: check_and_set
 * args: field (1st word on a line in .ckptrc)
 *       value (2nd word on a line in .ckptrc)
 * returns: 0 if no error on line, 1 otherwise.
 * called from: read_dotfile
 *************************************************************/

static int check_and_set(char *field, char* value){
  BOOL error = FALSE;
  int mode;

  if(!strcmp(field, "checkpointing")){ 
    if(!strcmp(value, "on")){
      ckptflags.checkpoint = TRUE;
    }
    else if(!strcmp(value, "off")){
      ckptflags.checkpoint = FALSE;
    }
    else
      error = TRUE;
  }
  else if(!strcmp(field, "verbose")){
    if(!strcmp(value, "on")){
      ckptflags.verbose = TRUE;
    }
    else if(!strcmp(value, "off")){
      ckptflags.verbose = FALSE;
    }
    else
      error = TRUE;
  }
  else if(!strcmp(field, "fork")){
    if(!strcmp(value, "on")){
      ckptflags.fork = TRUE;
    }
    else if(!strcmp(value, "off")){
      ckptflags.fork = FALSE;
    }
    else
      error = TRUE;
  }
  else if(!strcmp(field, "enhanced_fork")){
    if(!strcmp(value, "on")){
      ckptflags.enhanced_fork = TRUE;
    }
    else if(!strcmp(value, "off")){
      ckptflags.enhanced_fork = FALSE;
    }
    else
      error = TRUE;
  }
  else if(!strcmp(field, "incremental")){
    if(!strcmp(value, "on")){
      ckptflags.incremental = TRUE;
    }
    else if(!strcmp(value, "off")){
      ckptflags.incremental = FALSE;
    }
    else
      error = TRUE;
  }
  else if(!strcmp(field, "exclude")){
    if(!strcmp(value, "on")){
      ckptflags.exclude = TRUE;
    }
    else if(!strcmp(value, "off")){
      ckptflags.exclude = FALSE;
    }
    else
      error = TRUE;
  }
  else if(!strcmp(field, "mintime")){
    if(isnum(value)){
      ckptflags.mintime = strtol(value, 0, 0);
    }
    else{
      error = TRUE;
    }
  }
  else if(!strcmp(field, "maxtime")){
    if(isnum(value)){
      ckptflags.maxtime = strtol(value, 0, 0);

      if(ckptflags.mintime && ckptflags.maxtime <= ckptflags.mintime){
        fprintf(stderr, "Error: max time must be greater than min time\n");
        error = TRUE;
      }
    }
    else{
      error = TRUE;
    }
  }
  else if(!strcmp(field, "maxfiles")){
    if(isnum(value)){
      ckptflags.maxfiles = strtol(value, 0, 0);
    }
    else{
      error = TRUE;
    }
  }
  else if(!strcmp(field, "directory")){
    mode = access(value, F_OK);
    if (mode == -1){
      fprintf(stderr, "Error: directory %s does not exist\n",
                       value);
      error = TRUE;
    }
    else{
      mode = access(value, W_OK);
      if(mode == -1){
        fprintf(stderr, "Error: cannot write to directory %s\n",
                         value);
        error = TRUE;
      }
      else{
        strcpy(ckptflags.dir, value);
      }
    }
  }
  else if(!strcmp(field, "filename")){
    strcpy(ckptflags.filename, value);
  }
  else{
    error = TRUE;
  }

  return error;
}

/**************************************************************
 * function: is_num
 * arguments: s, a pointer to a string
 * returns: True if all the chars in s are digits, false otherwise
 * called from: check_and_set()
 *************************************************************/
 
static int isnum(char * s){
  if(!s) return FALSE;

  while(*s){
    if(!isdigit(*s))
      return FALSE;

    s++;
  }

  return TRUE;
}

/**************************************************************
 * function: check_files()
 * args: none
 * returns: void
 * side effects: checks for old ckpt.* files and exits if found.
 * called from: ckpt_setup()
 *************************************************************/
static void check_files(){
  DIR *dir;
  struct dirent *d;
  char * tempmap, *tempdata;
  int size;

  dir = opendir(ckptflags.dir);
  if (dir == NULL) {
    perror(ckptflags.dir);
    exit(1);
  }

  size = strlen(ckptflags.filename) + 6;
  tempmap = (char *)malloc(size * (sizeof(char*)));

  strcpy(tempmap, ckptflags.filename);
  strcat(tempmap, ".map.");

  size = strlen(ckptflags.filename) + 7;
  tempdata = (char *)malloc(size * (sizeof(char*)));
 
  strcpy(tempdata, ckptflags.filename);
  strcat(tempdata, ".data.");


  for (d = readdir(dir); d != NULL; d = readdir(dir)) {
    if( (strstr(d->d_name, tempmap)==d->d_name &&
         isnum(d->d_name + strlen(tempmap)))   ||
        (strstr(d->d_name, tempdata)==d->d_name &&
         isnum(d->d_name + strlen(tempdata))) ) {

      fprintf(stderr, "Error: checkpoint files exist in directory "
                      "%s\n", ckptflags.dir);
      closedir(dir);
      exit(-1);
    }
  }

  closedir(dir);
}

/**************************************************************
 * function: set_globals
 * args: none
 * returns: void
 * side effects: initializes global ckpt variables.
 * called from: ckpt_setup()
 *************************************************************/
static void set_globals(){
  ckptglobals.wait = FALSE;
  ckptglobals.enable = TRUE;
  ckptglobals.filenum = 0;
  ckptglobals.ckpt_num = 0;
  ckptglobals.mutex = CKPT_ALLOW;

  if(!ckptflags.incremental)
    ckptglobals.sbrkptr = (caddr_t)sbrk(0);

  ckptglobals.inc_list = make_ml();
  ckptglobals.save_list = make_ml();
}

/**************************************************************
 * function: set_incremental
 * args: setup -- if true, being called during setup
                  otherwise, being called after a checkpoint
 * returns: void
 * side effects: sets up incremental checkpointing. Sets all
                 pages to clean, protects them from writing,
                 sets up pagefault handler.
 * called from: ckpt_setup() and cleanup()
 *************************************************************/
void set_incremental()
{
  int i, test;
  caddr_t start, stop;
  caddr_t pageliststart, pagelistend;
  unsigned long addr;
  struct sigaction act;

  ckptglobals.sbrkptr = (caddr_t)sbrk(0);

  start = sys.DATASTART;
  start += PAGESIZE;
  stop = ckptglobals.sbrkptr;

  if((uintptr_t)stop % PAGESIZE)
    //stop += PAGESIZE - ((int)stop % PAGESIZE);
    stop -= ((uintptr_t)stop % PAGESIZE);

  addr = (unsigned long)ckptglobals.page_dirty;
  pageliststart = (caddr_t)((addr / PAGESIZE)*PAGESIZE);
  pagelistend = (caddr_t)(((addr + (sizeof(char)*MAXPAGES)) / PAGESIZE+1)
                 * PAGESIZE);

  for(i=0; i<MAXPAGES; i++)
    ckptglobals.page_dirty[i] = (char)0;
  
  debug(stderr, "DEBUG: mprotecting memory from 0x%p to 0x%p\n"
                "DEBUG: unprotecting pagelist from 0x%p to 0x%p\n",
                                  start, stop, pageliststart, pagelistend);

  /*-- install signal handler for seg. fault --*/
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO | SA_RESTART;
  act.sa_handler = pg_faulthandler;
  if(sigaction(SIGSEGV, &act, 0) == -1){
    fprintf(stderr, "CKPT: cannot install signal handler for SIGSEGV\n");
    exit(-1);
  }

  /*-- marking all pages in data and heap to "READ ONLY" --*/
  test = mprotect(start, (size_t)(stop - start), PROT_READ | PROT_EXEC);

  if(test == -1){
    fprintf(stderr, "CKPT: cannot set memory from 0x%p to 0x%p "
                    "to read only\n", start, stop);
    perror("mprotect");
    ckptflags.checkpoint = FALSE;
    exit(-1);
  }

  /*-- mark all pages in the page list to "READ/WRITE" --*/
  test = mprotect(pageliststart, (size_t)(pagelistend - pageliststart),
                  PROT_READ | PROT_WRITE );
  if(test == -1){
    fprintf(stderr, "CKPT: cannot set memory from 0x%p to 0x%p "
                    "to read/write\n", pageliststart, pagelistend);
    perror("mprotect");
    ckptflags.checkpoint = FALSE;
    exit(-1);
  }

  mark_dirty(stop);
  mark_dirty(sys.DATASTART);
  /*-- mark 1st and last page spanned by page list to dirty -- if partial --*/
  if((uintptr_t)ckptglobals.page_dirty % PAGESIZE != 0){
    mark_dirty((caddr_t)ckptglobals.page_dirty);
  }
  if( ((uintptr_t)ckptglobals.page_dirty + (sizeof(char) * MAXPAGES + 1)) % PAGESIZE != 0){
    mark_dirty((caddr_t)((uintptr_t)ckptglobals.page_dirty + (sizeof(char) * MAXPAGES)));
  }
}

/**************************************************************
 * function: print_flags
 * args: none
 * returns: void
 * side effects: prints flags if verbose mode is set
 * called from: ckpt_setup()
 *************************************************************/

static void print_flags(){
  char *var[2];

  var[FALSE] = "off";
  var[TRUE] = "on";

  if(ckptflags.verbose){
    fprintf(stderr, "Printing Checkpointing Flags\n"
           "~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
           "  checkpointing    %s\n"
           "  incremental      %s\n"
           "  fork             %s\n"
           "  enhanced fork    %s\n"
           "  exclude          %s\n"
           "  verbose          %s\n"
           "  min time         %ld\n"
           "  max time         %ld\n"
           "  max files        %ld\n"
           "  directory        %s\n"
           "  filename         %s\n"
           "=== Printing Finished ===\n\n",
           var[ckptflags.checkpoint],
           var[ckptflags.incremental],
           var[ckptflags.fork],
           var[ckptflags.enhanced_fork],
           var[ckptflags.exclude],
           var[ckptflags.verbose],
           ckptflags.mintime,
           ckptflags.maxtime,
           ckptflags.maxfiles,
           ckptflags.dir,
           ckptflags.filename);
  }
}

/**************************************************************
 * function: set_alarm
 * args: none
 * returns: void
 * side effects: makes calls to signal and alarm to set up
 *               synchronous checkpointing.
 * called from: ckpt_setup()
 *************************************************************/

void set_alarm(){
  struct sigaction act;
  int i;

  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESTART;
  act.sa_handler = alrm_handler;
  sigaction(SIGALRM, &act, 0);

  if(ckptflags.mintime != 0){
    ckptglobals.enable = FALSE;
    alarm(ckptflags.mintime);
  }
  else {
    ckptglobals.enable = TRUE;
    if(ckptflags.maxtime != 0){
      alarm(ckptflags.maxtime);
    }
  }
}

/**************************************************************
 * function: mark_dirty
 * args: addr -- the faulting address
 * returns: void
 * side effects: marks the faulting page as dirty
 * misc: checks to see if page is really from heap, otherwise exits
 * called from: pg_faulthandler()
 *************************************************************/
void mark_dirty(caddr_t addr){
  int pn;

  if((uintptr_t)addr < (uintptr_t)sys.DATASTART || (uintptr_t)addr > (uintptr_t)(ckptglobals.sbrkptr)){
    fprintf(stderr, "Error: Trying to access an invalid address (0x%p)\n", addr);
    fprintf(stderr, "The data starts at 0x%p and ends at 0x%p\n",
                                         sys.DATASTART, ckptglobals.sbrkptr);
    raise(SIGQUIT);
  }

  pn = (addr - sys.DATASTART) / PAGESIZE;
  ckptglobals.page_dirty[pn] = (char)1;
}

/**************************************************************
 * function: force_dirty
 * args: none
 * returns: none
 * side effects: marks the pages containing the configuration and
 *               global data as dirty to ensure they are ckpt'd
 * called from: ckpt_setup()
 ************************************************************/
static void force_dirty()
{
  caddr_t addr;

  /*-- mark start page of sys struct --*/
  addr = (caddr_t)&sys;
  mark_dirty(addr);

  /*-- mark end page of sys struct --*/
  addr = (caddr_t)(sizeof(RESOURCES)+&sys);
  mark_dirty(addr);
}
