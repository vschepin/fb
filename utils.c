#include "utils.h"

#ifdef HAVE_RUBY_REGEX_H
#  include "ruby/re.h"
#else
#  include "re.h"
#endif

VALUE rb_eFbError;

/* call-seq:
 *   error_code -> int
 *
 * Returns the sqlcode associated with the error.
 */
static VALUE error_error_code(VALUE error)
{
	rb_p(error);
	return rb_iv_get(error, "error_code");
}

VALUE default_string(VALUE hash, const char *key, const char *def)
{
	VALUE sym = ID2SYM(rb_intern(key));
	VALUE val = rb_hash_aref(hash, sym);
	return NIL_P(val) ? rb_str_new2(def) : StringValue(val);
}

VALUE default_int(VALUE hash, const char *key, int def)
{
	VALUE sym = ID2SYM(rb_intern(key));
	VALUE val = rb_hash_aref(hash, sym);
	return NIL_P(val) ? INT2NUM(def) : val;
}

void fb_error_check(ISC_STATUS *isc_status)
{
	if (isc_status[0] == 1 && isc_status[1]) {
		char buf[1024];
		VALUE exc, msg, msg1, msg2;
		short code = isc_sqlcode(isc_status);

		isc_sql_interprete(code, buf, 1024);
		msg1 = rb_str_new2(buf);
		msg2 = fb_error_msg(isc_status);
		msg = rb_str_cat(msg1, "\n", strlen("\n"));
		msg = rb_str_concat(msg, msg2);

		exc = rb_exc_new3(rb_eFbError, msg);
		rb_iv_set(exc, "error_code", INT2FIX(code));
		rb_exc_raise(exc);
	}
}

void fb_error_check_warn(ISC_STATUS *isc_status)
{
	short code = isc_sqlcode(isc_status);
	if (code != 0) {
		char buf[1024];
		isc_sql_interprete(code, buf, 1024);
		rb_warning("%s(%d)", buf, code);
	}
}

#if (FB_API_VER >= 20)
VALUE fb_error_msg(const ISC_STATUS *isc_status)
{
	char msg[1024];
	VALUE result = rb_str_new(NULL, 0);
	while (fb_interpret(msg, 1024, &isc_status))
	{
		result = rb_str_cat(result, msg, strlen(msg));
		result = rb_str_cat(result, "\n", strlen("\n"));
	}
	return result;
}
#else
VALUE fb_error_msg(ISC_STATUS *isc_status)
{
	char msg[1024];
	VALUE result = rb_str_new(NULL, 0);
	while (isc_interprete(msg, &isc_status))
	{
		result = rb_str_cat(result, msg, strlen(msg));
		result = rb_str_cat(result, "\n", strlen("\n"));
	}
	return result;
}
#endif

VALUE hash_from_connection_string(VALUE cs)
{
	VALUE hash = rb_hash_new();
	VALUE re_SemiColon = rb_reg_regcomp(rb_str_new2("\\s*;\\s*"));
	VALUE re_Equal = rb_reg_regcomp(rb_str_new2("\\s*=\\s*"));
	ID id_split = rb_intern("split");
	VALUE pairs = rb_funcall(cs, id_split, 1, re_SemiColon);
	int i;
	for (i = 0; i < RARRAY_LEN(pairs); i++) {
		VALUE pair = rb_ary_entry(pairs, i);
		VALUE keyValue = rb_funcall(pair, id_split, 1, re_Equal);
		if (RARRAY_LEN(keyValue) == 2) {
			VALUE key = rb_ary_entry(keyValue, 0);
			VALUE val = rb_ary_entry(keyValue, 1);
			rb_hash_aset(hash, rb_str_intern(key), val);
		}
	}
	return hash;
}

void init_utils(VALUE rb_mFb)
{
    rb_eFbError = rb_define_class_under(rb_mFb, "Error", rb_eStandardError);
    rb_define_method(rb_eFbError, "error_code", error_error_code, 0);
}
