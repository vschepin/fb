#ifndef __UTILS_H
#define __UTILS_H

#include "ruby.h"
#include <ibase.h>

extern VALUE rb_eFbError;

void init_utils(VALUE fb_module);
VALUE default_string(VALUE hash, const char *key, const char *def);
VALUE default_int(VALUE hash, const char *key, int def);
void fb_error_check(ISC_STATUS *isc_status);
void fb_error_check_warn(ISC_STATUS *isc_status);
#if (FB_API_VER >= 20)
VALUE fb_error_msg(const ISC_STATUS *isc_status);
#else
VALUE fb_error_msg(ISC_STATUS *isc_status);
#endif
VALUE hash_from_connection_string(VALUE cs);

#endif