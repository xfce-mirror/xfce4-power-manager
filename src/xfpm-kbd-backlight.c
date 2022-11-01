/*
 * * Copyright (C) 2013 Sonal Santan <sonal.santan@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "xfpm-kbd-backlight.h"
#include "xfpm-button.h"
#include "xfpm-notify.h"
#include "xfpm-power.h"

static void xfpm_kbd_backlight_finalize     (GObject *object);

struct XfpmKbdBacklightPrivate
{
  XfpmPower          *power;
  XfpmButton         *button;

  GDBusConnection    *bus;
  GDBusProxy         *proxy;

  gboolean            dimmed;
  gboolean            on_battery;
  gint32              max_level;
  gint                min_level;
  gint                step;

  XfpmNotify         *notify;
  NotifyNotification *n;
};

G_DEFINE_TYPE_WITH_PRIVATE (XfpmKbdBacklight, xfpm_kbd_backlight, G_TYPE_OBJECT)


static gint
calculate_step( gint max_level )
{
  if ( max_level < 20 )
    return 1;
  else
    return max_level / 20;
}


static void
xfpm_kbd_backlight_on_battery_changed_cb (XfpmPower *power, gboolean on_battery, XfpmKbdBacklight *backlight)
{
  backlight->priv->on_battery = on_battery;
}


static void
xfpm_kbd_backlight_init_max_level (XfpmKbdBacklight *backlight)
{
  GError *error = NULL;
  GVariant *var;

  var = g_dbus_proxy_call_sync (backlight->priv->proxy, "GetMaxBrightness",
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1, NULL,
                                &error);

  if (var)
  {
    g_variant_get (var,
                   "(i)",
                   &backlight->priv->max_level);
    g_variant_unref (var);
  }

  if ( error )
  {
    g_warning ("Failed to get keyboard max brightness level : %s", error->message);
    g_error_free (error);
  }
}


static void
xfpm_kbd_backlight_show_notification (XfpmKbdBacklight *self, gfloat value)
{
  gchar *summary;

  if ( self->priv->n == NULL )
  {
    /* generate a human-readable summary for the notification */
    summary = g_strdup_printf (_("Keyboard Brightness: %.0f percent"), value);
    self->priv->n = xfpm_notify_new_notification (self->priv->notify,
                                                  _("Power Manager"),
                                                  summary,
                                                  "keyboard-brightness",
                                                  XFPM_NOTIFY_NORMAL);
    g_free (summary);
  }

  /* add the brightness value to the notification */
  notify_notification_set_hint (self->priv->n, "value", g_variant_new_int32 (value));

  /* show the notification */
  notify_notification_show (self->priv->n, NULL);
}


static gint
xfpm_kbd_backlight_get_level (XfpmKbdBacklight *backlight)
{
  GError *error = NULL;
  gint32 level = -1;
  GVariant *var;

  var = g_dbus_proxy_call_sync (backlight->priv->proxy, "GetBrightness",
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1, NULL,
                                &error);
  if (var)
  {
    g_variant_get (var,
                   "(i)",
                   &level);
    g_variant_unref (var);
  }

  if ( error )
  {
    g_warning ("Failed to get keyboard brightness level : %s", error->message);
    g_error_free (error);
  }
  return level;
}


static void
xfpm_kbd_backlight_set_level (XfpmKbdBacklight *backlight, gint32 level)
{
  GError *error = NULL;
  gfloat percent;
  GVariant *var;

  var = g_dbus_proxy_call_sync (backlight->priv->proxy, "SetBrightness",
                                g_variant_new("(i)", level),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1, NULL,
                                &error);

  if (var)
    g_variant_unref (var);

  if ( error )
  {
    g_warning ("Failed to set keyboard brightness level : %s", error->message);
    g_error_free (error);
  }
  else
  {
    percent = 100.0 * ((gfloat)level / (gfloat)backlight->priv->max_level);
    xfpm_kbd_backlight_show_notification (backlight, percent);
  }
}

static void
xfpm_kbd_backlight_up (XfpmKbdBacklight *backlight)
{
  gint level;

  level = xfpm_kbd_backlight_get_level (backlight);

  if ( level == -1)
    return;

  if ( level == backlight->priv->max_level )
    return;

  level += backlight->priv->step;

  if ( level > backlight->priv->max_level )
    level = backlight->priv->max_level;

  xfpm_kbd_backlight_set_level (backlight, level);
}


static void
xfpm_kbd_backlight_down (XfpmKbdBacklight *backlight)
{
  gint level;

  level = xfpm_kbd_backlight_get_level (backlight);

  if ( level == -1)
    return;

  if ( level == backlight->priv->min_level )
    return;

  level -= backlight->priv->step;

  if ( level < backlight->priv->min_level )
    level = backlight->priv->min_level;

  xfpm_kbd_backlight_set_level (backlight, level);
}


static void
xfpm_kbd_backlight_button_pressed_cb (XfpmButton *button, XfpmButtonKey type, XfpmKbdBacklight *backlight)
{
  if ( type == BUTTON_KBD_BRIGHTNESS_UP )
  {
    xfpm_kbd_backlight_up (backlight);
  }
  else if ( type == BUTTON_KBD_BRIGHTNESS_DOWN )
  {
    xfpm_kbd_backlight_down (backlight);
  }
}


static void
xfpm_kbd_backlight_class_init (XfpmKbdBacklightClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xfpm_kbd_backlight_finalize;
}


static void
xfpm_kbd_backlight_init (XfpmKbdBacklight *backlight)
{
  GError *error = NULL;

  backlight->priv = xfpm_kbd_backlight_get_instance_private (backlight);

  backlight->priv->bus = NULL;
  backlight->priv->proxy = NULL;
  backlight->priv->power = NULL;
  backlight->priv->button = NULL;
  backlight->priv->dimmed = FALSE;
  backlight->priv->on_battery = FALSE;
  backlight->priv->max_level = 0;
  backlight->priv->min_level = 0;
  backlight->priv->notify = NULL;
  backlight->priv->n = NULL;

  backlight->priv->bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);

  if ( error )
  {
    g_critical ("Unable to get system bus connection : %s", error->message);
    g_error_free (error);
    goto out;
  }

  backlight->priv->proxy = g_dbus_proxy_new_sync (backlight->priv->bus,
                                                  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                  G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                  NULL,
                                                  "org.freedesktop.UPower",
                                                  "/org/freedesktop/UPower/KbdBacklight",
                                                  "org.freedesktop.UPower.KbdBacklight",
                                                  NULL,
                                                  NULL);
  if ( backlight->priv->proxy == NULL )
  {
    g_warning ("Unable to get the interface, org.freedesktop.UPower.KbdBacklight");
    goto out;
  }

  xfpm_kbd_backlight_init_max_level (backlight);

  if ( backlight->priv->max_level == 0 )
    goto out;

  backlight->priv->step = calculate_step (backlight->priv->max_level);
  backlight->priv->power = xfpm_power_get ();
  backlight->priv->button = xfpm_button_new ();
  backlight->priv->notify = xfpm_notify_new ();

  g_signal_connect (backlight->priv->button, "button-pressed",
                    G_CALLBACK (xfpm_kbd_backlight_button_pressed_cb), backlight);

  g_signal_connect (backlight->priv->power, "on-battery-changed",
                    G_CALLBACK (xfpm_kbd_backlight_on_battery_changed_cb), backlight);

  g_object_get (G_OBJECT (backlight->priv->power),
                "on-battery", &backlight->priv->on_battery,
                NULL);

out:
  ;
}


static void
xfpm_kbd_backlight_finalize (GObject *object)
{
  XfpmKbdBacklight *backlight = NULL;

  backlight = XFPM_KBD_BACKLIGHT (object);

  if ( backlight->priv->power )
    g_object_unref (backlight->priv->power );

  if ( backlight->priv->button )
    g_object_unref (backlight->priv->button);

  if ( backlight->priv->notify )
    g_object_unref (backlight->priv->notify);

  if ( backlight->priv->n )
    g_object_unref (backlight->priv->n);

  if ( backlight->priv->proxy )
    g_object_unref (backlight->priv->proxy);

  if ( backlight->priv->bus )
    g_object_unref (backlight->priv->bus);

  G_OBJECT_CLASS (xfpm_kbd_backlight_parent_class)->finalize (object);
}


XfpmKbdBacklight *
xfpm_kbd_backlight_new (void)
{
  XfpmKbdBacklight *backlight = NULL;
  backlight = g_object_new (XFPM_TYPE_KBD_BACKLIGHT, NULL);
  return backlight;
}


gboolean xfpm_kbd_backlight_has_hw (XfpmKbdBacklight *backlight)
{
  return ( backlight->priv->proxy == NULL ) ? FALSE : TRUE;
}
