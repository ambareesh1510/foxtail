// A basic Lisp implementation!
// (mostly courtesy of Claude, primarily for testing, will review/improve later)

#include "syscall_defs.h"
#include "malloc.h"
#include "printf.h"

#define MAX_SYMBOL_LEN 64
#define MAX_ENV_SIZE 256

// Lisp value types
enum lisp_type {
    LISP_INT,
    LISP_SYMBOL,
    LISP_CONS,
    LISP_FUNC,
    LISP_NIL
};

struct lisp_val;

// Function type
typedef struct lisp_val* (*builtin_fn)(struct lisp_val *args);

struct lisp_val {
    enum lisp_type type;
    union {
        int num;
        char *symbol;
        struct {
            struct lisp_val *car;
            struct lisp_val *cdr;
        } cons;
        struct {
            struct lisp_val *params;
            struct lisp_val *body;
            struct env *closure;
        } func;
        builtin_fn builtin;
    } data;
};

// Environment (symbol table)
struct env_entry {
    char *symbol;
    struct lisp_val *value;
};

struct env {
    struct env_entry entries[MAX_ENV_SIZE];
    int count;
    struct env *parent;
};

// Constructors
struct lisp_val* make_int(int n) {
    struct lisp_val *v = malloc(sizeof(struct lisp_val));
    v->type = LISP_INT;
    v->data.num = n;
    return v;
}

struct lisp_val* make_symbol(const char *s) {
    struct lisp_val *v = malloc(sizeof(struct lisp_val));
    v->type = LISP_SYMBOL;
    v->data.symbol = malloc(MAX_SYMBOL_LEN);
    int i = 0;
    while (s[i] && i < MAX_SYMBOL_LEN - 1) {
        v->data.symbol[i] = s[i];
        i++;
    }
    v->data.symbol[i] = '\0';
    return v;
}

struct lisp_val* make_cons(struct lisp_val *car, struct lisp_val *cdr) {
    struct lisp_val *v = malloc(sizeof(struct lisp_val));
    v->type = LISP_CONS;
    v->data.cons.car = car;
    v->data.cons.cdr = cdr;
    return v;
}

struct lisp_val* make_nil() {
    struct lisp_val *v = malloc(sizeof(struct lisp_val));
    v->type = LISP_NIL;
    return v;
}

// String utilities
int streq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

int isdigit(char c) {
    return c >= '0' && c <= '9';
}

int isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Environment operations
struct env* make_env(struct env *parent) {
    struct env *e = malloc(sizeof(struct env));
    e->count = 0;
    e->parent = parent;
    return e;
}

void env_set(struct env *e, const char *symbol, struct lisp_val *value) {
    // Update existing binding
    for (int i = 0; i < e->count; i++) {
        if (streq(e->entries[i].symbol, symbol)) {
            e->entries[i].value = value;
            return;
        }
    }
    // Add new binding
    if (e->count < MAX_ENV_SIZE) {
        e->entries[e->count].symbol = malloc(MAX_SYMBOL_LEN);
        int i = 0;
        while (symbol[i] && i < MAX_SYMBOL_LEN - 1) {
            e->entries[e->count].symbol[i] = symbol[i];
            i++;
        }
        e->entries[e->count].symbol[i] = '\0';
        e->entries[e->count].value = value;
        e->count++;
    }
}

struct lisp_val* env_get(struct env *e, const char *symbol) {
    for (int i = 0; i < e->count; i++) {
        if (streq(e->entries[i].symbol, symbol)) {
            return e->entries[i].value;
        }
    }
    if (e->parent) {
        return env_get(e->parent, symbol);
    }
    return make_nil();
}

// Parser
const char *skip_whitespace(const char *s) {
    while (*s && isspace(*s)) s++;
    return s;
}

struct lisp_val* parse_expr(const char **s);

struct lisp_val* parse_list(const char **s) {
    *s = skip_whitespace(*s);
    
    if (**s == ')') {
        (*s)++;
        return make_nil();
    }
    
    struct lisp_val *car = parse_expr(s);
    struct lisp_val *cdr = parse_list(s);
    return make_cons(car, cdr);
}

struct lisp_val* parse_expr(const char **s) {
    *s = skip_whitespace(*s);
    
    if (**s == '(') {
        (*s)++;
        return parse_list(s);
    }
    
    if (**s == '`' || isdigit(**s)) {
        int neg = 0;
        if (**s == '`') {
            neg = 1;
            (*s)++;
        }
        int num = 0;
        while (isdigit(**s)) {
            num = num * 10 + (**s - '0');
            (*s)++;
        }
        return make_int(neg ? -num : num);
    }
    
    // Parse symbol
    char buf[MAX_SYMBOL_LEN];
    int i = 0;
    while (**s && !isspace(**s) && **s != '(' && **s != ')' && i < MAX_SYMBOL_LEN - 1) {
        buf[i++] = **s;
        (*s)++;
    }
    buf[i] = '\0';
    return make_symbol(buf);
}

// Evaluator
struct lisp_val* eval(struct lisp_val *expr, struct env *e);

struct lisp_val* eval_list(struct lisp_val *args, struct env *e) {
    if (args->type == LISP_NIL) {
        return make_nil();
    }
    struct lisp_val *car = eval(args->data.cons.car, e);
    struct lisp_val *cdr = eval_list(args->data.cons.cdr, e);
    return make_cons(car, cdr);
}

int list_length(struct lisp_val *list) {
    int len = 0;
    while (list->type == LISP_CONS) {
        len++;
        list = list->data.cons.cdr;
    }
    return len;
}

struct lisp_val* builtin_add(struct lisp_val *args) {
    int sum = 0;
    while (args->type == LISP_CONS) {
        if (args->data.cons.car->type == LISP_INT) {
            sum += args->data.cons.car->data.num;
        }
        args = args->data.cons.cdr;
    }
    return make_int(sum);
}

struct lisp_val* builtin_sub(struct lisp_val *args) {
    if (args->type != LISP_CONS) return make_int(0);
    int result = args->data.cons.car->data.num;
    args = args->data.cons.cdr;
    while (args->type == LISP_CONS) {
        if (args->data.cons.car->type == LISP_INT) {
            result -= args->data.cons.car->data.num;
        }
        args = args->data.cons.cdr;
    }
    return make_int(result);
}

struct lisp_val* builtin_mul(struct lisp_val *args) {
    int prod = 1;
    while (args->type == LISP_CONS) {
        if (args->data.cons.car->type == LISP_INT) {
            prod *= args->data.cons.car->data.num;
        }
        args = args->data.cons.cdr;
    }
    return make_int(prod);
}

struct lisp_val* builtin_div(struct lisp_val *args) {
    if (args->type != LISP_CONS) return make_int(0);
    int result = args->data.cons.car->data.num;
    args = args->data.cons.cdr;
    while (args->type == LISP_CONS) {
        if (args->data.cons.car->type == LISP_INT && args->data.cons.car->data.num != 0) {
            result /= args->data.cons.car->data.num;
        }
        args = args->data.cons.cdr;
    }
    return make_int(result);
}

struct lisp_val* eval(struct lisp_val *expr, struct env *e) {
    if (expr->type == LISP_INT) {
        return expr;
    }
    
    if (expr->type == LISP_SYMBOL) {
        return env_get(e, expr->data.symbol);
    }
    
    if (expr->type == LISP_NIL) {
        return expr;
    }
    
    if (expr->type == LISP_CONS) {
        struct lisp_val *op = expr->data.cons.car;
        struct lisp_val *args = expr->data.cons.cdr;
        
        // Special forms
        if (op->type == LISP_SYMBOL && streq(op->data.symbol, "define")) {
            struct lisp_val *sym = args->data.cons.car;
            struct lisp_val *val = eval(args->data.cons.cdr->data.cons.car, e);
            env_set(e, sym->data.symbol, val);
            return val;
        }
        
        if (op->type == LISP_SYMBOL && streq(op->data.symbol, "lambda")) {
            struct lisp_val *func = malloc(sizeof(struct lisp_val));
            func->type = LISP_FUNC;
            func->data.func.params = args->data.cons.car;
            func->data.func.body = args->data.cons.cdr->data.cons.car;
            func->data.func.closure = e;
            return func;
        }
        
        // Evaluate operator and arguments
        struct lisp_val *func = eval(op, e);
        
        // Built-in functions
        if (func->type == LISP_SYMBOL) {
            struct lisp_val *evaled_args = eval_list(args, e);
            if (streq(func->data.symbol, "+")) return builtin_add(evaled_args);
            if (streq(func->data.symbol, "-")) return builtin_sub(evaled_args);
            if (streq(func->data.symbol, "*")) return builtin_mul(evaled_args);
            if (streq(func->data.symbol, "/")) return builtin_div(evaled_args);
        }
        
        // User-defined function
        if (func->type == LISP_FUNC) {
            struct env *new_env = make_env(func->data.func.closure);
            struct lisp_val *params = func->data.func.params;
            struct lisp_val *arg_vals = eval_list(args, e);
            
            while (params->type == LISP_CONS && arg_vals->type == LISP_CONS) {
                env_set(new_env, params->data.cons.car->data.symbol, arg_vals->data.cons.car);
                params = params->data.cons.cdr;
                arg_vals = arg_vals->data.cons.cdr;
            }
            
            return eval(func->data.func.body, new_env);
        }
    }
    
    return make_nil();
}

// Printer
void print_val(struct lisp_val *v) {
    if (v->type == LISP_INT) {
        printf("%d", v->data.num);
    } else if (v->type == LISP_SYMBOL) {
        printf("%s", v->data.symbol);
    } else if (v->type == LISP_NIL) {
        printf("nil");
    } else if (v->type == LISP_CONS) {
        printf("(");
        while (v->type == LISP_CONS) {
            print_val(v->data.cons.car);
            v = v->data.cons.cdr;
            if (v->type == LISP_CONS) printf(" ");
        }
        if (v->type != LISP_NIL) {
            printf(" . ");
            print_val(v);
        }
        printf(")");
    } else if (v->type == LISP_FUNC) {
        printf("<function>");
    }
}

// REPL
void _start() {
    printf("Simple Lisp Interpreter\n");
    printf("Examples:\n");
    printf("  (+ 1 2 3)\n");
    printf("  (define x 10)\n");
    printf("  (define square (lambda (n) (* n n)))\n");
    printf("  (square 5)\n\n");
    
    struct env *global_env = make_env(0);
    
    // Add built-ins to environment
    env_set(global_env, "+", make_symbol("+"));
    env_set(global_env, "-", make_symbol("-"));
    env_set(global_env, "*", make_symbol("*"));
    env_set(global_env, "/", make_symbol("/"));
    
    char input[256];
    while (1) {
        printf("lisp> ");
        
        int n = read(1, input, 255);
        if (n <= 0) continue;
        input[n] = '\0';
        
        // Remove newline
        for (int i = 0; i < n; i++) {
            if (input[i] == '\n') input[i] = '\0';
        }
        
        if (streq(input, "exit")) break;
        
        const char *ptr = input;
        struct lisp_val *expr = parse_expr(&ptr);
        struct lisp_val *result = eval(expr, global_env);
        
        print_val(result);
        printf("\n");
    }
    
    exit(0);
}
