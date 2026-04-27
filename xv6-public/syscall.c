#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

// User code makes a system call with INT T_SYSCALL.
// System call number in %eax.
// Arguments on the stack, from the user call to the C
// library system call function. The saved user %esp points
// to a saved program counter, and then the first argument.

// Fetch the int at addr from the current process.
int
fetchint(uint addr, int *ip)
{
  struct proc *curproc = myproc();

  if(addr >= curproc->sz || addr+4 > curproc->sz)
    return -1;
  *ip = *(int*)(addr);
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.
int
fetchstr(uint addr, char **pp)
{
  char *s, *ep;
  struct proc *curproc = myproc();

  if(addr >= curproc->sz)
    return -1;
  *pp = (char*)addr;
  ep = (char*)curproc->sz;
  for(s = *pp; s < ep; s++){
    if(*s == 0)
      return s - *pp;
  }
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  return fetchint((myproc()->tf->esp) + 4 + 4*n, ip);
}

// Fetch the nth word-sized system call argument as a pointer
// to a block of memory of size bytes.  Check that the pointer
// lies within the process address space.
int
argptr(int n, char **pp, int size)
{
  int i;
  struct proc *curproc = myproc();
 
  if(argint(n, &i) < 0)
    return -1;
  if(size < 0 || (uint)i >= curproc->sz || (uint)i+size > curproc->sz)
    return -1;
  *pp = (char*)i;
  return 0;
}

// Fetch the nth word-sized system call argument as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)
int
argstr(int n, char **pp)
{
  int addr;
  if(argint(n, &addr) < 0)
    return -1;
  return fetchstr(addr, pp);
}

extern int sys_chdir(void);
extern int sys_close(void);
extern int sys_dup(void);
extern int sys_exec(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_fstat(void);
extern int sys_date(void);
extern int sys_getpid(void);
extern int sys_kill(void);
extern int sys_link(void);
extern int sys_mkdir(void);
extern int sys_mknod(void);
extern int sys_open(void);
extern int sys_pipe(void);
extern int sys_read(void);
extern int sys_sbrk(void);
extern int sys_sleep(void);
extern int sys_unlink(void);
extern int sys_wait(void);
extern int sys_write(void);
extern int sys_uptime(void);

static int (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_date]    sys_date,
};

#define SYSARG_INT 0
#define SYSARG_PTR 1
#define SYSARG_STR 2
#define MAXSYSARGS 3
#define MAXSYSARGSTR 32

struct syscallinfo {
  char *name;
  int nargs;
  int argtypes[MAXSYSARGS];
};

static struct syscallinfo syscallinfo[] = {
[SYS_fork]    { "fork",   0, { 0, 0, 0 } },
[SYS_exit]    { "exit",   0, { 0, 0, 0 } },
[SYS_wait]    { "wait",   0, { 0, 0, 0 } },
[SYS_pipe]    { "pipe",   1, { SYSARG_PTR, 0, 0 } },
[SYS_read]    { "read",   3, { SYSARG_INT, SYSARG_PTR, SYSARG_INT } },
[SYS_kill]    { "kill",   1, { SYSARG_INT, 0, 0 } },
[SYS_exec]    { "exec",   2, { SYSARG_STR, SYSARG_PTR, 0 } },
[SYS_fstat]   { "fstat",  2, { SYSARG_INT, SYSARG_PTR, 0 } },
[SYS_chdir]   { "chdir",  1, { SYSARG_STR, 0, 0 } },
[SYS_dup]     { "dup",    1, { SYSARG_INT, 0, 0 } },
[SYS_getpid]  { "getpid", 0, { 0, 0, 0 } },
[SYS_sbrk]    { "sbrk",   1, { SYSARG_INT, 0, 0 } },
[SYS_sleep]   { "sleep",  1, { SYSARG_INT, 0, 0 } },
[SYS_uptime]  { "uptime", 0, { 0, 0, 0 } },
[SYS_open]    { "open",   2, { SYSARG_STR, SYSARG_INT, 0 } },
[SYS_write]   { "write",  3, { SYSARG_INT, SYSARG_PTR, SYSARG_INT } },
[SYS_mknod]   { "mknod",  3, { SYSARG_STR, SYSARG_INT, SYSARG_INT } },
[SYS_unlink]  { "unlink", 1, { SYSARG_STR, 0, 0 } },
[SYS_link]    { "link",   2, { SYSARG_STR, SYSARG_STR, 0 } },
[SYS_mkdir]   { "mkdir",  1, { SYSARG_STR, 0, 0 } },
[SYS_close]   { "close",  1, { SYSARG_INT, 0, 0 } },
[SYS_date]    { "date",   1, { SYSARG_PTR, 0, 0 } },
};

static void
copyargstr(char *dst, char *src)
{
  int i;

  for(i = 0; i < MAXSYSARGSTR-1 && src[i]; i++)
    dst[i] = src[i];
  dst[i] = 0;
}

static void
collectsysargs(int num, int args[], char argstrs[][MAXSYSARGSTR])
{
  int i;
  char *s;

  for(i = 0; i < syscallinfo[num].nargs; i++){
    argstrs[i][0] = 0;
    if(argint(i, &args[i]) < 0){
      args[i] = 0;
      if(syscallinfo[num].argtypes[i] == SYSARG_STR)
        copyargstr(argstrs[i], "<bad>");
      continue;
    }
    if(syscallinfo[num].argtypes[i] == SYSARG_STR){
      if(fetchstr(args[i], &s) < 0)
        copyargstr(argstrs[i], "<bad>");
      else
        copyargstr(argstrs[i], s);
    }
  }
}

static void
printsyscall(int num, int args[], char argstrs[][MAXSYSARGSTR], int ret)
{
  int i;

  cprintf("%s", syscallinfo[num].name);
  if(syscallinfo[num].nargs > 0){
    cprintf("(");
    for(i = 0; i < syscallinfo[num].nargs; i++){
      if(i > 0)
        cprintf(", ");
      switch(syscallinfo[num].argtypes[i]){
      case SYSARG_STR:
        cprintf("\"%s\"", argstrs[i]);
        break;
      case SYSARG_PTR:
        cprintf("0x%x", args[i]);
        break;
      default:
        cprintf("%d", args[i]);
        break;
      }
    }
    cprintf(")");
  }
  cprintf(" -> %d\n", ret);
}

void
syscall(void)
{
  int num;
  int args[MAXSYSARGS];
  char argstrs[MAXSYSARGS][MAXSYSARGSTR];
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    collectsysargs(num, args, argstrs);
    curproc->tf->eax = syscalls[num]();
    printsyscall(num, args, argstrs, curproc->tf->eax);
  } else {
    cprintf("%d %s: unknown sys call %d\n",
            curproc->pid, curproc->name, num);
    curproc->tf->eax = -1;
  }
}
