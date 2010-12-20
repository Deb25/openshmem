#include <stdio.h>

#include <mpp/shmem.h>

int
main(int argc, char *argv[])
{
  int me, npes;
  long *x;

  start_pes(0);
  me = _my_pe();
  npes = _num_pes();

  x = (long *) shmalloc(8 * me);      /* deliberately pass different values */
  if (x == (long *) NULL) {
    fprintf(stderr, "%d/%d: %s\n", me, npes, sherror());
    return 1;
  }

  shfree(x);

  return 0;
}
