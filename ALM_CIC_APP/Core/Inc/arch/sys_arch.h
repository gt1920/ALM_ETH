/**
  * sys_arch.h - Minimal sys_arch for LwIP NO_SYS=1 (bare-metal)
  */
#ifndef __SYS_ARCH_H__
#define __SYS_ARCH_H__

/* NO_SYS=1: most sys types are not needed.
   Only sys_now() is required, which we implement in sys_arch.c */

#define SYS_MBOX_NULL   NULL
#define SYS_SEM_NULL    NULL

typedef int sys_sem_t;
typedef int sys_mbox_t;
typedef int sys_mutex_t;
typedef int sys_thread_t;

#endif /* __SYS_ARCH_H__ */
