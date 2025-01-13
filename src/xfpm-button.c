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
#include <X11/XKBlib.h>
#include <gdk/gdkx.h>
#endif

static void
xfpm_button_finalize (GObject *object);

#ifdef ENABLE_X11
static struct
{
  XfpmButtonKey key;
  guint key_code;
  guint key_modifiers;
} xfpm_key_map[N_XFPM_BUTTON_KEYS] = { { 0, 0 } };
#endif

struct XfpmButtonPrivate
{
  GdkScreen *screen;
  GdkWindow *window;
  Display *xdisplay;
  GdkDisplay *gdisplay;
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

static guint
xfpm_button_get_key (unsigned int keycode,
                     unsigned int keymodifiers)
{
  XfpmButtonKey key = BUTTON_UNKNOWN;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (xfpm_key_map); i++)
  {
    /* Match keycode and modifiers, but ignore CapsLock state */
    if (xfpm_key_map[i].key_code == keycode
        && xfpm_key_map[i].key_modifiers == (keymodifiers & ~LockMask))
      key = xfpm_key_map[i].key;
  }

  return key;
}

static gboolean
xfpm_button_keysym_to_code_mask (XfpmButton *button,
                                 guint keysym,
                                 guint *keycode,
                                 guint *modmask)
{
  GdkKeymap *keymap = gdk_keymap_get_for_display (button->priv->gdisplay);
  GdkKeymapKey *keys;
  gint nkeys;
  gboolean retval = FALSE;
  *keycode = 0;
  *modmask = 0;

  /*
   * Try to figure out the keysym modifier mask.
   * If there is more than 1 keycode for the keysym, the first keycode is used.
   * Defaults to previously used 'AllModifiers'. Code taken from keybinder library.
   */
  /* Force keymap sync */
  gdk_keymap_have_bidi_layouts (keymap);
  if (gdk_keymap_get_entries_for_keyval (keymap, keysym, &keys, &nkeys))
  {
    XFPM_DEBUG ("%i Keymap entries found", nkeys);
    XkbDescPtr xkbmap = XkbGetMap (button->priv->xdisplay, XkbAllClientInfoMask, XkbUseCoreKbd);
    if (xkbmap)
    {
      XkbKeyTypeRec *type = XkbKeyKeyType (xkbmap, keys[0].keycode, keys[0].group);
      if (type)
      {
        *keycode = keys[0].keycode;
        for (int k = 0; k < type->map_count; k++)
        {
          if (type->map[k].active && type->map[k].level == keys[0].level)
          {
            if (type->preserve)
            {
              *modmask = type->map[k].mods.mask & ~type->preserve[k].mask;
            }
            else
            {
              *modmask = type->map[k].mods.mask;
            }
          }
        }
        retval = TRUE;
      }
    }
    else
    {
      *modmask = AnyModifier;
    }
    g_free (keys);
  }
  else
  {
    XFPM_DEBUG ("No keymap entry found for keysym %i", keysym);
  }
  return retval;
}

static GdkFilterReturn
xfpm_button_filter_x_events (GdkXEvent *xevent,
                             GdkEvent *ev,
                             gpointer data)
{
  XfpmButtonKey key;
  XfpmButton *button;

  XEvent *xev = (XEvent *) xevent;

  if (xev->type != KeyPress)
    return GDK_FILTER_CONTINUE;

  XFPM_DEBUG ("keycode:%u, state:%u", xev->xkey.keycode, xev->xkey.state);
  key = xfpm_button_get_key (xev->xkey.keycode, xev->xkey.state);

  if (key != BUTTON_UNKNOWN)
  {
    button = (XfpmButton *) data;

    XFPM_DEBUG_ENUM (key, XFPM_TYPE_BUTTON_KEY, "Key press");

    g_signal_emit (G_OBJECT (button), signals[BUTTON_PRESSED], 0, key);
    return GDK_FILTER_REMOVE;
  }

  return GDK_FILTER_CONTINUE;
}

static gboolean
xfpm_button_grab_keystring (XfpmButton *button,
                            guint keycode,
                            guint modmask)
{
  guint ret;
  guint retval = 0;

  gdk_x11_display_error_trap_push (button->priv->gdisplay);

  ret = XGrabKey (button->priv->xdisplay, keycode, modmask,
                  GDK_WINDOW_XID (button->priv->window), True,
                  GrabModeAsync, GrabModeAsync);

  if (ret == BadAccess || ret == BadValue || ret == BadWindow)
  {
    g_warning ("ReturnCode=%u - Failed to grab modmask=%u, keycode=%li", ret, modmask, (long int) keycode);
    retval++;
  }

  ret = XGrabKey (button->priv->xdisplay, keycode, LockMask | modmask,
                  GDK_WINDOW_XID (button->priv->window), True,
                  GrabModeAsync, GrabModeAsync);

  if (ret == BadAccess || ret == BadValue || ret == BadWindow)
  {
    g_warning ("ReturnCode=%u - Failed to grab modmask=%u, keycode=%li", ret, LockMask | modmask, (long int) keycode);
    retval++;
  }

  gdk_display_flush (button->priv->gdisplay);
  gdk_x11_display_error_trap_pop_ignored (button->priv->gdisplay);

  return retval == 0;
}


static gboolean
xfpm_button_xevent_key (XfpmButton *button,
                        guint keysym,
                        XfpmButtonKey key)
{
  XFPM_DEBUG ("keysym:%x", keysym);
  guint keycode;
  guint keymodifiers;
  if (xfpm_button_keysym_to_code_mask (button, keysym, &keycode, &keymodifiers))
  {
    XFPM_DEBUG ("keycode:%u, keymodifiers:%u", keycode, keymodifiers);
    if (!xfpm_button_grab_keystring (button, keycode, keymodifiers))
      return FALSE;

    XFPM_DEBUG_ENUM (key, XFPM_TYPE_BUTTON_KEY, "Grabbed key:%li, mod:%u ", (long int) keycode, keymodifiers);

    xfpm_key_map[key].key_code = keycode;
    xfpm_key_map[key].key_modifiers = keymodifiers;
    xfpm_key_map[key].key = key;

    return TRUE;
  }
  else
  {
    XFPM_DEBUG ("could not map keysym %x to keycode and modifiers", keysym);
    return FALSE;
  }
}

static void
xfpm_button_ungrab (XfpmButton *button,
                    guint keysym,
                    XfpmButtonKey key)
{
  guint keycode;
  guint modmask;

  if (xfpm_button_keysym_to_code_mask (button, keysym, &keycode, &modmask))
  {
    gdk_x11_display_error_trap_push (button->priv->gdisplay);
    XUngrabKey (button->priv->xdisplay, keycode, modmask,
                GDK_WINDOW_XID (button->priv->window));
    XUngrabKey (button->priv->xdisplay, keycode, LockMask | modmask,
                GDK_WINDOW_XID (button->priv->window));
    gdk_display_flush (button->priv->gdisplay);
    gdk_x11_display_error_trap_pop_ignored (button->priv->gdisplay);
    XFPM_DEBUG_ENUM (key, XFPM_TYPE_BUTTON_KEY, "Ungrabbed key %li ", (long int) keycode);

    xfpm_key_map[key].key_code = 0;
    xfpm_key_map[key].key_modifiers = 0;
    xfpm_key_map[key].key = 0;
  }
  else
  {
    XFPM_DEBUG ("could not map keysym %x to keycode and modifiers", keysym);
  }
}

static void
xfpm_button_setup (XfpmButton *button)
{
  button->priv->screen = gdk_screen_get_default ();
  button->priv->window = gdk_screen_get_root_window (button->priv->screen);
  button->priv->gdisplay = gdk_display_get_default ();
  button->priv->xdisplay = gdk_x11_get_default_xdisplay ();

  if (xfpm_button_xevent_key (button, XF86XK_PowerOff, BUTTON_POWER_OFF))
    button->priv->mapped_buttons |= POWER_KEY;

#ifdef HAVE_XF86XK_HIBERNATE
  if (xfpm_button_xevent_key (button, XF86XK_Hibernate, BUTTON_HIBERNATE))
    button->priv->mapped_buttons |= HIBERNATE_KEY;
#endif

#ifdef HAVE_XF86XK_SUSPEND
  if (xfpm_button_xevent_key (button, XF86XK_Suspend, BUTTON_HIBERNATE))
    button->priv->mapped_buttons |= HIBERNATE_KEY;
#endif

  if (xfpm_button_xevent_key (button, XF86XK_Sleep, BUTTON_SLEEP))
    button->priv->mapped_buttons |= SLEEP_KEY;

  if (button->priv->handle_brightness_keys)
  {
    if (xfpm_button_xevent_key (button, XF86XK_MonBrightnessUp, BUTTON_MON_BRIGHTNESS_UP))
      button->priv->mapped_buttons |= BRIGHTNESS_KEY_UP;

    if (xfpm_button_xevent_key (button, XF86XK_MonBrightnessDown, BUTTON_MON_BRIGHTNESS_DOWN))
      button->priv->mapped_buttons |= BRIGHTNESS_KEY_DOWN;
  }

  if (xfpm_button_xevent_key (button, XF86XK_Battery, BUTTON_BATTERY))
    button->priv->mapped_buttons |= BATTERY_KEY;

  if (xfpm_button_xevent_key (button, XF86XK_KbdBrightnessUp, BUTTON_KBD_BRIGHTNESS_UP))
    button->priv->mapped_buttons |= KBD_BRIGHTNESS_KEY_UP;

  if (xfpm_button_xevent_key (button, XF86XK_KbdBrightnessDown, BUTTON_KBD_BRIGHTNESS_DOWN))
    button->priv->mapped_buttons |= KBD_BRIGHTNESS_KEY_DOWN;

  if (xfpm_button_xevent_key (button, XF86XK_KbdLightOnOff, BUTTON_KBD_BRIGHTNESS_CYCLE))
    button->priv->mapped_buttons |= KBD_BRIGHTNESS_CYCLE;

  gdk_window_add_filter (button->priv->window, xfpm_button_filter_x_events, button);
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
  button->priv->xdisplay = NULL;
  button->priv->gdisplay = NULL;

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
        if (xfpm_button_xevent_key (button, XF86XK_MonBrightnessUp, BUTTON_MON_BRIGHTNESS_UP))
          button->priv->mapped_buttons |= BRIGHTNESS_KEY_UP;

        if (xfpm_button_xevent_key (button, XF86XK_MonBrightnessDown, BUTTON_MON_BRIGHTNESS_DOWN))
          button->priv->mapped_buttons |= BRIGHTNESS_KEY_DOWN;
      }
      else
      {
        if ((button->priv->mapped_buttons & BRIGHTNESS_KEY_UP) != 0)
        {
          xfpm_button_ungrab (button, XF86XK_MonBrightnessUp, BUTTON_MON_BRIGHTNESS_UP);
          button->priv->mapped_buttons &= ~(BRIGHTNESS_KEY_UP);
        }

        if ((button->priv->mapped_buttons & BRIGHTNESS_KEY_DOWN) != 0)
        {
          xfpm_button_ungrab (button, XF86XK_MonBrightnessDown, BUTTON_MON_BRIGHTNESS_DOWN);
          button->priv->mapped_buttons &= ~(BRIGHTNESS_KEY_DOWN);
        }
      }
    }
#endif
  }
}
