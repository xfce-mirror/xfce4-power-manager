
#ifndef ___hal_marshal_MARSHAL_H__
#define ___hal_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* NONE:INT,BOXED (hal-marshal.list:1) */
extern void _hal_marshal_VOID__INT_BOXED (GClosure     *closure,
                                          GValue       *return_value,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint,
                                          gpointer      marshal_data);
#define _hal_marshal_NONE__INT_BOXED	_hal_marshal_VOID__INT_BOXED

/* NONE:STRING,STRING,BOOLEAN,BOOLEAN (hal-marshal.list:2) */
extern void _hal_marshal_VOID__STRING_STRING_BOOLEAN_BOOLEAN (GClosure     *closure,
                                                              GValue       *return_value,
                                                              guint         n_param_values,
                                                              const GValue *param_values,
                                                              gpointer      invocation_hint,
                                                              gpointer      marshal_data);
#define _hal_marshal_NONE__STRING_STRING_BOOLEAN_BOOLEAN	_hal_marshal_VOID__STRING_STRING_BOOLEAN_BOOLEAN

G_END_DECLS

#endif /* ___hal_marshal_MARSHAL_H__ */

