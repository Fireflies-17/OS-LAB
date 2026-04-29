#include "types.h"
#include "stat.h"
#include "user.h"
#include "memlayout.h"

void handler(void);
void badstack(uint);

int
main(int argc, char *argv[])
{
  printf(1, "alarmattack starting\n");
  printf(1, "safe kernels should kill this process\n");
  alarm(1, handler);
  badstack(KERNBASE + 4);
  exit();
}

void
handler(void)
{
  printf(1, "handler should not run on a safe kernel\n");
}

void
badstack(uint sp)
{
  asm volatile(
    "movl %0, %%esp\n"
    "1: jmp 1b\n"
    :
    : "r"(sp));
}
