#include "sbp.h"
#include "ruby.h"
#include <ibase.h>
#include <string.h>

sbp_t sbp_create()
{
    sbp_t result = ALLOC(struct SBP);
    result->alloc_size = 64;
    result->size = 0;
    result->buf = ALLOC_N(char, result->alloc_size);
    return result;
}

void sbp_destroy(sbp_t sbp)
{
    xfree(sbp->buf);
    xfree(sbp);
}

void sbp_grow(sbp_t sbp, size_t add_size)
{
    if(sbp->size + add_size > sbp->alloc_size) {
        sbp->alloc_size += (add_size / 64 + 1) * 64;
        REALLOC_N(sbp->buf, char, sbp->alloc_size);
    }
}

void sbp_add_code(sbp_t sbp, char code)
{
    sbp_grow(sbp, 1);
    char *w_ptr = sbp->buf + sbp->size;
    *w_ptr = code;
    sbp->size++;
}

void sbp_add_string(sbp_t sbp, char code, int length_width, char *str)
{
    int len = strlen(str);
    sbp_grow(sbp, 1 + length_width + len);
    char *w_ptr = sbp->buf + sbp->size;
    *w_ptr++ = code;
    *w_ptr++ = (ISC_SCHAR) (ISC_UCHAR) len;
    if (length_width == 2) {
        *w_ptr++ = (ISC_SCHAR) (ISC_UCHAR) (len >> 8);;
    }
    strncpy(w_ptr, str, len);
    sbp->size += 1 + length_width + len;
}

void sbp_add_numeric(sbp_t sbp, char code, unsigned long n)
{
    sbp_grow(sbp, 5);
    char *w_ptr = sbp->buf + sbp->size;
    *w_ptr++ = code;
    ADD_SPB_NUMERIC(w_ptr, n);
    sbp->size += 5;
}
