/*
 * * Copyright (C) 2016 Eric Koegel <eric@xfce.org>
 * * Copyright (C) 2019 Kacper Piwiński
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
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "xfpm-config.h"
#include "xfce-screensaver.h"


#define XFSM_CHANNEL            "xfce4-session"
#define XFSM_PROPERTIES_PREFIX  "/general/"

static void xfce_screensvaer_finalize   (GObject *object);

static void xfce_screensaver_set_property(GObject *object,
                                          guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec);
static void xfce_screensaver_get_property(GObject *object,
                                          guint property_id,
                                          GValue *value,
                                          GParamSpec *pspec);



typedef enum
{
  SCREENSAVER_TYPE_NONE,
  SCREENSAVER_TYPE_FREEDESKTOP,
  SCREENSAVER_TYPE_CINNAMON,
  SCREENSAVER_TYPE_MATE,
  SCREENSAVER_TYPE_GNOME,
  SCREENSAVER_TYPE_XFCE,
  SCREENSAVER_TYPE_OTHER,
  N_SCREENSAVER_TYPE
} ScreenSaverType;

enum
{
  PROP_0 = 0,
  PROP_HEARTBEAT_COMMAND,
  PROP_LOCK_COMMAND
};

struct XfceScreenSaverPrivate
{
  guint            cookie;
  gchar           *heartbeat_command;
  gchar           *lock_command;
  GDBusProxy      *proxy;
  guint            screensaver_id;
  ScreenSaverType  screensaver_type;
  XfconfChannel   *xfpm_channel;
  XfconfChannel   *xfsm_channel;
};


G_DEFINE_TYPE_WITH_PRIVATE (XfceScreenSaver, xfce_screensaver, G_TYPE_OBJECT)


static void
xfce_screensaver_class_init (XfceScreenSaverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->finalize = xfce_screensvaer_finalize;
  object_class->set_property = xfce_screensaver_set_property;
  object_class->get_property = xfce_screensaver_get_property;

#define XFCE_PARAM_FLAGS  (G_PARAM_READWRITE \
                       | G_PARAM_CONSTRUCT \
                       | G_PARAM_STATIC_NAME \
                       | G_PARAM_STATIC_NICK \
                       | G_PARAM_STATIC_BLURB)

  /* heartbeat command - to inhibit the screensaver from activating,
   * i.e. xscreensaver-command -deactivate */
  g_object_class_install_property (object_class, PROP_HEARTBEAT_COMMAND,
                                   g_param_spec_string (HEARTBEAT_COMMAND,
                                                        HEARTBEAT_COMMAND,
                                                        "Inhibit the screensaver from activating, "
                                                        "i.e. xscreensaver-command -deactivate",
                                                        "xscreensaver-command -deactivate",
                                                        XFCE_PARAM_FLAGS));

  /* lock command - to lock the desktop, i.e. xscreensaver-command -lock */
  g_object_class_install_property (object_class, PROP_LOCK_COMMAND,
                                   g_param_spec_string (LOCK_COMMAND,
                                                        LOCK_COMMAND,
                                                        "Lock the desktop, i.e. "
                                                        "xscreensaver-command -lock",
                                                        "xscreensaver-command -lock",
                                                        XFCE_PARAM_FLAGS));
#undef XFCE_PARAM_FLAGS
}

static void
xfce_screensaver_set_property(GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  XfceScreenSaver *saver = XFCE_SCREENSAVER (object);

  switch(property_id) {
    case PROP_HEARTBEAT_COMMAND:
    {
      g_free (saver->priv->heartbeat_command);
      saver->priv->heartbeat_command = g_value_dup_string (value);
      DBG ("saver->priv->heartbeat_command %s", saver->priv->heartbeat_command);
      break;
    }
    case PROP_LOCK_COMMAND:
    {
      g_free (saver->priv->lock_command);
      saver->priv->lock_command = g_value_dup_string (value);
      DBG ("saver->priv->lock_command %s", saver->priv->lock_command);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
xfce_screensaver_get_property(GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  XfceScreenSaver *saver = XFCE_SCREENSAVER (object);

  switch(property_id) {
    case PROP_HEARTBEAT_COMMAND:
      g_value_set_string (value, saver->priv->heartbeat_command);
      break;

    case PROP_LOCK_COMMAND:
      g_value_set_string (value, saver->priv->lock_command);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static gboolean
screen_saver_proxy_setup(XfceScreenSaver *saver,
                         const gchar     *name,
                         const gchar     *object_path,
                         const gchar     *interface)
{
  GDBusProxy *proxy;

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         name,
                                         object_path,
                                         interface,
                                         NULL,
                                         NULL);

  if (proxy != NULL)
  {
    gchar *owner = NULL;
    /* is there anyone actually providing a service? */
    owner = g_dbus_proxy_get_name_owner (proxy);
    if (owner != NULL)
    {
      DBG ("proxy owner: %s", owner);
      saver->priv->proxy = proxy;
      g_free (owner);
      return TRUE;
    }
    else
    {
      /* not using this proxy, nobody's home */
      g_object_unref (proxy);
    }
  }

  return FALSE;
}

static void
xfce_screensaver_setup(XfceScreenSaver *saver)
{
  if (screen_saver_proxy_setup (saver,
                                "org.xfce.ScreenSaver",
                                "/org/xfce/ScreenSaver",
                                "org.xfce.ScreenSaver"))
  {
    DBG ("using Xfce screensaver daemon");
    saver->priv->screensaver_type = SCREENSAVER_TYPE_XFCE;
  }
  /* Try to use the freedesktop dbus API */
  else if (screen_saver_proxy_setup (saver,
                                     "org.freedesktop.ScreenSaver",
                                     "/org/freedesktop/ScreenSaver",
                                     "org.freedesktop.ScreenSaver"))
  {
    DBG ("using freedesktop compliant screensaver daemon");
    saver->priv->screensaver_type = SCREENSAVER_TYPE_FREEDESKTOP;
  }
  else if (screen_saver_proxy_setup (saver,
                                     "org.cinnamon.ScreenSaver",
                                     "/org/cinnamon/ScreenSaver",
                                     "org.cinnamon.ScreenSaver"))
  {
    DBG ("using cinnamon screensaver daemon");
    saver->priv->screensaver_type = SCREENSAVER_TYPE_CINNAMON;
  }
  else if (screen_saver_proxy_setup (saver,
                                     "org.mate.ScreenSaver",
                                     "/org/mate/ScreenSaver",
                                     "org.mate.ScreenSaver"))
  {
    DBG ("using mate screensaver daemon");
    saver->priv->screensaver_type = SCREENSAVER_TYPE_MATE;
  }
  else if (screen_saver_proxy_setup (saver,
                                     "org.gnome.ScreenSaver",
                                     "/org/gnome/ScreenSaver",
                                     "org.gnome.ScreenSaver"))
  {
    DBG ("using gnome screensaver daemon");
    saver->priv->screensaver_type = SCREENSAVER_TYPE_GNOME;
  }
  else
  {
    DBG ("using command line screensaver interface");
    saver->priv->screensaver_type = SCREENSAVER_TYPE_OTHER;
  }
}

static void
xfce_screensaver_init (XfceScreenSaver *saver)
{
  GError *error = NULL;

  saver->priv = xfce_screensaver_get_instance_private (saver);

  if ( !xfconf_init (&error) )
  {
    g_critical ("xfconf_init failed: %s\n", error->message);
    g_clear_error (&error);
  }
  else
  {
          saver->priv->xfpm_channel = xfconf_channel_get (XFPM_CHANNEL);
          saver->priv->xfsm_channel = xfconf_channel_get (XFSM_CHANNEL);
  }

  xfce_screensaver_setup (saver);
}

static void
xfce_screensvaer_finalize (GObject *object)
{
  XfceScreenSaver *saver = XFCE_SCREENSAVER (object);

  if (saver->priv->screensaver_id != 0)
  {
    g_source_remove (saver->priv->screensaver_id);
    saver->priv->screensaver_id = 0;
  }

  if (saver->priv->proxy)
  {
    g_object_unref (saver->priv->proxy);
    saver->priv->proxy = NULL;
  }

  if (saver->priv->heartbeat_command)
  {
    g_free (saver->priv->heartbeat_command);
    saver->priv->heartbeat_command = NULL;
  }

  if (saver->priv->lock_command)
  {
    g_free (saver->priv->heartbeat_command);
    saver->priv->heartbeat_command = NULL;
  }
}

/**
 * xfce_screensaver_new:
 *
 * Creates a new XfceScreenSaver object or increases the refrence count
 * of the current object. Call g_object_unref when finished.
 *
 * RETURNS: an XfceScreenSaver object
 **/
XfceScreenSaver *
xfce_screensaver_new (void)
{
  static gpointer *saver = NULL;

  if (saver != NULL)
  {
    g_object_ref (saver);
  }
  else
  {
    saver = g_object_new (XFCE_TYPE_SCREENSAVER, NULL);

    g_object_add_weak_pointer (G_OBJECT (saver), (gpointer *) &saver);

    xfconf_g_property_bind (XFCE_SCREENSAVER (saver)->priv->xfpm_channel,
                            XFPM_PROPERTIES_PREFIX HEARTBEAT_COMMAND,
                            G_TYPE_STRING,
                            G_OBJECT(saver),
                            HEARTBEAT_COMMAND);

    xfconf_g_property_bind (XFCE_SCREENSAVER (saver)->priv->xfsm_channel,
                            XFSM_PROPERTIES_PREFIX LOCK_COMMAND,
                            G_TYPE_STRING,
                            G_OBJECT(saver),
                            LOCK_COMMAND);
  }

  return XFCE_SCREENSAVER (saver);
}

static gboolean
xfce_reset_screen_saver (gpointer user_data)
{
  XfceScreenSaver *saver = user_data;
  TRACE("entering");

  /* If we found an interface during the setup, use it */
  if (saver->priv->proxy)
  {
    GVariant *response = g_dbus_proxy_call_sync (saver->priv->proxy,
                                                 "SimulateUserActivity",
                                                 NULL,
                                                 G_DBUS_CALL_FLAGS_NONE,
                                                 -1,
                                                 NULL,
                                                 NULL);
    if (response != NULL)
    {
      g_variant_unref (response);
    }
  } else if (saver->priv->heartbeat_command)
  {
    DBG ("running heartbeat command: %s", saver->priv->heartbeat_command);
    g_spawn_command_line_async (saver->priv->heartbeat_command, NULL);
  }

  /* continue until we're removed */
  return TRUE;
}

/**
 * xfce_screensaver_inhibit:
 * @saver: The XfceScreenSaver object
 * @inhibit: Wether to inhibit the screensaver from activating.
 *
 * Calling this function with inhibit as TRUE will prevent the user's
 * screensaver from activating. This is useful when the user is watching
 * a movie or giving a presentation.
 *
 * Calling this function with inhibit as FALSE will remove any current
 * screensaver inhibit the XfceScreenSaver object has.
 *
 **/
void
xfce_screensaver_inhibit (XfceScreenSaver *saver,
                          gboolean inhibit)
{
  /* SCREENSAVER_TYPE_FREEDESKTOP, SCREENSAVER_TYPE_MATE,
   * SCREENSAVER_TYPE_GNOME and SCREENSAVER_TYPE_XFCE
   * don't need a periodic timer because they have an actual
   * inhibit/uninhibit setup */
  switch (saver->priv->screensaver_type)
  {
    case SCREENSAVER_TYPE_FREEDESKTOP:
    case SCREENSAVER_TYPE_MATE:
    case SCREENSAVER_TYPE_GNOME:
    case SCREENSAVER_TYPE_XFCE:
    {
      if (inhibit)
      {
        GVariant *response = NULL;
        response = g_dbus_proxy_call_sync (saver->priv->proxy,
                                           "Inhibit",
                                           g_variant_new ("(ss)",
                                                          PACKAGE_NAME,
                                                          "Inhibit requested"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL, NULL);
        if (response != NULL)
        {
          g_variant_get (response, "(u)",
                         &saver->priv->cookie);
          g_variant_unref (response);
        }
      }
      else
      {
        GVariant *response = NULL;
        response = g_dbus_proxy_call_sync (saver->priv->proxy,
                                           "UnInhibit",
                                           g_variant_new ("(u)",
                                                          saver->priv->cookie),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL, NULL);

        saver->priv->cookie = 0;
        if (response != NULL)
        {
          g_variant_unref (response);
        }
      }
      break;
    }
    case SCREENSAVER_TYPE_OTHER:
    case SCREENSAVER_TYPE_CINNAMON:
    {
      /* remove any existing keepalive */
      if (saver->priv->screensaver_id != 0)
      {
        g_source_remove (saver->priv->screensaver_id);
        saver->priv->screensaver_id = 0;
      }

      if (inhibit)
      {
        /* Reset the screensaver timers every so often
         * so they don't activate */
        saver->priv->screensaver_id = g_timeout_add_seconds (20,
                                                             xfce_reset_screen_saver,
                                                             saver);
      }
      break;
    }
    default:
    {
      g_warning ("Not able to inhibit or uninhibit screensaver");
      break;
    }
  }
}

/**
 * xfce_screensaver_lock:
 * @saver: The XfceScreenSaver object
 *
 * Attempts to lock the screen, either with one of the screensaver
 * dbus proxies, the xfconf lock command, or one of the
 * fallback scripts such as xdg-screensaver.
 *
 * RETURNS TRUE if the lock attempt returns success.
 **/
gboolean
xfce_screensaver_lock (XfceScreenSaver *saver)
{
  switch (saver->priv->screensaver_type)
  {
    case SCREENSAVER_TYPE_FREEDESKTOP:
    case SCREENSAVER_TYPE_MATE:
    case SCREENSAVER_TYPE_GNOME:
    case SCREENSAVER_TYPE_XFCE:
    {
      GVariant *response = NULL;
      response = g_dbus_proxy_call_sync (saver->priv->proxy,
                                         "Lock",
                                         g_variant_new ("()"),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         NULL);
      if (response != NULL)
      {
        g_variant_unref (response);
        return TRUE;
      }
      else
      {
        return FALSE;
      }
      break;
    }
    case SCREENSAVER_TYPE_CINNAMON:
    {
      GVariant *response = NULL;
      response = g_dbus_proxy_call_sync (saver->priv->proxy,
                                         "Lock",
                                         g_variant_new ("(s)", PACKAGE_NAME),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         NULL);
      if (response != NULL)
      {
        g_variant_unref (response);
        return TRUE;
      }
      else
      {
        return FALSE;
      }
      break;
    }
    case SCREENSAVER_TYPE_OTHER:
    {
      gboolean ret = FALSE;

      if (saver->priv->lock_command != NULL)
      {
        DBG ("running lock command: %s", saver->priv->lock_command);
        ret = g_spawn_command_line_async (saver->priv->lock_command, NULL);
      }

      if (!ret)
      {
        g_warning ("Screensaver lock command not set when attempting to lock the screen.\n"
                   "Please set the xfconf property %s%s in xfce4-session to the desired lock command",
                   XFSM_PROPERTIES_PREFIX, LOCK_COMMAND);

        ret = g_spawn_command_line_async ("xflock4", NULL);
      }

      if (!ret)
      {
        ret = g_spawn_command_line_async ("xdg-screensaver lock", NULL);
      }

      if (!ret)
      {
        ret = g_spawn_command_line_async ("xscreensaver-command -lock", NULL);
      }

      return ret;
      /* obviously we don't need this break statement but I'm sure some
       * compiler or static analysis tool will complain */
      break;
    }
    default:
    {
      g_warning ("Unknown screensaver type set when calling xfce_screensaver_lock");
      break;
    }
  }

  return FALSE;
}
