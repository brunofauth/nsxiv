#pragma once

#include <dirent.h>
#include <stdbool.h>
#include <sys/types.h>


typedef struct {
    DIR *dir;
    char *name;
    int d;
    bool recursive;

    char **stack;
    int stcap;
    int stlen;
} r_dir_t;

extern const char *progname;


// util.c {{{
void* emalloc(size_t);
void* ecalloc(size_t, size_t);
void* erealloc(void*, size_t);
char* estrdup(const char*);
void error(int, int, const char*, ...);
int r_opendir(r_dir_t*, const char dirname[], bool recursive) __attribute__((nonnull (1, 2)));
int r_closedir(r_dir_t*)                                      __attribute__((nonnull (1)));
char* r_readdir(r_dir_t*, bool skip_dotfiles)                 __attribute__((nonnull (1)));
int r_mkdir(char*)                                            __attribute__((nonnull (1)));
void construct_argv(char**, unsigned int, ...);
pid_t spawn(int*, int*, int, char *const []);
// }}}
