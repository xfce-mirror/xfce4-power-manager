#ifndef XFCE_SHORTCUTS_PORTAL_H
#define XFCE_SHORTCUTS_PORTAL_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _XfceShortcutsPortal XfceShortcutsPortal;

typedef void (*XfceShortcutsPortalHandler) (const gchar *key,
                                            gpointer     user_data);

XfceShortcutsPortal *xfce_shortcuts_portal_new      (const gchar                *app_id,
                                                     XfceShortcutsPortalHandler  handler,
                                                     gpointer                    user_data);
void                 xfce_shortcuts_portal_set_keys (XfceShortcutsPortal        *portal,
                                                     const gchar * const        *keys);
void                 xfce_shortcuts_portal_free     (XfceShortcutsPortal        *portal);

G_END_DECLS

#endif
