
#include "PyKAdminObject.h"
#include "PyKAdminErrors.h"
#include "PyKAdminIterator.h"
#include "PyKAdminPrincipalObject.h"
#include "PyKAdminPolicyObject.h"

#include "PyKAdminCommon.h"

#include <datetime.h>

/*
kadm_principal_ent_rec reference

typedef struct _kadm5_principal_ent_t {

    // done
    krb5_principal  principal;
    krb5_principal  mod_name;

    // how to expose timestamps? 
    krb5_timestamp  princ_expire_time;
    krb5_timestamp  last_pwd_change;
    krb5_timestamp  pw_expiration;
    krb5_timestamp  mod_date;

    krb5_deltat     max_life;

    krb5_kvno       kvno;
    krb5_kvno       mkvno;

    char            *policy;

    krb5_flags      attributes;
    long            aux_attributes;

    // version 2 fields 

    krb5_timestamp last_success;
    krb5_timestamp last_failed;

    krb5_deltat max_renewable_life;

    krb5_kvno fail_auth_count;

    // these should not be accessed or modified directly by python.
    krb5_int16 n_key_data;
    krb5_int16 n_tl_data;
    krb5_tl_data *tl_data;
    krb5_key_data *key_data;

} kadm5_principal_ent_rec, *kadm5_principal_ent_t;
*/




static void PyKAdminPrincipal_dealloc(PyKAdminPrincipalObject *self) {
    
    kadm5_free_principal_ent(self->kadmin->server_handle, &self->entry);

    Py_XDECREF(self->kadmin);
   
    self->ob_type->tp_free((PyObject*)self);
}

static PyObject *PyKAdminPrincipal_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {

    PyKAdminPrincipalObject *self;

    self = (PyKAdminPrincipalObject *)type->tp_alloc(type, 0);

    if (!self)
        return NULL;
    
    memset(&self->entry, 0, sizeof(kadm5_principal_ent_rec));
    return (PyObject *)self;

}

static int PyKAdminPrincipal_init(PyKAdminPrincipalObject *self, PyObject *args, PyObject *kwds) {

    return 0;
}



static int PyKAdminPrincipal_print(PyKAdminPrincipalObject *self, FILE *file, int flags){

    static const char *kPRINT_FORMAT = "%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s";

    krb5_error_code errno;
    char *client_name = NULL;

    if (self && self->kadmin) {

        errno = krb5_unparse_name(self->kadmin->context, self->entry.principal, &client_name);

        fprintf(file, kPRINT_FORMAT, 
            "Principal",                      client_name,
            "Expiration date",                NULL,
            "Last password change",           NULL,
            "Password expiration date",       NULL,
            "Maximum ticket life",            NULL,
            "Maximum renewable life",         NULL,
            "Last modified",                  NULL,
            "Last successful authentication", NULL,
            "Last failed authentication",     NULL,
            "Failed password attempts",       NULL,
            "Number of keys",                 NULL
            );
    }

    if (client_name)
        free(client_name);
    
    return 0;
}



static PyMemberDef PyKAdminPrincipal_members[] = {
  
    {"failed_auth_count",           T_INT, offsetof(PyKAdminPrincipalObject, entry) + offsetof(kadm5_principal_ent_rec, fail_auth_count),       READONLY, ""},
    {"key_version_number",          T_INT, offsetof(PyKAdminPrincipalObject, entry) + offsetof(kadm5_principal_ent_rec, kvno),                  READONLY, ""},
    {"master_key_version_number",   T_INT, offsetof(PyKAdminPrincipalObject, entry) + offsetof(kadm5_principal_ent_rec, mkvno),                 READONLY, ""},

    {"policy",                      T_STRING, offsetof(PyKAdminPrincipalObject, entry) + offsetof(kadm5_principal_ent_rec, policy),                READONLY, ""},
    
    {NULL}
};



static PyObject *PyKAdminPrincipal_set_expire(PyKAdminPrincipalObject *self, PyObject *args, PyObject *kwds) {
    
    kadm5_ret_t retval = KADM5_OK; 
    time_t date        = 0; 
    char *expire       = NULL;

    if (!PyArg_ParseTuple(args, "s", &expire))
        return NULL;
    
    date = get_date(expire);
    if (date == (time_t)-1 ) { 
        // todo raise exception 
        return NULL;
    }

    self->entry.princ_expire_time = date;

    retval = kadm5_modify_principal(self->kadmin->server_handle, &self->entry, KADM5_PRINC_EXPIRE_TIME);
    if (retval != KADM5_OK) { PyKAdmin_RaiseKAdminError(retval, "kadm5_modify_principal"); return NULL; }

    Py_RETURN_TRUE;
}


static PyObject *PyKAdminPrincipal_clear_policy(PyKAdminPrincipalObject *self) {
    
    kadm5_ret_t retval = KADM5_OK;

    retval = kadm5_modify_principal(self->kadmin->server_handle, &self->entry, KADM5_POLICY_CLR);
    if (retval != KADM5_OK) { PyKAdmin_RaiseKAdminError(retval, "kadm5_modify_principal"); return NULL; }

    Py_RETURN_TRUE;
}


static PyObject *PyKAdminPrincipal_set_policy(PyKAdminPrincipalObject *self, PyObject *args, PyObject *kwds) {
    
    kadm5_ret_t retval = KADM5_OK;

    //  todo: parse as a pyobject, if we pass in a PolicyObject we need to accept that too.

    if (!PyArg_ParseTuple(args, "|z", &self->entry.policy))
        return NULL;

    if (self->entry.policy == NULL) {
        PyKAdminPrincipal_clear_policy(self);
    } else {
        retval = kadm5_modify_principal(self->kadmin->server_handle, &self->entry, KADM5_POLICY);
        if (retval != KADM5_OK) { PyKAdmin_RaiseKAdminError(retval, "kadm5_modify_principal"); return NULL; }
    }

    Py_RETURN_TRUE;
}

static PyObject *PyKAdminPrincipal_reload(PyKAdminPrincipalObject *self) {

    kadm5_ret_t retval = KADM5_OK; 

    if (self) {
        retval = kadm5_get_principal(self->kadmin->server_handle, self->entry.principal, &self->entry, KADM5_PRINCIPAL_NORMAL_MASK);
        if (retval != KADM5_OK) { PyKAdmin_RaiseKAdminError(retval, "kadm5_get_principal"); return NULL; }
    }

    Py_RETURN_TRUE;
}

static PyObject *PyKAdminPrincipal_change_password(PyKAdminPrincipalObject *self, PyObject *args, PyObject *kwds) {

    kadm5_ret_t retval = KADM5_OK; 
    char *password     = NULL;

    if (!PyArg_ParseTuple(args, "s", &password))
        return NULL; 

    retval = kadm5_chpass_principal(self->kadmin->server_handle, self->entry.principal, password);
    if (retval != KADM5_OK) { PyKAdmin_RaiseKAdminError(retval, "kadm5_chpass_principal"); return NULL; }

    Py_RETURN_TRUE;
}

static PyObject *PyKAdminPrincipal_randomize_key(PyKAdminPrincipalObject *self) {

    kadm5_ret_t retval = KADM5_OK; 

    retval = kadm5_randkey_principal(self->kadmin->server_handle, self->entry.principal, NULL, NULL);
    if (retval != KADM5_OK) { PyKAdmin_RaiseKAdminError(retval, "kadm5_randkey_principal"); return NULL; }

    Py_RETURN_TRUE;
}

PyObject *PyKAdminPrincipal_RichCompare(PyObject *o1, PyObject *o2, int opid) {

    PyKAdminPrincipalObject *a = (PyKAdminPrincipalObject *)o1;
    PyKAdminPrincipalObject *b = (PyKAdminPrincipalObject *)o2;

    PyObject *result = NULL; 
        
    int equal = pykadmin_principal_ent_rec_compare(a->kadmin->context, &a->entry, &b->entry);

    switch (opid) {

        case Py_EQ:
            result = ((a == b) || equal) ? Py_True : Py_False;
            break;
        case Py_NE:
            result = ((a != b) && !equal) ? Py_True : Py_False;
            break;
        case Py_LT:
        case Py_LE:
        case Py_GT:
        case Py_GE:
        default: 
            result = Py_NotImplemented;
            goto done;
    }


done:
    Py_XINCREF(result);
    return result;
}

static PyMethodDef PyKAdminPrincipal_methods[] = {
    {"cpw",             (PyCFunction)PyKAdminPrincipal_change_password,   METH_VARARGS, ""},
    {"change_password", (PyCFunction)PyKAdminPrincipal_change_password,   METH_VARARGS, ""},
    {"randkey",         (PyCFunction)PyKAdminPrincipal_randomize_key,     METH_NOARGS, ""},
    {"randomize_key",   (PyCFunction)PyKAdminPrincipal_randomize_key,     METH_NOARGS, ""},
    
    {"expire",          (PyCFunction)PyKAdminPrincipal_set_expire,        METH_VARARGS, ""},
    {"set_policy",      (PyCFunction)PyKAdminPrincipal_set_policy,        METH_VARARGS, ""},
    {"clear_policy",    (PyCFunction)PyKAdminPrincipal_clear_policy,      METH_NOARGS, ""},

    // TODO
    //{"set_max_renew",    (PyCFunction)NULL,      METH_NOARGS, ""},
    //{"password_expire",    (PyCFunction)NULL,      METH_NOARGS, ""},

    {"reload",          (PyCFunction)PyKAdminPrincipal_reload,            METH_NOARGS, ""},

    {NULL, NULL, 0, NULL}
};



static PyObject *PyKAdminPrincipal_get_principal(PyKAdminPrincipalObject *self, void *closure) {
  
    krb5_error_code ret = 0;
    PyObject *principal = NULL;
    char *client_name   = NULL;
    
    // todo: handle error
    ret = krb5_unparse_name(self->kadmin->context, self->entry.principal, &client_name);

    if (client_name) {
        principal = PyString_FromString(client_name);
        free(client_name);
    }

    return principal;
}


static PyObject *PyKAdminPrincipal_get_mod_name(PyKAdminPrincipalObject *self, void *closure) {
  
    krb5_error_code ret = 0;
    PyObject *principal = NULL;
    char *client_name   = NULL;
    
    // todo: handle error
    ret = krb5_unparse_name(self->kadmin->context, self->entry.mod_name, &client_name);

    if (client_name) {
        principal = PyString_FromString(client_name);
        free(client_name);
    }

    return principal;
}

static PyObject *PyKAdminPrincipal_get_last_pwd_change(PyKAdminPrincipalObject *self, void *closure) {

    PyObject *value = NULL;

    value = pykadmin_pydatetime_from_timestamp(self->entry.last_pwd_change);

    if (!value) {
        // todo: raise exception
    }

    return value;
}

static PyObject *PyKAdminPrincipal_get_princ_expire_time(PyKAdminPrincipalObject *self, void *closure) {

    PyObject *value = NULL;

    value = pykadmin_pydatetime_from_timestamp(self->entry.princ_expire_time);
    
    if (!value) {
        // todo: raise exception
    }

    return value;
}

static PyObject *PyKAdminPrincipal_get_pw_expiration(PyKAdminPrincipalObject *self, void *closure) {

    PyObject *value = NULL;

    value = pykadmin_pydatetime_from_timestamp(self->entry.pw_expiration);
    
    if (!value) {
        // todo: raise exception
    }

    return value;
}

static PyObject *PyKAdminPrincipal_get_mod_date(PyKAdminPrincipalObject *self, void *closure) {

    PyObject *value = NULL;

    value = pykadmin_pydatetime_from_timestamp(self->entry.mod_date);
    
    if (!value) {
        // todo: raise exception
    }

    return value;
}

static PyObject *PyKAdminPrincipal_get_last_success(PyKAdminPrincipalObject *self, void *closure) {

    PyObject *value = NULL;

    value = pykadmin_pydatetime_from_timestamp(self->entry.last_success);
    
    if (!value) {
        // todo: raise exception
    }

    return value;
}

static PyObject *PyKAdminPrincipal_get_last_failed(PyKAdminPrincipalObject *self, void *closure) {

    PyObject *value = NULL;

    value = pykadmin_pydatetime_from_timestamp(self->entry.last_failed);
    
    if (!value) {
        // todo: raise exception
    }

    return value;
}

static PyObject *PyKAdminPrincipal_get_max_renewable_life(PyKAdminPrincipalObject *self, void *closure) {

    PyDateTime_IMPORT;

    PyObject *value = NULL;

    value = PyDelta_FromDSU(0, self->entry.max_renewable_life, 0);
    
    if (!value) {
        // todo: raise exception
    }

    return value;
}




static PyGetSetDef PyKAdminPrincipal_getters_setters[] = {

    {"principal", (getter)PyKAdminPrincipal_get_principal,  NULL, "Kerberos Principal", NULL},
    {"name",      (getter)PyKAdminPrincipal_get_principal,  NULL, "Kerberos Principal", NULL},
    {"mod_name",  (getter)PyKAdminPrincipal_get_mod_name,   NULL, "Kerberos Principal", NULL},

    {"last_password_change", (getter)PyKAdminPrincipal_get_last_pwd_change,    NULL, "Kerberos Principal", NULL},
    {"expire_time",          (getter)PyKAdminPrincipal_get_princ_expire_time,  NULL, "Kerberos Principal", NULL},
    {"password_expiration",  (getter)PyKAdminPrincipal_get_pw_expiration,      NULL, "Kerberos Principal", NULL},
    {"mod_date",             (getter)PyKAdminPrincipal_get_mod_date,           NULL, "Kerberos Principal", NULL},
    {"last_success",         (getter)PyKAdminPrincipal_get_last_success,       NULL, "Kerberos Principal", NULL},
    {"last_failure",         (getter)PyKAdminPrincipal_get_last_failed,        NULL, "Kerberos Principal", NULL},

    {"max_renewable_life",   (getter)PyKAdminPrincipal_get_max_renewable_life, NULL, "Kerberos Principal", NULL},

    {NULL, NULL, NULL, NULL, NULL}
};



PyTypeObject PyKAdminPrincipalObject_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "kadmin.Principal",             /*tp_name*/
    sizeof(PyKAdminPrincipalObject),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyKAdminPrincipal_dealloc, /*tp_dealloc*/
    (printfunc)PyKAdminPrincipal_print,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0, //PyKAdminPrincipal_str,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /*tp_flags*/
    "KAdminPrincipal objects",           /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    PyKAdminPrincipal_RichCompare,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    PyKAdminPrincipal_methods,             /* tp_methods */
    PyKAdminPrincipal_members,             /* tp_members */
    PyKAdminPrincipal_getters_setters,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PyKAdminPrincipal_init,      /* tp_init */
    0,                         /* tp_alloc */
    PyKAdminPrincipal_new,                 /* tp_new */
};


PyKAdminPrincipalObject *PyKAdminPrincipalObject_principal_with_name(PyKAdminObject *kadmin, char *client_name) {
        
    krb5_error_code errno;

    PyKAdminPrincipalObject *principal = (PyKAdminPrincipalObject *)Py_None;

    if (client_name) {

        principal = (PyKAdminPrincipalObject *)PyKAdminPrincipal_new(&PyKAdminPrincipalObject_Type, NULL, NULL);

        if (principal) {

            Py_INCREF(kadmin);
            principal->kadmin = kadmin;

            errno = krb5_parse_name(kadmin->context, client_name, &principal->entry.principal);
            PyObject *result = PyKAdminPrincipal_reload(principal);

            if (!result || errno) {
                Py_INCREF(Py_None);
                PyKAdminPrincipal_dealloc(principal);
                principal = (PyKAdminPrincipalObject *)Py_None;
            }

        }
    }

    return principal;
}

PyKAdminPrincipalObject *PyKAdminPrincipalObject_principal_with_db_entry(PyKAdminObject *kadmin, krb5_db_entry *kdb) {

    kadm5_ret_t retval = KADM5_OK;

    PyKAdminPrincipalObject *principal = (PyKAdminPrincipalObject *)PyKAdminPrincipal_new(&PyKAdminPrincipalObject_Type, NULL, NULL);

    if (kdb) {

        Py_XINCREF(kadmin);
        principal->kadmin = kadmin;

        retval = pykadmin_kadm_from_kdb(kadmin, kdb, &principal->entry, KADM5_PRINCIPAL_NORMAL_MASK);

        if (retval) {

            PyKAdminPrincipal_dealloc(principal);
            
            // todo: set exception
            principal = NULL;

        } 
    }

    return principal;
}

void PyKAdminPrincipalObject_destroy(PyKAdminPrincipalObject *self) {
    PyKAdminPrincipal_dealloc(self);
}



