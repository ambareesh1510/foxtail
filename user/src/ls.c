#include "syscall_defs.h"
#include "printf.h"
#include "string.h"
#include "malloc.h"

void _start(int argc, char **argv) {
    char *dir;
    if (argc == 1) {
        dir = ".";
    } else if (argc == 2) {
        dir = argv[1];
    } else {
        puts("Too many arguments\n");
        exit();
    }
    int cd_res = cd(dir);
    if (cd_res < 0) {
        puts("Couldn't find directory\n");
        exit();
    }
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
            printf("failed opening dirent file %s %d\n", my_dirent_info.name, dirent_fd);
            continue;
        }
        int dirent_ft = ftype(dirent_fd);
        if (dirent_ft < 0) {
            puts("failed getting dirent ft\n");
        }
        if (dirent_ft == SYS_FTYPE_FILE) {
            struct file_info my_dirent_file_info;
            file_info(dirent_fd, &my_dirent_file_info);
            printf("[FILE] %d %s\n", my_dirent_file_info.size, my_dirent_info.name);
        } else if (dirent_ft == SYS_FTYPE_DIR) {
            struct dir_info my_dirent_dir_info;
            dir_info(dirent_fd, &my_dirent_dir_info);
            printf("[DIR ] %d %s/\n", my_dirent_dir_info.num_entries, my_dirent_info.name);
        } else if (dirent_ft == SYS_FTYPE_SYMLINK) {
            unsigned int len = 100;
            char *buf = malloc(len);
            while (symlink_info(dirent_fd, buf, len) < 0) {
                len *= 2;
                buf = realloc(buf, len);
            }
            printf("[LINK] %s -> %s\n", my_dirent_info.name, buf);
            free(buf);
        }
        close(dirent_fd);
    }
    exit();
}
