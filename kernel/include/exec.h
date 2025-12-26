#ifndef EXEC_H
#define EXEC_H

#include "fs_defs.h"
#include "syscall_defs.h"

enum exec_status {
    EXEC_SUCCESS,
    EXEC_ERROR_FT_DIRECTORY,
    EXEC_ERROR_INVALID_MAGIC,
};

struct proc *exec_helper(struct inode *prog_ptr, u32 argc, char **argv, u32 num_custom_commands, struct spawn_custom_command *commands);
enum exec_status exec(struct inode *prog);

#endif /* EXEC_H */
