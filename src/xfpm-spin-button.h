#ifndef __XFPM_SPIN_BUTTON_H
#define __XFPM_SPIN_BUTTON_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFPM_TYPE_SPIN_BUTTON    (xfpm_spin_button_get_type())
#define XFPM_SPIN_BUTTON(o)      (G_TYPE_CHECK_INSTANCE_CAST((o),XFPM_TYPE_SPIN_BUTTON,XfpmSpinButton))
#define XFPM_IS_SPIN_BUTTON(o)   (G_TYPE_CHECK_INSTANCE_TYPE((o),XFPM_TYPE_SPIN_BUTTON))

typedef struct XfpmSpinButtonPrivate XfpmSpinButtonPrivate;

typedef struct 
{
    GtkSpinButton parent;
    
    gchar *suffix;
    gint suffix_length;
    gint suffix_position;
    
} XfpmSpinButton;

typedef struct 
{
    GtkSpinButtonClass parent_class;
    
} XfpmSpinButtonClass;

GType          xfpm_spin_button_get_type           (void);
GtkWidget     *xfpm_spin_button_new_with_range     (gdouble min,
													gdouble max,
													gdouble step);
void           xfpm_spin_button_set_suffix         (XfpmSpinButton *spin,
													const gchar *suffix);
G_END_DECLS

#endif /* __XFPM_SPIN_BUTTON_H */
