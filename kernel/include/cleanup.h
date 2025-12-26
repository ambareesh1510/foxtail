#ifndef CLEANUP_H
#define CLEANUP_H

#include "proc.h"

#define FAULT_EXIT_CODE 128

void cleanup_proc(struct proc *proc);

#endif /* CLEANUP_H */
