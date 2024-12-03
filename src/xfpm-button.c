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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


/*
 * Based on code from gpm-button (gnome power manager)
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xfpm-button.h"

#include "common/xfpm-debug.h"
#include "common/xfpm-enum-types.h"
#include "common/xfpm-enum.h"

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#ifdef ENABLE_X11
#include <X11/X.h>
#include <X11/XF86keysym.h>
#include <gdk/gdkx.h>
#include <keybinder.h>
#endif

static void 
xfpm_button_finalize (GObject *object);

struct XfpmButtonPrivate
{
  GdkScreen *screen;
  GdkWindow *window;
  gboolean handle_brightness_keys;
  guint16 mapped_buttons;
};

enum
{
  BUTTON_PRESSED,
  LAST_SIGNAL
};

#define DUPLICATE_SHUTDOWN_TIMEOUT 4.0f

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (XfpmButton, xfpm_button, G_TYPE_OBJECT)

#ifdef ENABLE_X11

static struct
{
  char *keysymbol;
  XfpmButtonKey key;
} xfpm_symbol_map[NUMBER_OF_BUTTONS] = {
  { "XF86MonBrightnessUp", BUTTON_MON_BRIGHTNESS_UP },
  { "XF86MonBrightnessDown", BUTTON_MON_BRIGHTNESS_DOWN },
  { "XF86KbdBrightnessUp", BUTTON_KBD_BRIGHTNESS_UP },
  { "XF86KbdBrightnessDown", BUTTON_KBD_BRIGHTNESS_DOWN },
  { "XF86PowerOff", BUTTON_POWER_OFF },
  { "XF86Hibernate", BUTTON_HIBERNATE },
  { "XF86Suspend", BUTTON_HIBERNATE },
  { "XF86Sleep", BUTTON_SLEEP },
  { "XF86XK_Battery", BUTTON_BATTERY },
  { NULL , BUTTON_UNKNOWN },
};

static void
xfpm_key_handler (const char *keystring, void *data)
{
  XfpmButton *button = (XfpmButton *) data;

  XFPM_DEBUG ("Key symbol received: %s", keystring);
  for (int idx=0; xfpm_symbol_map[idx].keysymbol; idx++) 
  {
    if (strcmp( keystring, xfpm_symbol_map[idx].keysymbol) == 0) 
    {
      g_signal_emit (G_OBJECT (button), signals[BUTTON_PRESSED], 0, xfpm_symbol_map[idx].key);
      XFPM_DEBUG ("Key press signalled");
      break;
    }
  }
}

static char *modifiers[] = {
  "",
 "<Ctrl>",
  "<Alt>",
  "<Super>",
  "<Shift>",
  "<Ctrl><Shift>",
  "<Ctrl><Alt>",
  "<Ctrl><Super>",
  "<Alt><Shift>",
  "<Alt><Super>",
  "<Shift><Super>",
  "<Ctrl><Shift><Super>",
  "<Ctrl><Shift><Alt>",
  "<Ctrl><Alt><Super>",
  "<Shift><Alt><Super>",
  "<Ctrl><Shift><Alt><Super>",
  NULL
};

static void
xfpm_bind_keysym (XfpmButton *button,
                  char *keysym,
                  XfpmKeys key)
{
  char buffer[100];

  if ((button->priv->mapped_buttons & key) == 0) 
  {
    for (int idx=0; modifiers[idx]; idx++) 
    {
      sprintf ( buffer, "%s%s", modifiers[idx], keysym);
      keybinder_bind (buffer, xfpm_key_handler, button);
    }
    button->priv->mapped_buttons |= key;
    XFPM_DEBUG ("Button bound: %s", keysym);
  }
}

static void
xfpm_unbind_keysym (XfpmButton *button,
                    char *keysym,
                    XfpmKeys key)
{
  char buffer[100];

  if ((button->priv->mapped_buttons & key) != 0) 
  {
    for (int idx=0; modifiers[idx]; idx++) 
    {
      sprintf ( buffer, "%s%s", modifiers[idx], keysym);
      keybinder_unbind (buffer, xfpm_key_handler);
    }
    button->priv->mapped_buttons &= ~(key);
    XFPM_DEBUG ("Button unbound: %s", keysym);
  }
}


static void
xfpm_button_setup (XfpmButton *button)
{
  button->priv->screen = gdk_screen_get_default ();
  button->priv->window = gdk_screen_get_root_window (button->priv->screen);

  keybinder_init ();

#ifdef HAVE_XF86XK_HIBERNATE
  xfpm_bind_keysym (button, "XF86Hibernate", HIBERNATE_KEY);
#endif
#ifdef HAVE_XF86XK_SUSPEND
  xfpm_bind_keysym (button, "XF86Suspend", HIBERNATE_KEY);
#endif
  xfpm_bind_keysym (button, "XF86PowerOff", POWER_KEY);
  xfpm_bind_keysym (button, "XF86Sleep", SLEEP_KEY);
  xfpm_bind_keysym (button, "XF86Battery", BATTERY_KEY);
  xfpm_bind_keysym (button, "XF86KbdBrightnessUp", KBD_BRIGHTNESS_KEY_UP);
  xfpm_bind_keysym (button, "XF86KbdBrightnessDown", KBD_BRIGHTNESS_KEY_DOWN);

  if (button->priv->handle_brightness_keys)
  {
    xfpm_bind_keysym (button, "XF86MonBrightnessUp", BRIGHTNESS_KEY_UP);
    xfpm_bind_keysym (button, "XF86MonBrightnessDown", BRIGHTNESS_KEY_DOWN);
  }
}
#endif /* ENABLE_X11 */

static void
xfpm_button_class_init (XfpmButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  signals[BUTTON_PRESSED] = g_signal_new ("button-pressed",
                                          XFPM_TYPE_BUTTON,
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (XfpmButtonClass, button_pressed),
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__ENUM,
                                          G_TYPE_NONE, 1, XFPM_TYPE_BUTTON_KEY);

  object_class->finalize = xfpm_button_finalize;
}

static void
xfpm_button_init (XfpmButton *button)
{
  button->priv = xfpm_button_get_instance_private (button);

  button->priv->handle_brightness_keys = FALSE;
  button->priv->mapped_buttons = 0;
  button->priv->screen = NULL;
  button->priv->window = NULL;

#ifdef ENABLE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    xfpm_button_setup (button);
#endif
}

static void
xfpm_button_finalize (GObject *object)
{
  G_OBJECT_CLASS (xfpm_button_parent_class)->finalize (object);
}

XfpmButton *
xfpm_button_new (void)
{
  static gpointer xfpm_button_object = NULL;

  if (G_LIKELY (xfpm_button_object != NULL))
  {
    g_object_ref (xfpm_button_object);
  }
  else
  {
    xfpm_button_object = g_object_new (XFPM_TYPE_BUTTON, NULL);
    g_object_add_weak_pointer (xfpm_button_object, &xfpm_button_object);
  }

  return XFPM_BUTTON (xfpm_button_object);
}

guint16
xfpm_button_get_mapped (XfpmButton *button)
{
  g_return_val_if_fail (XFPM_IS_BUTTON (button), 0);

  return button->priv->mapped_buttons;
}

void
xfpm_button_set_handle_brightness_keys (XfpmButton *button,
                                        gboolean handle_brightness_keys)
{
  g_return_if_fail (XFPM_IS_BUTTON (button));

  if (button->priv->handle_brightness_keys != handle_brightness_keys)
  {
    button->priv->handle_brightness_keys = handle_brightness_keys;

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      if (handle_brightness_keys)
      {
        xfpm_bind_keysym (button, "XF86MonBrightnessUp", BRIGHTNESS_KEY_UP);
        xfpm_bind_keysym (button, "XF86MonBrightnessDown", BRIGHTNESS_KEY_DOWN);
      }
      else
      {
        xfpm_unbind_keysym (button, "XF86MonBrightnessUp", BRIGHTNESS_KEY_UP);
        xfpm_unbind_keysym (button, "XF86MonBrightnessDown", BRIGHTNESS_KEY_DOWN);
      }
    }
#endif
  }
}
