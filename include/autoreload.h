#pragma once

#include <stdbool.h>


typedef struct {
    int fd;
    int wd_dir;
    int wd_file;
    const char *filename;
} AutoreloadState;


void autoreload_init(AutoreloadState*);
void autoreload_cleanup(AutoreloadState*);
void autoreload_add(AutoreloadState*, const char* /* result of realpath(3) */);
bool autoreload_handle_events(AutoreloadState*);

