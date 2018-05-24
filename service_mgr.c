#include "service_mgr.h"
#include "sbp.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ibase.h>
#include <float.h>
#include <time.h>
#include <stdbool.h>


static ID id_force_encoding;
static VALUE rb_cFbHost;
static VALUE rb_cFbServiceConnection;
static VALUE rb_cFbUser;


VALUE user_allocate_instance(VALUE klass)
{
	NEWOBJ(obj, struct RObject);
	OBJSETUP((VALUE)obj, klass, T_OBJECT);
	return (VALUE)obj;
}

struct FbServiceConnection {
	isc_svc_handle handle;
	ISC_STATUS isc_status[20];
};

static void fb_svc_connection_check(struct FbServiceConnection *svc_connection)
{
	if (svc_connection->handle == 0) {
		rb_raise(rb_eFbError, "closed host connection");
	}
}

static void fb_svc_connection_disconnect(struct FbServiceConnection *svc_connection)
{
    isc_service_detach(svc_connection->isc_status, &svc_connection->handle);
	fb_error_check(svc_connection->isc_status);
}

static void fb_svc_connection_disconnect_warn(struct FbServiceConnection *svc_connection)
{
	isc_service_detach(svc_connection->isc_status, &svc_connection->handle);
	fb_error_check_warn(svc_connection->isc_status);
}

static void fb_svc_connection_mark(struct FbServiceConnection *svc_connection)
{
}

static void fb_svc_connection_free(struct FbServiceConnection *svc_connection)
{
	if (svc_connection->handle) {
		fb_svc_connection_disconnect_warn(svc_connection);
	}
	xfree(svc_connection);
}

static const char* SERVICE_CONNECTION_PARMS[] = {
	"@host",
	"@username",
	"@password",
	(char *)0
};

static VALUE service_connection_create(isc_svc_handle handle, VALUE db)
{
	const char *parm;
	int i;
	struct FbServiceConnection *fb_connection;
	VALUE connection = Data_Make_Struct(rb_cFbServiceConnection, struct FbServiceConnection, fb_svc_connection_mark, fb_svc_connection_free, fb_connection);
	fb_connection->handle = handle;

	for (i = 0; (parm = SERVICE_CONNECTION_PARMS[i]); i++) {
		rb_iv_set(connection, parm, rb_iv_get(db, parm));
		VALUE v = rb_iv_get(db, parm);
	}

	return connection;
}

/* call-seq:
 *   close() -> nil
 *
 * Closes connection to host.
 */
static VALUE service_connection_close(VALUE self)
{
	struct FbServiceConnection *svc_connection;

	Data_Get_Struct(self, struct FbServiceConnection, svc_connection);

	fb_svc_connection_check(svc_connection);
	fb_svc_connection_disconnect(svc_connection);

	return Qnil;
}

static VALUE service_connection_users(VALUE self)
{
    struct FbServiceConnection *svc_connection;

	Data_Get_Struct(self, struct FbServiceConnection, svc_connection);

	fb_svc_connection_check(svc_connection);

    sbp_t sbp = sbp_create();
    sbp_add_code(sbp, isc_action_svc_display_user);
    isc_service_start(svc_connection->isc_status, &svc_connection->handle, 0, sbp->size, sbp->buf);
    sbp_destroy(sbp);

    fb_error_check(svc_connection->isc_status);


    char request_buffer[] = {isc_info_svc_get_users};

    const size_t result_buffer_size = 65535;
    char *result_buffer = ALLOC_N(char, result_buffer_size);

    sbp = sbp_create();
    sbp_add_numeric(sbp, isc_info_svc_timeout, 60);

    isc_service_query(svc_connection->isc_status, &svc_connection->handle, 0, sbp->size, sbp->buf,
                      sizeof(request_buffer), request_buffer, result_buffer_size, result_buffer);
    sbp_destroy(sbp);

    fb_error_check(svc_connection->isc_status);

    char *p = result_buffer;
    VALUE result = Qnil;
    if (*p++ == isc_info_svc_get_users) {
        ISC_USHORT info_len = (ISC_USHORT) isc_vax_integer(p, sizeof(ISC_USHORT));
        p += sizeof(ISC_USHORT);
        result = rb_ary_new();
        VALUE user;

        char type;
        while(info_len > 0 && ((type = *p++) != isc_info_end)) {
            VALUE v;
            info_len--;
            if(type == isc_spb_sec_userid || type == isc_spb_sec_groupid){
                ISC_ULONG id = (ISC_ULONG) isc_vax_integer(p, sizeof(ISC_ULONG));
                p += sizeof(ISC_ULONG);
                info_len -= sizeof(ISC_ULONG);
                v = UINT2NUM(id);
            }else if(type == isc_spb_sec_username || type == isc_spb_sec_password || type == isc_spb_sec_firstname || type == isc_spb_sec_middlename || type == isc_spb_sec_lastname) {
                ISC_USHORT len = (ISC_USHORT) isc_vax_integer(p, sizeof(ISC_USHORT));
                p += sizeof(ISC_USHORT);
                info_len -= sizeof(ISC_USHORT);
                v = rb_str_new(p, len);
                #if HAVE_RUBY_ENCODING_H
                rb_funcall(v, id_force_encoding, 1, rb_str_new2("utf-8"));
                #endif
                p += len;
                info_len -= len;
            }else{
                rb_bug("unknown result attribute %d", type);
                break;
            }
            switch(type){
                case isc_spb_sec_username:
                    user = user_allocate_instance(rb_cFbUser);
                    rb_ary_push(result, user);
                    rb_iv_set(user, "@name", v);
                    break;
                case isc_spb_sec_password:
                    rb_iv_set(user, "@password", v);
                    break;
                case isc_spb_sec_firstname:
                    rb_iv_set(user, "@first_name", v);
                    break;
                case isc_spb_sec_middlename:
                    rb_iv_set(user, "@middle_name", v);
                    break;
                case isc_spb_sec_lastname:
                    rb_iv_set(user, "@last_name", v);
                    break;
                case isc_spb_sec_userid:
                    rb_iv_set(user, "@id", v);
                    break;
                case isc_spb_sec_groupid:
                    rb_iv_set(user, "@group_id", v);
                    break;
            }
        }
    }
    xfree(result_buffer);
    return result;
}

static VALUE host_allocate_instance(VALUE klass)
{
	NEWOBJ(obj, struct RObject);
	OBJSETUP((VALUE)obj, klass, T_OBJECT);
	return (VALUE)obj;
}

/* call-seq:
 *   Host.new(options) -> Host
 *
 * Initialize Host with Hash of values:
 * :host:: Full Firebird host string, e.g. 'hostname:service_mgr', '//hostname/service_mgr' (required)
 * :username:: database username (default: 'sysdba')
 * :password:: database password (default: 'masterkey')
 */
static VALUE host_initialize(int argc, VALUE *argv, VALUE self)
{
	VALUE parms, host;

	if (argc >= 1) {
		parms = argv[0];
		if (TYPE(parms) == T_STRING) {
			parms = hash_from_connection_string(parms);
		} else {
			Check_Type(parms, T_HASH);
		}
		host = rb_hash_aref(parms, ID2SYM(rb_intern("host")));
		if (NIL_P(host)) rb_raise(rb_eFbError, "host must be specified.");
		rb_iv_set(self, "@host", host);
		rb_iv_set(self, "@username", default_string(parms, "username", "sysdba"));
		rb_iv_set(self, "@password", default_string(parms, "password", "masterkey"));
	}
	return self;
}

/* call-seq:
 *   connect() -> ServiceConnection
 *   connect() {|service_connection| } -> nil
 *
 * Connect to the host specified by the current options.
 *
 * If a block is provided, the open connection is passed to it before being
 * automatically closed.
 */
static VALUE host_connect(VALUE self)
{
	ISC_STATUS isc_status[20];
	isc_svc_handle service_handle = 0;
	VALUE host = rb_iv_get(self, "@host");

	Check_Type(host, T_STRING);

    VALUE username = rb_iv_get(self, "@username");
    Check_Type(username, T_STRING);
    VALUE password = rb_iv_get(self, "@password");
    Check_Type(password, T_STRING);

    sbp_t sbp = sbp_create();
    sbp_add_code(sbp, isc_spb_version);
    sbp_add_code(sbp, isc_spb_current_version);
    sbp_add_string(sbp, isc_spb_user_name, 1, StringValuePtr(username));
    sbp_add_string(sbp, isc_spb_password, 1, StringValuePtr(password));

	isc_service_attach(isc_status, 0, StringValuePtr(host), &service_handle, sbp->size, sbp->buf);

	sbp_destroy(sbp);

    fb_error_check(isc_status);
	{
		VALUE connection = service_connection_create(service_handle, self);
		if (rb_block_given_p()) {
			return rb_ensure(rb_yield, connection, service_connection_close, connection);
			return Qnil;
		} else {
			return connection;
		}
	}
}

/* call-seq:
 *   Host.connect(options) -> ServiceConnection
 *   Host.connect(options) {|service_connection| } -> nil
 *
 * Connect to a host using the options given (see: Host.new for details of options Hash).
 *
 * If a block is provided, the open connection is passed to it before being
 * automatically closed.
 */
static VALUE host_s_connect(int argc, VALUE *argv, VALUE klass)
{
	VALUE obj = host_allocate_instance(klass);
	host_initialize(argc, argv, obj);
	return host_connect(obj);
}

void init_service_mgr(VALUE rb_mFb)
{
	rb_cFbHost = rb_define_class_under(rb_mFb, "Host", rb_cData);
    rb_define_alloc_func(rb_cFbHost, host_allocate_instance);
    rb_define_method(rb_cFbHost, "initialize", host_initialize, -1);
	rb_define_attr(rb_cFbHost, "host", 1, 1);
	rb_define_attr(rb_cFbHost, "username", 1, 1);
	rb_define_attr(rb_cFbHost, "password", 1, 1);
	rb_define_method(rb_cFbHost, "connect", host_connect, 0);
	rb_define_singleton_method(rb_cFbHost, "connect", host_s_connect, -1);


	rb_cFbServiceConnection = rb_define_class_under(rb_mFb, "ServiceConnection", rb_cData);
	rb_define_attr(rb_cFbServiceConnection, "database", 1, 1);
	rb_define_attr(rb_cFbServiceConnection, "username", 1, 1);
	rb_define_attr(rb_cFbServiceConnection, "password", 1, 1);
	rb_define_method(rb_cFbServiceConnection, "users", service_connection_users, 0);
	rb_define_method(rb_cFbServiceConnection, "close", service_connection_close, 0);

	rb_cFbUser = rb_define_class_under(rb_mFb, "User", rb_cData);
	rb_define_alloc_func(rb_cFbUser, user_allocate_instance);
	rb_define_attr(rb_cFbUser, "id", 1, 0);
	rb_define_attr(rb_cFbUser, "group_id", 1, 0);
	rb_define_attr(rb_cFbUser, "name", 1, 0);
	rb_define_attr(rb_cFbUser, "password", 1, 0);
	rb_define_attr(rb_cFbUser, "first_name", 1, 0);
	rb_define_attr(rb_cFbUser, "middle_name", 1, 0);
	rb_define_attr(rb_cFbUser, "last_name", 1, 0);

    id_force_encoding = rb_intern("force_encoding");
}