/* This file is generated, all changes will be lost */
#ifndef ___GDK_MARSHAL_MARSHAL_H__
#define ___GDK_MARSHAL_MARSHAL_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* VOID:OBJECT (./gdkmarshalers.list:1) */
#define _gdk_marshal_VOID__OBJECT	g_cclosure_marshal_VOID__OBJECT

/* VOID:BOOLEAN (./gdkmarshalers.list:2) */
#define _gdk_marshal_VOID__BOOLEAN	g_cclosure_marshal_VOID__BOOLEAN

/* VOID:POINTER,POINTER,POINTER (./gdkmarshalers.list:3) */
G_BEGIN_DECLS
extern
void _gdk_marshal_VOID__POINTER_POINTER_POINTER (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);
G_END_DECLS

/* OBJECT:VOID (./gdkmarshalers.list:4) */
G_BEGIN_DECLS
extern
void _gdk_marshal_OBJECT__VOID (GClosure     *closure,
                                GValue       *return_value,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint,
                                gpointer      marshal_data);
G_END_DECLS

/* OBJECT:DOUBLE,DOUBLE (./gdkmarshalers.list:5) */
G_BEGIN_DECLS
extern
void _gdk_marshal_OBJECT__DOUBLE_DOUBLE (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);
G_END_DECLS

/* BOXED:INT,INT (./gdkmarshalers.list:6) */
G_BEGIN_DECLS
extern
void _gdk_marshal_BOXED__INT_INT (GClosure     *closure,
                                  GValue       *return_value,
                                  guint         n_param_values,
                                  const GValue *param_values,
                                  gpointer      invocation_hint,
                                  gpointer      marshal_data);
G_END_DECLS

/* VOID:DOUBLE,DOUBLE,POINTER,POINTER (./gdkmarshalers.list:7) */
G_BEGIN_DECLS
extern
void _gdk_marshal_VOID__DOUBLE_DOUBLE_POINTER_POINTER (GClosure     *closure,
                                                       GValue       *return_value,
                                                       guint         n_param_values,
                                                       const GValue *param_values,
                                                       gpointer      invocation_hint,
                                                       gpointer      marshal_data);
G_END_DECLS

/* VOID:POINTER,POINTER,BOOLEAN,BOOLEAN (./gdkmarshalers.list:8) */
G_BEGIN_DECLS
extern
void _gdk_marshal_VOID__POINTER_POINTER_BOOLEAN_BOOLEAN (GClosure     *closure,
                                                         GValue       *return_value,
                                                         guint         n_param_values,
                                                         const GValue *param_values,
                                                         gpointer      invocation_hint,
                                                         gpointer      marshal_data);
G_END_DECLS


G_END_DECLS

#endif /* ___GDK_MARSHAL_MARSHAL_H__ */
