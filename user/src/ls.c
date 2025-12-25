#include "syscall_defs.h"
#include "printf.h"
#include "string.h"
#include "malloc.h"

void _start() {
    int cwd_fd = open(".", 0);
    if (cwd_fd < 0) {
        puts("open failed\n");
        exit();
    }

    int ft = ftype(cwd_fd);
    if (ft != SYS_FTYPE_DIR) {
        puts("not a dir\n");
        exit();
    }

    struct dir_info my_dir_info;
    int dir_info_res = dir_info(cwd_fd, &my_dir_info);
    if (dir_info_res < 0) {
        puts("failed getting dir_info\n");
        exit();
    }

    struct dirent_info my_dirent_info = {0};
    while (my_dir_info.num_entries--) {
        int dirent_info_res = dirent_info(cwd_fd, &my_dirent_info);
        if (dirent_info_res < 0) {
            puts("failed getting dirent info\n");
            continue;
        }
        int dirent_fd = open(my_dirent_info.name, 0);
        if (dirent_fd < 0) {
            puts("failed opening dirent file\n");
            continue;
        }
        int dirent_ft = ftype(dirent_fd);
        if (dirent_ft < 0) {
            puts("failed getting dirent ft\n");
        }
        if (dirent_ft == SYS_FTYPE_FILE) {
            struct file_info my_dirent_file_info;
            file_info(dirent_fd, &my_dirent_file_info);
            printf("[F] %d %s\n", my_dirent_file_info.size, my_dirent_info.name);
        } else if (dirent_ft == SYS_FTYPE_DIR) {
            struct dir_info my_dirent_dir_info;
            dir_info(dirent_fd, &my_dirent_dir_info);
            printf("[D] %d %s/\n", my_dirent_dir_info.num_entries, my_dirent_info.name);
        }
    }
    exit();
}
