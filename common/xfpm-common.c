/*
 * * Copyright (C) 2008-2011 Ali <aliov@xfce.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#include "xfpm-common.h"
#include "xfpm-debug.h"

#ifdef ENABLE_X11
#include <X11/extensions/Xrandr.h>
#include <gdk/gdkx.h>
#endif

#ifdef ENABLE_WAYLAND
#include "protocols/wlr-output-management-unstable-v1-client.h"

#include <gdk/gdkwayland.h>
#endif

#include <libxfce4util/libxfce4util.h>

const gchar *
xfpm_bool_to_string (gboolean value)
{
  if (value)
    return "TRUE";
  else
    return "FALSE";
}

gboolean
xfpm_string_to_bool (const gchar *string)
{
  if (g_strcmp0 (string, "TRUE") == 0)
    return TRUE;
  else if (g_strcmp0 (string, "FALSE") == 0)
    return FALSE;

  return FALSE;
}

GtkBuilder *
xfpm_builder_new_from_resource (const gchar *resource,
                                GError **error)
{
  GtkBuilder *builder;

  builder = gtk_builder_new ();

  gtk_builder_add_from_resource (GTK_BUILDER (builder), resource, error);

  return builder;
}

void
xfpm_preferences (void)
{
  g_spawn_command_line_async ("xfce4-power-manager-settings", NULL);
}

void
xfpm_preferences_device_id (const gchar *object_path)
{
  gchar *string = g_strdup_printf ("xfce4-power-manager-settings -d %s", object_path);

  if (string)
    g_spawn_command_line_async (string, NULL);

  g_free (string);
}

void
xfpm_quit (void)
{
  g_spawn_command_line_async ("xfce4-power-manager -q", NULL);
}

void
xfpm_about (gpointer data)
{
  gchar *package = (gchar *) data;

  const gchar *authors[] = {
    "Ali Abdallah <aliov@xfce.org>",
    "Nick Schermer <nick@xfce.org>",
    "Eric Koegel <eric.koegel@gmail.com>",
    "Harald Judt <h.judt@gmx.at>",
    "Simon Steinbeiß <simon@xfce.org>",
    NULL,
  };

  static const gchar *documenters[] = {
    "Ali Abdallah <aliov@xfce.org>",
    NULL,
  };

  static const gchar *artists[] = {
    "Simon Steinbeiß <simon@xfce.org>",
    NULL,
  };

  gtk_show_about_dialog (NULL,
                         "copyright", "Copyright \302\251 2008-" COPYRIGHT_YEAR " The Xfce development team",
                         "destroy-with-parent", TRUE,
                         "authors", authors,
                         "artists", artists,
                         "documenters", documenters,
                         "license", XFCE_LICENSE_GPL,
                         "program-name", package,
                         "translator-credits", _("translator-credits"),
                         "version", VERSION_FULL,
                         "website", "https://docs.xfce.org/xfce/xfce4-power-manager/start",
                         "logo-icon-name", "org.xfce.powermanager",
                         NULL);
}

GdkPixbuf *
xfpm_icon_load (const gchar *icon_name,
                gint size,
                gint scale_factor)
{
  GdkPixbuf *pix = NULL;
  GError *error = NULL;

  pix = gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (),
                                            icon_name,
                                            size,
                                            scale_factor,
                                            GTK_ICON_LOOKUP_FORCE_SIZE,
                                            &error);

  if (error)
  {
    g_warning ("Unable to load icon : %s : %s", icon_name, error->message);
    g_error_free (error);
  }

  return pix;
}

XfpmMultiheadListener multihead_listener = NULL;

#ifdef ENABLE_X11
typedef struct _XfpmMultiheadDataX11
{
  GObject *lifetime;
  gboolean multihead_connected;
  gint event_base;
} XfpmMultiheadDataX11;

static GdkFilterReturn
filter (GdkXEvent *gdk_xevent,
        GdkEvent *event,
        gpointer _data)
{
  XfpmMultiheadDataX11 *data = _data;
  XRRScreenResources *resources;
  XEvent *xevent = gdk_xevent;
  Display *xdisplay;
  guint n_connected_outputs = 0;

  if (xevent == NULL)
    return GDK_FILTER_CONTINUE;

  if (xevent->type - data->event_base != RRScreenChangeNotify)
    return GDK_FILTER_CONTINUE;

  xdisplay = gdk_x11_get_default_xdisplay ();
  resources = XRRGetScreenResourcesCurrent (xdisplay, gdk_x11_get_default_root_xwindow ());
  for (gint n = 0; n < resources->noutput; n++)
  {
    XRROutputInfo *output_info = XRRGetOutputInfo (xdisplay, resources, resources->outputs[n]);
    if (output_info->connection == RR_Connected)
      n_connected_outputs++;
    XRRFreeOutputInfo (output_info);
  }
  XRRFreeScreenResources (resources);

  if (data->multihead_connected != (n_connected_outputs > 1))
  {
    data->multihead_connected = !data->multihead_connected;
    if (multihead_listener != NULL)
      multihead_listener (data->lifetime, data->multihead_connected);
  }

  return GDK_FILTER_CONTINUE;
}

static void
xfpm_multihead_data_x11_free (XfpmMultiheadDataX11 *data)
{
  gdk_window_remove_filter (gdk_get_default_root_window (), filter, data);
  g_free (data);
}
#endif

#ifdef ENABLE_WAYLAND
/* clang-format off */
static void registry_global (void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version);
static void registry_global_remove (void *data, struct wl_registry *registry, uint32_t id) {}
static void manager_head (void *data, struct zwlr_output_manager_v1 *wl_manager, struct zwlr_output_head_v1 *head);
static void manager_done (void *data, struct zwlr_output_manager_v1 *wl_manager, uint32_t serial);
static void manager_finished (void *data, struct zwlr_output_manager_v1 *wl_manager);
static void head_name (void *data, struct zwlr_output_head_v1 *head, const char *name) {}
static void head_description (void *data, struct zwlr_output_head_v1 *head, const char *description) {}
static void head_physical_size (void *data, struct zwlr_output_head_v1 *head, int32_t width, int32_t height) {}
static void head_mode (void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *wl_mode) {}
static void head_enabled (void *data, struct zwlr_output_head_v1 *head, int32_t enabled) {}
static void head_current_mode (void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *wl_mode) {}
static void head_position (void *data, struct zwlr_output_head_v1 *head, int32_t x, int32_t y) {}
static void head_transform (void *data, struct zwlr_output_head_v1 *head, int32_t transform) {}
static void head_scale (void *data, struct zwlr_output_head_v1 *head, wl_fixed_t scale) {}
static void head_finished (void *data, struct zwlr_output_head_v1 *head);
static void head_make (void *data, struct zwlr_output_head_v1 *head, const char *make) {}
static void head_model (void *data, struct zwlr_output_head_v1 *head, const char *model) {}
static void head_serial_number (void *data, struct zwlr_output_head_v1 *head, const char *serial_number) {}
static void head_adaptive_sync (void *data, struct zwlr_output_head_v1 *head, uint32_t state) {}
/* clang-format on */

static const struct wl_registry_listener registry_listener = {
  .global = registry_global,
  .global_remove = registry_global_remove,
};

static const struct zwlr_output_manager_v1_listener manager_listener = {
  .head = manager_head,
  .done = manager_done,
  .finished = manager_finished,
};

static const struct zwlr_output_head_v1_listener head_listener = {
  .name = head_name,
  .description = head_description,
  .physical_size = head_physical_size,
  .mode = head_mode,
  .enabled = head_enabled,
  .current_mode = head_current_mode,
  .position = head_position,
  .transform = head_transform,
  .scale = head_scale,
  .finished = head_finished,
  .make = head_make,
  .model = head_model,
  .serial_number = head_serial_number,
  .adaptive_sync = head_adaptive_sync,
};

typedef struct _XfpmMultiheadDataWayland
{
  GObject *lifetime;
  gboolean multihead_connected;
  struct wl_registry *wl_registry;
  struct zwlr_output_manager_v1 *wl_manager;
  GList *heads;
} XfpmMultiheadDataWayland;

static void
xfpm_multihead_data_wayland_free (XfpmMultiheadDataWayland *data)
{
  if (data->wl_manager != NULL)
    zwlr_output_manager_v1_destroy (data->wl_manager);
  g_list_free_full (data->heads, (GDestroyNotify) zwlr_output_head_v1_destroy);
  wl_registry_destroy (data->wl_registry);
  g_free (data);
}

static void
registry_global (void *_data,
                 struct wl_registry *registry,
                 uint32_t id,
                 const char *interface,
                 uint32_t version)
{
  XfpmMultiheadDataWayland *data = _data;
  if (g_strcmp0 (zwlr_output_manager_v1_interface.name, interface) == 0)
    data->wl_manager = wl_registry_bind (data->wl_registry, id, &zwlr_output_manager_v1_interface,
                                         MIN ((uint32_t) zwlr_output_manager_v1_interface.version, version));
}

static void
manager_head (void *_data,
              struct zwlr_output_manager_v1 *wl_manager,
              struct zwlr_output_head_v1 *head)
{
  XfpmMultiheadDataWayland *data = _data;
  data->heads = g_list_prepend (data->heads, head);
  zwlr_output_head_v1_add_listener (head, &head_listener, data);
}

static void
manager_done (void *_data,
              struct zwlr_output_manager_v1 *wl_manager,
              uint32_t serial)
{
  XfpmMultiheadDataWayland *data = _data;
  if (data->multihead_connected != (g_list_length (data->heads) > 1))
  {
    data->multihead_connected = !data->multihead_connected;
    if (multihead_listener != NULL)
      multihead_listener (data->lifetime, data->multihead_connected);
  }
}

static void
manager_finished (void *_data,
                  struct zwlr_output_manager_v1 *_wl_manager)
{
  XfpmMultiheadDataWayland *data = _data;
  zwlr_output_manager_v1_destroy (data->wl_manager);
  data->wl_manager = NULL;
}

static void
head_finished (void *_data,
               struct zwlr_output_head_v1 *head)
{
  XfpmMultiheadDataWayland *data = _data;
  data->heads = g_list_remove (data->heads, head);
  zwlr_output_head_v1_destroy (head);
}
#endif

gboolean
xfpm_is_multihead_connected (GObject *_lifetime)
{
  static GObject *lifetime = NULL;
  static gboolean native_checked = FALSE;
  static gboolean native_available = TRUE;

  GdkDisplay *display = gdk_display_get_default ();

  g_return_val_if_fail (lifetime == NULL || lifetime == _lifetime, FALSE);

#ifdef ENABLE_X11
  if (native_available && GDK_IS_X11_DISPLAY (display))
  {
    static XfpmMultiheadDataX11 *data = NULL;

    if (!native_checked)
    {
      Display *xdisplay = gdk_x11_get_default_xdisplay ();
      int event_base, error_base;
      native_available = XRRQueryExtension (xdisplay, &event_base, &error_base);
      native_checked = TRUE;

      if (native_available)
      {
        GdkWindow *window = gdk_get_default_root_window ();
        XEvent xevent = { 0 };

        lifetime = _lifetime;
        data = g_new0 (XfpmMultiheadDataX11, 1);
        data->lifetime = lifetime;
        data->event_base = event_base;
        g_object_weak_ref (lifetime, (GWeakNotify) (void (*) (void)) xfpm_multihead_data_x11_free, data);

        XRRSelectInput (xdisplay, GDK_WINDOW_XID (window), RRScreenChangeNotifyMask);
        gdk_x11_register_standard_event_type (gdk_display_get_default (), event_base, RRNotify + 1);
        gdk_window_add_filter (window, filter, data);
        xevent.type = event_base + RRScreenChangeNotify;
        filter ((GdkXEvent *) &xevent, NULL, data);
      }
      else
      {
        g_warning ("No Xrandr extension found, falling back to GDK output detection");
        return gdk_display_get_n_monitors (display) > 1;
      }
    }

    return data->multihead_connected;
  }
#endif

#ifdef ENABLE_WAYLAND
  if (native_available && GDK_IS_WAYLAND_DISPLAY (display))
  {
    static XfpmMultiheadDataWayland *data = NULL;

    if (!native_checked)
    {
      struct wl_display *wl_display = gdk_wayland_display_get_wl_display (display);

      data = g_new0 (XfpmMultiheadDataWayland, 1);
      data->wl_registry = wl_display_get_registry (wl_display);
      wl_registry_add_listener (data->wl_registry, &registry_listener, data);
      wl_display_roundtrip (wl_display);
      native_available = data->wl_manager != NULL;
      native_checked = TRUE;

      if (native_available)
      {
        lifetime = _lifetime;
        data->lifetime = lifetime;
        g_object_weak_ref (lifetime, (GWeakNotify) (void (*) (void)) xfpm_multihead_data_wayland_free, data);
        zwlr_output_manager_v1_add_listener (data->wl_manager, &manager_listener, data);
        wl_display_roundtrip (wl_display);
      }
      else
      {
        xfpm_multihead_data_wayland_free (data);
        g_warning ("Your compositor does not seem to support the wlr-output-management protocol:"
                   "falling back to GDK output detection");
        return gdk_display_get_n_monitors (display) > 1;
      }
    }

    return data->multihead_connected;
  }
#endif

  /* fallback: wrong if laptop screen is disabled and external screen is enabled */
  return gdk_display_get_n_monitors (display) > 1;
}

void
xfpm_set_multihead_listener (GObject *lifetime,
                             XfpmMultiheadListener listener)
{
  /* init if necessary, with NULL listener */
  xfpm_is_multihead_connected (lifetime);

  multihead_listener = listener;
}
