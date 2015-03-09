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

#define XFPM_KBD_BACKLIGHT_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_KBD_BACKLIGHT, XfpmKbdBacklightPrivate))

static void xfpm_kbd_backlight_finalize     (GObject *object);

struct XfpmKbdBacklightPrivate
{
    XfpmPower       *power;
    XfpmButton      *button;

    DBusGConnection *bus;
    DBusGProxy      *proxy;

    gboolean         dimmed;
    gboolean         on_battery;
    gint             max_level;
    gint             min_level;
    gint             step;

    XfpmNotify      *notify;
    NotifyNotification *n;
};

G_DEFINE_TYPE (XfpmKbdBacklight, xfpm_kbd_backlight, G_TYPE_OBJECT)


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

    dbus_g_proxy_call (backlight->priv->proxy, "GetMaxBrightness", &error,
                       G_TYPE_INVALID,
                       G_TYPE_INT, &backlight->priv->max_level,
                       G_TYPE_INVALID);

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
        self->priv->n = xfpm_notify_new_notification (self->priv->notify,
                "",
                "",
                "xfpm-brightness-keyboard",
                0,
                XFPM_NOTIFY_NORMAL);
    }

    /* generate a human-readable summary for the notification */
    summary = g_strdup_printf (_("Keyboard Brightness: %.0f percent"), value);
    notify_notification_update (self->priv->n, summary, NULL, NULL);
    g_free (summary);

    /* add the brightness value to the notification */
    notify_notification_set_hint_int32 (self->priv->n, "value", value);

    /* show the notification */
    notify_notification_show (self->priv->n, NULL);
}


static gint
xfpm_kbd_backlight_get_level (XfpmKbdBacklight *backlight)
{
    GError *error = NULL;
    gint level = -1;

    dbus_g_proxy_call (backlight->priv->proxy, "GetBrightness", &error,
                       G_TYPE_INVALID,
                       G_TYPE_INT, &level,
                       G_TYPE_INVALID);
    if ( error )
    {
        g_warning ("Failed to get keyboard brightness level : %s", error->message);
        g_error_free (error);
    }
    return level;
}


static void
xfpm_kbd_backlight_set_level (XfpmKbdBacklight *backlight, gint level)
{
    GError *error = NULL;
    gfloat percent;

    dbus_g_proxy_call (backlight->priv->proxy, "SetBrightness", &error,
                       G_TYPE_INT, level,
                       G_TYPE_INVALID, G_TYPE_INVALID);
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

    level = xfpm_kbd_backlight_get_level(backlight);

    if ( level == -1)
        return;

    if ( level == backlight->priv->max_level )
        return;

    level += backlight->priv->step;

    if ( level > backlight->priv->max_level )
        level = backlight->priv->max_level;

    xfpm_kbd_backlight_set_level(backlight, level);
}


static void
xfpm_kbd_backlight_down (XfpmKbdBacklight *backlight)
{
    gint level;

    level = xfpm_kbd_backlight_get_level(backlight);

    if ( level == -1)
        return;

    if ( level == backlight->priv->min_level )
        return;

    level -= backlight->priv->step;

    if ( level < backlight->priv->min_level )
        level = backlight->priv->min_level;

    xfpm_kbd_backlight_set_level(backlight, level);
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

    g_type_class_add_private (klass, sizeof (XfpmKbdBacklightPrivate));
}


static void
xfpm_kbd_backlight_init (XfpmKbdBacklight *backlight)
{
    GError *error = NULL;

    backlight->priv = XFPM_KBD_BACKLIGHT_GET_PRIVATE (backlight);

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

    backlight->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);

    if ( error )
    {
        g_critical ("Unable to get system bus connection : %s", error->message);
        g_error_free (error);
        goto out;
    }

    backlight->priv->proxy = dbus_g_proxy_new_for_name (backlight->priv->bus,
                                                        "org.freedesktop.UPower",
                                                        "/org/freedesktop/UPower/KbdBacklight",
                                                        "org.freedesktop.UPower.KbdBacklight");
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
        dbus_g_connection_unref (backlight->priv->bus);

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
