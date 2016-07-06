#include "prefix.h"
#define EXTERN extern
#include "header.h"

// constructor for an object
//
BUILTIN_FUNCTION(object_constr)
{
  JSValue rsv;
  uint64_t tag;
  JSValue ret;

  builtin_prologue();
  rsv = args[0];

  // If this is called with `new', which kind of object is allocated
  // depends on the type of the first argument.
  if (na > 0) {
    JSValue arg;
    arg = args[1];
    tag = get_tag(arg);
    switch(tag){
    case T_OBJECT:
      ret = arg;
      break;
    case T_FIXNUM:
    case T_FLONUM:
      ret = new_number(arg);
      set_prop_all(ret, gconsts.g_string___proto__, gconsts.g_number_proto);
      break;
    case T_SPECIAL:
      if (is_true(arg) || is_false(arg)) {
        ret = new_boolean(arg);
        set_prop_all(ret, gconsts.g_string___proto__, gconsts.g_boolean_proto);
      } else {
        ret = new_object();
        set_prop_all(ret, gconsts.g_string___proto__, gconsts.g_object_proto);
      }
      break;
    case T_STRING:
      ret = new_string(arg);
      set_prop_all(ret, gconsts.g_string___proto__, gconsts.g_string_proto);
      break;
    }
  } else {
    ret = new_object();
    set_prop_all(ret, gconsts.g_string___proto__, gconsts.g_object_proto);
  }
  set_a(context, ret);
}

BUILTIN_FUNCTION(object_toString)
{
  set_a(context, gconsts.g_string_objtostr);
}

void init_builtin_object(void)
{
  gconsts.g_object = new_builtin(object_constr, 0);
  gconsts.g_object_proto = new_object();
  set_prop_all(gconsts.g_object_proto, gconsts.g_string_prototype,
               gconsts.g_object_proto);
  set_prop_de(gconsts.g_object_proto, gconsts.g_string_tostring,
               new_builtin(object_toString, 0));
#ifdef PARALLEL
  set_obj_cstr_prop(gconsts.g_object_proto, "setShared",
               new_builtin(objectProtoSetShared, 0), ATTR_DE);

#endif
}

