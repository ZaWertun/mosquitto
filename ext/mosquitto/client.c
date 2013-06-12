#include "mosquitto_ext.h"

void rb_mosquitto_client_on_connect_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int rc)
{
    MosquittoGetClient((VALUE)obj);
    rb_funcall(client->connect_cb, intern_call, 1, NUM2INT(rc));
}

void rb_mosquitto_client_on_disconnect_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int rc)
{
    MosquittoGetClient((VALUE)obj);
   rb_funcall(client->disconnect_cb, intern_call, 1, NUM2INT(rc));
}

void rb_mosquitto_client_on_publish_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int mid)
{
    MosquittoGetClient((VALUE)obj);
    rb_funcall(client->publish_cb, intern_call, 1, INT2NUM(mid));
}

void rb_mosquitto_client_on_message_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
    VALUE message;
    MosquittoGetClient((VALUE)obj);
    message = rb_mosquitto_message_alloc(msg);
    rb_funcall(client->message_cb, intern_call, 1, message);
}

void rb_mosquitto_client_on_subscribe_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
    MosquittoGetClient((VALUE)obj);
    rb_funcall(client->subscribe_cb, intern_call, 3, INT2NUM(mid), INT2NUM(qos_count), INT2NUM(*granted_qos));
}

void rb_mosquitto_client_on_unsubscribe_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int mid)
{
    MosquittoGetClient((VALUE)obj);
    rb_funcall(client->message_cb, intern_call, 1, INT2NUM(mid));
}

void rb_mosquitto_client_on_log_cb(MOSQ_UNUSED struct mosquitto *mosq, void *obj, int level, const char *str)
{
    MosquittoGetClient((VALUE)obj);
	printf("XXX\n");
    rb_funcall(client->log_cb, intern_call, 2, INT2NUM(level), rb_str_new2(str));
	printf("YYY\n");
}

static void rb_mosquitto_mark_client(void *ptr)
{
    mosquitto_client_wrapper *client = (mosquitto_client_wrapper *)ptr;
    if (client) {
        rb_gc_mark(client->connect_cb);
        rb_gc_mark(client->disconnect_cb);
        rb_gc_mark(client->publish_cb);
        rb_gc_mark(client->message_cb);
        rb_gc_mark(client->subscribe_cb);
        rb_gc_mark(client->unsubscribe_cb);
        rb_gc_mark(client->log_cb);
    }
}

static void rb_mosquitto_free_client(void *ptr)
{
    mosquitto_client_wrapper *client = (mosquitto_client_wrapper *)ptr;
    if (client) {
        mosquitto_destroy(client->mosq);

        xfree(client);
    }
}

VALUE rb_mosquitto_client_s_new(int argc, VALUE *argv, VALUE client)
{
    VALUE client_id;
    char *cl_id = NULL;
    mosquitto_client_wrapper *cl = NULL;
    bool clean_session;
    rb_scan_args(argc, argv, "01&", &client_id);
    if (NIL_P(client_id)) {
        clean_session = true;
    } else {
        clean_session = false;
        Check_Type(client_id, T_STRING);
        cl_id = StringValueCStr(client_id);
    }
    client = Data_Make_Struct(rb_cMosquittoClient, mosquitto_client_wrapper, rb_mosquitto_mark_client, rb_mosquitto_free_client, cl);
    cl->mosq = mosquitto_new(cl_id, clean_session, (void *)client);
    if (cl->mosq == NULL) {
        switch (errno) {
            case EINVAL:
                MosquittoError("invalid input params");
                break;
            case ENOMEM:
                rb_memerror();
                break;
            default:
                return Qtrue;
        }
    }
    cl->connect_cb = Qnil;
    cl->disconnect_cb = Qnil;
    cl->publish_cb = Qnil;
    cl->message_cb = Qnil;
    cl->subscribe_cb = Qnil;
    cl->unsubscribe_cb = Qnil;
    cl->log_cb = Qnil;
    rb_obj_call_init(client, 0, NULL);
    return client;
}

static VALUE rb_mosquitto_client_reinitialise_nogvl(void *ptr)
{
    struct nogvl_reinitialise_args *args = ptr;
    return (VALUE)mosquitto_reinitialise(args->mosq, args->client_id, args->clean_session, args->obj);
}

VALUE rb_mosquitto_client_reinitialise(int argc, VALUE *argv, VALUE obj)
{
    struct nogvl_reinitialise_args args;
    MosquittoGetClient(obj);
    VALUE client_id;
    int ret;
    bool clean_session;
    char *cl_id = NULL;
    rb_scan_args(argc, argv, "01&", &client_id);
    if (NIL_P(client_id)) {
        clean_session = true;
    } else {
        clean_session = false;
        Check_Type(client_id, T_STRING);
        cl_id = StringValueCStr(client_id);
    }
    args.mosq = client->mosq;
    args.client_id = cl_id;
    args.clean_session = clean_session;
    args.obj = (void *)obj;
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_reinitialise_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       default:
           return Qtrue;
    }
}

VALUE rb_mosquitto_client_will_set(VALUE obj, VALUE topic, VALUE payload, VALUE qos, VALUE retain)
{
    MosquittoGetClient(obj);
    int ret;
    Check_Type(topic, T_STRING);
    Check_Type(payload, T_STRING);
    Check_Type(qos, T_FIXNUM);
    ret = mosquitto_will_set(client->mosq, StringValueCStr(topic), RSTRING_LEN(payload), StringValueCStr(payload), NUM2INT(qos), ((retain == Qtrue) ? true : false));
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_PAYLOAD_SIZE:
           MosquittoError("payload too large");
           break;
       default:
           return Qtrue;
    }
}

VALUE rb_mosquitto_client_will_clear(VALUE obj)
{
    MosquittoGetClient(obj);
    int ret;
    ret = mosquitto_will_clear(client->mosq);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       default:
           return Qtrue;
    }
}

VALUE rb_mosquitto_client_auth(VALUE obj, VALUE username, VALUE password)
{
    MosquittoGetClient(obj);
    int ret;
    Check_Type(username, T_STRING);
    Check_Type(password, T_STRING);
    ret = mosquitto_username_pw_set(client->mosq, StringValueCStr(username), StringValueCStr(password));
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       default:
           return Qtrue;
    }
}

static VALUE rb_mosquitto_client_connect_nogvl(void *ptr)
{
    struct nogvl_connect_args *args = ptr;
    return (VALUE)mosquitto_connect(args->mosq, args->host, args->port, args->keepalive);
}

VALUE rb_mosquitto_client_connect(VALUE obj, VALUE host, VALUE port, VALUE keepalive)
{
    struct nogvl_connect_args args;
    MosquittoGetClient(obj);
    int ret;
    Check_Type(host, T_STRING);
    Check_Type(port, T_FIXNUM);
    Check_Type(keepalive, T_FIXNUM);
    args.mosq = client->mosq;
    args.host = StringValueCStr(host);
    args.port = NUM2INT(port);
    args.keepalive = NUM2INT(keepalive);
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_connect_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_connect");
           break;
       default:
           return Qtrue;
    }
}

static VALUE rb_mosquitto_client_connect_async_nogvl(void *ptr)
{
    struct nogvl_connect_args *args = ptr;
    return (VALUE)mosquitto_connect_async(args->mosq, args->host, args->port, args->keepalive);
}

VALUE rb_mosquitto_client_connect_async(VALUE obj, VALUE host, VALUE port, VALUE keepalive)
{
    struct nogvl_connect_args args;
    MosquittoGetClient(obj);
    int ret;
    Check_Type(host, T_STRING);
    Check_Type(port, T_FIXNUM);
    Check_Type(keepalive, T_FIXNUM);
    args.mosq = client->mosq;
    args.host = StringValueCStr(host);
    args.port = NUM2INT(port);
    args.keepalive = NUM2INT(keepalive);
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_connect_async_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_connect_async");
           break;
       default:
           return Qtrue;
    }
}

VALUE rb_mosquitto_client_reconnect_nogvl(void *ptr)
{
    return (VALUE)mosquitto_reconnect((struct mosquitto *)ptr);
}

VALUE rb_mosquitto_client_reconnect(VALUE obj)
{
    MosquittoGetClient(obj);
    int ret;
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_reconnect_nogvl, (void *)&client->mosq, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_reconnect");
           break;
       default:
           return Qtrue;
    }
}

VALUE rb_mosquitto_client_disconnect_nogvl(void *ptr)
{
    return (VALUE)mosquitto_disconnect((struct mosquitto *)ptr);
}

VALUE rb_mosquitto_client_disconnect(VALUE obj)
{
    MosquittoGetClient(obj);
    int ret;
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_disconnect_nogvl, (void *)&client->mosq, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       default:
           return Qtrue;
    }
}

static VALUE rb_mosquitto_client_publish_nogvl(void *ptr)
{
    struct nogvl_publish_args *args = ptr;
    return (VALUE)mosquitto_publish(args->mosq, args->mid, args->topic, args->payloadlen, args->payload, args->qos, args->retain);
}

VALUE rb_mosquitto_client_publish(VALUE obj, VALUE mid, VALUE topic, VALUE payload, VALUE qos, VALUE retain)
{
    struct nogvl_publish_args args;
    MosquittoGetClient(obj);
    int ret, msg_id;
    Check_Type(topic, T_STRING);
    Check_Type(payload, T_STRING);
    Check_Type(qos, T_FIXNUM);
    if (!NIL_P(mid)) {
        Check_Type(mid, T_FIXNUM);
        msg_id = INT2NUM(mid);
    }
    args.mosq = client->mosq;
    args.mid = NIL_P(mid) ? NULL : &msg_id;
    args.topic = StringValueCStr(topic);
    args.payloadlen = RSTRING_LEN(payload);
    args.payload = (const char *)StringValueCStr(payload);
    args.qos = NUM2INT(qos);
    args.retain = (retain == Qtrue) ? true : false;
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_publish_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       case MOSQ_ERR_PROTOCOL:
           MosquittoError("protocol error communicating with broker");
           break;
       case MOSQ_ERR_PAYLOAD_SIZE:
           MosquittoError("payload too large");
           break;
       default:
           return Qtrue;
    }
}

static VALUE rb_mosquitto_client_subscribe_nogvl(void *ptr)
{
    struct nogvl_subscribe_args *args = ptr;
    return (VALUE)mosquitto_subscribe(args->mosq, args->mid, args->subscription, args->qos);
}

VALUE rb_mosquitto_client_subscribe(VALUE obj, VALUE mid, VALUE subscription, VALUE qos)
{
    struct nogvl_subscribe_args args;
    MosquittoGetClient(obj);
    int ret, msg_id;
    Check_Type(subscription, T_STRING);
    Check_Type(qos, T_FIXNUM);
    if (!NIL_P(mid)) {
        Check_Type(mid, T_FIXNUM);
        msg_id = INT2NUM(mid);
    }
    args.mosq = client->mosq;
    args.mid = NIL_P(mid) ? NULL : &msg_id;
    args.subscription = StringValueCStr(subscription);
    args.qos = NUM2INT(qos);
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_subscribe_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       default:
           return Qtrue;
    }
}

static VALUE rb_mosquitto_client_unsubscribe_nogvl(void *ptr)
{
    struct nogvl_subscribe_args *args = ptr;
    return (VALUE)mosquitto_unsubscribe(args->mosq, args->mid, args->subscription);
}

VALUE rb_mosquitto_client_unsubscribe(VALUE obj, VALUE mid, VALUE subscription)
{
    struct nogvl_subscribe_args args;
    MosquittoGetClient(obj);
    int ret, msg_id;
    Check_Type(subscription, T_STRING);
    if (!NIL_P(mid)) {
        Check_Type(mid, T_FIXNUM);
        msg_id = INT2NUM(mid);
    }
    args.mosq = client->mosq;
    args.mid = NIL_P(mid) ? NULL : &msg_id;
    args.subscription = StringValueCStr(subscription);
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_unsubscribe_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       default:
           return Qtrue;
    }
}

VALUE rb_mosquitto_client_socket(VALUE obj)
{
    MosquittoGetClient(obj);
    int socket;
    socket = mosquitto_socket(client->mosq);
    return INT2NUM(socket);
}

static VALUE rb_mosquitto_client_loop_nogvl(void *ptr)
{
    struct nogvl_loop_args *args = ptr;
    return (VALUE)mosquitto_loop(args->mosq, args->timeout, args->max_packets);
}

VALUE rb_mosquitto_client_loop(VALUE obj, VALUE timeout, VALUE max_packets)
{
    struct nogvl_loop_args args;
    MosquittoGetClient(obj);
    int ret;
    args.mosq = client->mosq;
    args.timeout = NUM2INT(timeout);
    args.max_packets = NUM2INT(max_packets);
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_loop_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       case MOSQ_ERR_CONN_LOST:
           MosquittoError("connection to the broker was lost");
           break;
       case MOSQ_ERR_PROTOCOL:
           MosquittoError("protocol error communicating with the broker");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_loop");
           break;
       default:
           return Qtrue;
    }
}

static VALUE rb_mosquitto_client_loop_forever_nogvl(void *ptr)
{
    struct nogvl_loop_args *args = ptr;
    return (VALUE)mosquitto_loop_forever(args->mosq, args->timeout, args->max_packets);
}

VALUE rb_mosquitto_client_loop_forever(VALUE obj, VALUE timeout, VALUE max_packets)
{
    struct nogvl_loop_args args;
    MosquittoGetClient(obj);
    int ret;
    Check_Type(timeout, T_FIXNUM);
    Check_Type(max_packets, T_FIXNUM);
    args.mosq = client->mosq;
    args.timeout = NUM2INT(timeout);
    args.max_packets = NUM2INT(max_packets);
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_loop_forever_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOT_SUPPORTED :
           MosquittoError("thread support is not available");
           break;
       default:
           return Qtrue;
    }
}

VALUE rb_mosquitto_client_loop_start_nogvl(void *ptr)
{
    return (VALUE)mosquitto_loop_start((struct mosquitto *)ptr);
}

VALUE rb_mosquitto_client_loop_start(VALUE obj)
{
    MosquittoGetClient(obj);
    int ret;
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_loop_start_nogvl, (void *)&client->mosq, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOT_SUPPORTED :
           MosquittoError("thread support is not available");
           break;
       default:
           return Qtrue;
    }
}

VALUE rb_mosquitto_client_loop_stop_nogvl(void *ptr)
{
    struct nogvl_loop_stop_args *args = ptr;
    return (VALUE)mosquitto_loop_stop(args->mosq, args->force);
}

VALUE rb_mosquitto_client_loop_stop(VALUE obj, VALUE force)
{
    struct nogvl_loop_stop_args args;
    MosquittoGetClient(obj);
    int ret;
    args.mosq = client->mosq;
    args.force = ((force == Qtrue) ? true : false);
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_loop_stop_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOT_SUPPORTED :
           MosquittoError("thread support is not available");
           break;
       default:
           return Qtrue;
    }
}

static VALUE rb_mosquitto_client_loop_read_nogvl(void *ptr)
{
    struct nogvl_loop_args *args = ptr;
    return (VALUE)mosquitto_loop_read(args->mosq, args->max_packets);
}

VALUE rb_mosquitto_client_loop_read(VALUE obj, VALUE max_packets)
{
    struct nogvl_loop_args args;
    MosquittoGetClient(obj);
    int ret;
    Check_Type(max_packets, T_FIXNUM);
    args.mosq = client->mosq;
    args.max_packets = NUM2INT(max_packets);
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_loop_read_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       case MOSQ_ERR_CONN_LOST:
           MosquittoError("connection to the broker was lost");
           break;
       case MOSQ_ERR_PROTOCOL:
           MosquittoError("protocol error communicating with the broker");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_loop");
           break;
       default:
           return Qtrue;
    }
}

static VALUE rb_mosquitto_client_loop_write_nogvl(void *ptr)
{
    struct nogvl_loop_args *args = ptr;
    return (VALUE)mosquitto_loop_write(args->mosq, args->max_packets);
}

VALUE rb_mosquitto_client_loop_write(VALUE obj, VALUE max_packets)
{
    struct nogvl_loop_args args;
    MosquittoGetClient(obj);
    int ret;
    Check_Type(max_packets, T_FIXNUM);
    args.mosq = client->mosq;
    args.max_packets = NUM2INT(max_packets);
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_loop_write_nogvl, (void *)&args, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOMEM:
           rb_memerror();
           break;
       case MOSQ_ERR_NO_CONN:
           MosquittoError("client not connected to broker");
           break;
       case MOSQ_ERR_CONN_LOST:
           MosquittoError("connection to the broker was lost");
           break;
       case MOSQ_ERR_PROTOCOL:
           MosquittoError("protocol error communicating with the broker");
           break;
       case MOSQ_ERR_ERRNO:
           rb_sys_fail("mosquitto_loop");
           break;
       default:
           return Qtrue;
    }
}

VALUE rb_mosquitto_client_loop_misc_nogvl(void *ptr)
{
    return (VALUE)mosquitto_loop_misc((struct mosquitto *)ptr);
}

VALUE rb_mosquitto_client_loop_misc(VALUE obj)
{
    MosquittoGetClient(obj);
    int ret;
    ret = (int)rb_thread_blocking_region(rb_mosquitto_client_loop_misc_nogvl, (void *)&client->mosq, RUBY_UBF_IO, 0);
    switch (ret) {
       case MOSQ_ERR_INVAL:
           MosquittoError("invalid input params");
           break;
       case MOSQ_ERR_NOT_SUPPORTED :
           MosquittoError("thread support is not available");
           break;
       default:
           return Qtrue;
    }
}

VALUE rb_mosquitto_client_want_write(VALUE obj)
{
    MosquittoGetClient(obj);
    bool ret;
    ret = mosquitto_want_write(client->mosq);
    return (ret == true) ? Qtrue : Qfalse;
}

VALUE rb_mosquitto_client_on_connect(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc;
    VALUE cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 1);
    client->connect_cb = cb;
    mosquitto_connect_callback_set(client->mosq, rb_mosquitto_client_on_connect_cb);
    return Qtrue;
}

VALUE rb_mosquitto_client_on_disconnect(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc;
    VALUE cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 0);
    client->disconnect_cb = cb;
    mosquitto_disconnect_callback_set(client->mosq, rb_mosquitto_client_on_disconnect_cb);
    return Qtrue;
}

VALUE rb_mosquitto_client_on_publish(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc;
    VALUE cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 1);
    client->publish_cb = cb;
    mosquitto_publish_callback_set(client->mosq, rb_mosquitto_client_on_publish_cb);
    return Qtrue;
}

VALUE rb_mosquitto_client_on_message(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc;
    VALUE cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 1);
    client->message_cb = cb;
    mosquitto_message_callback_set(client->mosq, rb_mosquitto_client_on_message_cb);
    return Qtrue;
}

VALUE rb_mosquitto_client_on_subscribe(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc;
    VALUE cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 3);
    client->subscribe_cb = cb;
    mosquitto_subscribe_callback_set(client->mosq, rb_mosquitto_client_on_subscribe_cb);
    return Qtrue;
}

VALUE rb_mosquitto_client_on_unsubscribe(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc;
    VALUE cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 1);
    client->unsubscribe_cb = cb;
    mosquitto_unsubscribe_callback_set(client->mosq, rb_mosquitto_client_on_unsubscribe_cb);
    return Qtrue;
}

VALUE rb_mosquitto_client_on_log(int argc, VALUE *argv, VALUE obj)
{
    VALUE proc;
    VALUE cb;
    MosquittoGetClient(obj);
    rb_scan_args(argc, argv, "01&", &proc, &cb);
    MosquittoAssertCallback(cb, 2);
    client->log_cb = cb;
    mosquitto_log_callback_set(client->mosq, rb_mosquitto_client_on_log_cb);
    return Qtrue;
}

void _init_rb_mosquitto_client()
{
    rb_cMosquittoClient = rb_define_class_under(rb_mMosquitto, "Client", rb_cObject);

    // Init / setup

    rb_define_singleton_method(rb_cMosquittoClient, "new", rb_mosquitto_client_s_new, -1);
    rb_define_method(rb_cMosquittoClient, "reinitialise", rb_mosquitto_client_reinitialise, -1);
    rb_define_method(rb_cMosquittoClient, "will_set", rb_mosquitto_client_will_set, 4);
    rb_define_method(rb_cMosquittoClient, "will_clear", rb_mosquitto_client_will_clear, 0);
    rb_define_method(rb_cMosquittoClient, "auth", rb_mosquitto_client_auth, 2);

    // Network

    rb_define_method(rb_cMosquittoClient, "connect", rb_mosquitto_client_connect, 3);
    rb_define_method(rb_cMosquittoClient, "connect_async", rb_mosquitto_client_connect_async, 3);
    rb_define_method(rb_cMosquittoClient, "reconnect", rb_mosquitto_client_reconnect, 0);
    rb_define_method(rb_cMosquittoClient, "disconnect", rb_mosquitto_client_disconnect, 0);
    rb_define_method(rb_cMosquittoClient, "publish", rb_mosquitto_client_publish, 5);
    rb_define_method(rb_cMosquittoClient, "subscribe", rb_mosquitto_client_subscribe, 3);
    rb_define_method(rb_cMosquittoClient, "unsubscribe", rb_mosquitto_client_unsubscribe, 2);
    rb_define_method(rb_cMosquittoClient, "socket", rb_mosquitto_client_socket, 0);
    rb_define_method(rb_cMosquittoClient, "loop", rb_mosquitto_client_loop, 2);
    rb_define_method(rb_cMosquittoClient, "loop_start", rb_mosquitto_client_loop_start, 0);
    rb_define_method(rb_cMosquittoClient, "loop_forever", rb_mosquitto_client_loop_forever, 2);
    rb_define_method(rb_cMosquittoClient, "loop_stop", rb_mosquitto_client_loop_stop, 1);
    rb_define_method(rb_cMosquittoClient, "loop_read", rb_mosquitto_client_loop_read, 1);
    rb_define_method(rb_cMosquittoClient, "loop_write", rb_mosquitto_client_loop_write, 1);
    rb_define_method(rb_cMosquittoClient, "loop_misc", rb_mosquitto_client_loop_misc, 0);

    // Callbacks

    rb_define_method(rb_cMosquittoClient, "on_connect", rb_mosquitto_client_on_connect, -1);
    rb_define_method(rb_cMosquittoClient, "on_disconnect", rb_mosquitto_client_on_disconnect, -1);
    rb_define_method(rb_cMosquittoClient, "on_publish", rb_mosquitto_client_on_publish, -1);
    rb_define_method(rb_cMosquittoClient, "on_message", rb_mosquitto_client_on_message, -1);
    rb_define_method(rb_cMosquittoClient, "on_subscribe", rb_mosquitto_client_on_subscribe, -1);
    rb_define_method(rb_cMosquittoClient, "on_unsubscribe", rb_mosquitto_client_on_unsubscribe, -1);
    rb_define_method(rb_cMosquittoClient, "on_log", rb_mosquitto_client_on_log, -1);
}