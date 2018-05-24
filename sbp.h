#ifndef __SBP_H
#define __SBP_H

#include <stddef.h>

typedef struct SBP {
    char *buf;
    size_t size;
    size_t alloc_size;
} *sbp_t;

sbp_t sbp_create();
void sbp_destroy(sbp_t sbp);
void sbp_add_code(sbp_t sbp, char code);
void sbp_add_string(sbp_t sbp, char code, int length_width, char *str);
void sbp_add_numeric(sbp_t sbp, char code, unsigned long n);

#endif
