#include "libckpt.h"
#include "copyright.h"

int main(int argc, char ** argv, char ** envp)
{
  ckptglobals.lang = FALSE;

  ckpt_setup(&argc, argv);

  return ckpt_target(argc, argv, envp);
}
