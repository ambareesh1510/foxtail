#include "syscall_defs.h"
#include "malloc.h"
#include "printf.h"

#define MAX_LINE_LEN 256

struct line {
    char *text;
    struct line *next;
    struct line *prev;
};

struct editor {
    struct line *first;
    struct line *current;
    struct line *last;
    int current_line_idx;
    int total_lines;
    char *filename;
    int modified;
};

char *strdup(const char *s) {
    int len = 0;
    while (s[len]) len++;
    char *copy = malloc(len + 1);
    for (int i = 0; i <= len; i++) {
        copy[i] = s[i];
    }
    return copy;
}

int atoi(const char *s) {
    int num = 0;
    int i = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        num = num * 10 + (s[i] - '0');
        i++;
    }
    return num;
}

struct line *make_line(const char *text) {
    struct line *l = malloc(sizeof(struct line));
    l->text = strdup(text);
    l->next = 0;
    l->prev = 0;
    return l;
}

// Initialize editor
struct editor *make_editor(const char *filename) {
    struct editor *ed = malloc(sizeof(struct editor));
    ed->first = 0;
    ed->current = 0;
    ed->last = 0;
    ed->current_line_idx = 0;
    ed->total_lines = 0;
    ed->filename = strdup(filename);
    ed->modified = 0;
    return ed;
}

bool load_file(struct editor *ed) {
    int fd = open(ed->filename, SYS_OPEN_FILE_MODE_READ);
    if (fd < 0) {
        printf("File doesn't exist: %s\n", ed->filename);
        return false;
    }
    
    char buffer[MAX_LINE_LEN];
    int line_len = 0;
    
    while (1) {
        char c;
        int n = read(fd, &c, 1);
        if (n <= 0) break;
        
        if (c == '\n' || line_len >= MAX_LINE_LEN - 1) {
            buffer[line_len] = '\0';
            struct line *l = make_line(buffer);
            
            if (!ed->first) {
                ed->first = l;
                ed->current = l;
                ed->last = l;
            } else {
                ed->last->next = l;
                l->prev = ed->last;
                ed->last = l;
            }
            
            ed->total_lines++;
            line_len = 0;
        } else {
            buffer[line_len++] = c;
        }
    }
    
    // Handle last line without newline
    if (line_len > 0) {
        buffer[line_len] = '\0';
        struct line *l = make_line(buffer);
        if (!ed->first) {
            ed->first = l;
            ed->current = l;
            ed->last = l;
        } else {
            ed->last->next = l;
            l->prev = ed->last;
            ed->last = l;
        }
        ed->total_lines++;
    }
    
    close(fd);
    ed->current_line_idx = 1;
    printf("Loaded %d lines from %s\n", ed->total_lines, ed->filename);
    return true;
}

void save_file(struct editor *ed) {
    int fd = open(ed->filename, SYS_OPEN_FILE_MODE_WRITE);
    if (fd < 0) {
        printf("Error: Cannot open file for writing\n");
        return;
    }
    
    struct line *l = ed->first;
    while (l) {
        write(fd, l->text, strlen(l->text));
        write(fd, "\n", 1);
        l = l->next;
    }
    
    close(fd);
    ed->modified = 0;
    printf("Saved %d lines to %s\n", ed->total_lines, ed->filename);
}

// TODO: right-align line numbers when printing
void print_lines(struct editor *ed, int count) {
    if (!ed->current) {
        printf("No lines to print\n");
        return;
    }
    
    struct line *l = ed->current;
    int line_num = ed->current_line_idx;
    
    for (int i = 0; i < count && l; i++) {
        printf("%d: %s\n", line_num, l->text);
        l = l->next;
        line_num++;
    }
}

void goto_line(struct editor *ed, int target) {
    if (target < 1 || target > ed->total_lines) {
        printf("Error: Line %d out of range (1-%d)\n", target, ed->total_lines);
        return;
    }
    
    if (target < ed->current_line_idx / 2) {
        ed->current = ed->first;
        ed->current_line_idx = 1;
    }
    
    while (ed->current_line_idx < target) {
        ed->current = ed->current->next;
        ed->current_line_idx++;
    }
    
    while (ed->current_line_idx > target) {
        ed->current = ed->current->prev;
        ed->current_line_idx--;
    }
    
    printf("At line %d: %s\n", ed->current_line_idx, ed->current->text);
}

void insert_after(struct editor *ed, const char *text) {
    struct line *new_line = make_line(text);
    
    if (!ed->first) {
        ed->first = new_line;
        ed->current = new_line;
        ed->last = new_line;
        ed->current_line_idx = 1;
    } else {
        new_line->prev = ed->current;
        new_line->next = ed->current->next;
        
        if (ed->current->next) {
            ed->current->next->prev = new_line;
        } else {
            ed->last = new_line;
        }
        
        ed->current->next = new_line;
        ed->current = new_line;
        ed->current_line_idx++;
    }
    
    ed->total_lines++;
    ed->modified = 1;
}

void insert_before(struct editor *ed, const char *text) {
    struct line *new_line = make_line(text);
    
    if (!ed->first) {
        ed->first = new_line;
        ed->current = new_line;
        ed->last = new_line;
        ed->current_line_idx = 1;
    } else {
        new_line->next = ed->current;
        new_line->prev = ed->current->prev;
        
        if (ed->current->prev) {
            ed->current->prev->next = new_line;
        } else {
            ed->first = new_line;
        }
        
        ed->current->prev = new_line;
        ed->current = new_line;
    }
    
    ed->total_lines++;
    ed->modified = 1;
}

void replace_line(struct editor *ed, const char *text) {
    if (!ed->current) {
        printf("Error: No current line\n");
        return;
    }
    
    free(ed->current->text);
    ed->current->text = strdup(text);
    ed->modified = 1;
    printf("Replaced line %d\n", ed->current_line_idx);
}

void delete_line(struct editor *ed) {
    if (!ed->current) return;
    
    struct line *to_delete = ed->current;
    
    if (to_delete->prev) to_delete->prev->next = to_delete->next;
    else ed->first = to_delete->next;
    
    if (to_delete->next) to_delete->next->prev = to_delete->prev;
    else ed->last = to_delete->prev;
    
    ed->current = to_delete->next ? to_delete->next : to_delete->prev;
    
    free(to_delete->text);
    free(to_delete);
    ed->total_lines--;
    ed->modified = 1;
}

int read_line(char *buffer, int max_len) {
    int n = read(1, buffer, max_len - 1);
    if (n > 0) {
        buffer[n] = '\0';
        if (n > 0 && buffer[n-1] == '\n') {
            buffer[n-1] = '\0';
            n--;
        }
    }
    return n;
}

void _start(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: ed <filename>\n");
        exit(1);
    }
    
    struct editor *ed = make_editor(argv[1]);
    bool load_res = load_file(ed);
    if (!load_res) {
        exit(1);
    }
    
    char input[MAX_LINE_LEN];
    
    puts("\nCommands:\n");
    puts("  g <n>     - goto line n\n");
    puts("  p [n]     - print n lines (default 1)\n");
    puts("  i         - insert lines before current\n");
    puts("  a         - insert lines after current\n");
    puts("  r         - replace current line\n");
    puts("  w         - write (save) file\n");
    puts("  q         - quit\n\n");
    
    while (1) {
        printf("ed> ");
        int len = read_line(input, MAX_LINE_LEN);
        if (len <= 0) continue;
        
        // Parse command
        if (input[0] == 'q') {
            if (ed->modified) {
                printf("Warning: unsaved changes. Use 'w' to save or 'q!' to quit anyway\n");
                if (len > 1 && input[1] == '!') {
                    break;
                }
            } else {
                break;
            }
        } else if (input[0] == 'w') {
            save_file(ed);
        } else if (input[0] == 'g') {
            int line_num = atoi(input + 2);
            goto_line(ed, line_num);
        } else if (input[0] == 'p') {
            int count = 1;
            if (len > 2) {
                count = atoi(input + 2);
            }
            print_lines(ed, count);
        } else if (input[0] == 'a') {
            printf("Enter lines (empty line to finish):\n");
            while (1) {
                printf("+ ");
                len = read_line(input, MAX_LINE_LEN);
                if (len == 0) break;
                insert_after(ed, input);
            }
        } else if (input[0] == 'i') {
            printf("Enter lines (empty line to finish):\n");
            while (1) {
                printf("+ ");
                len = read_line(input, MAX_LINE_LEN);
                if (len == 0) break;
                insert_before(ed, input);
            }
        } else if (input[0] == 'r') {
            printf("New text: ");
            len = read_line(input, MAX_LINE_LEN);
            replace_line(ed, input);
        } else if (input[0] == 'd') {
            delete_line(ed);
            printf("Deleted current line\n");
        } else {
            printf("Unknown command: %c\n", input[0]);
        }
    }
    
    printf("Goodbye!\n");
    exit(0);
}
