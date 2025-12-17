#ifndef EXEC_H
#define EXEC_H

#include "fs_defs.h"

enum exec_status {
    EXEC_SUCCESS,
    EXEC_ERROR_FT_DIRECTORY,
    EXEC_ERROR_INVALID_MAGIC,
};

enum exec_status exec(struct inode *prog, uint32_t *kernel_pgtbl);

#endif /* EXEC_H */
