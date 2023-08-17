/*
 * * Copyright (C) 2023 Elliot <BlindRepublic@mailo.com>
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

#include <gio/gio.h>

#include "xfpm-ppd-common.h"

GDBusProxy *
xfpm_ppd_g_dbus_proxy_new (void)
{
  /* Helper function to create a dbus proxy for the power profiles daemon*/
  return g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        NULL,
                                        "net.hadess.PowerProfiles",
                                        "/net/hadess/PowerProfiles",
                                        "net.hadess.PowerProfiles",
                                        NULL, NULL);
}

GSList *
xfpm_ppd_get_profiles (GDBusProxy *proxy)
{
  GVariant *var  = NULL;
  GSList *names = NULL;

  g_return_val_if_fail (proxy != NULL, NULL);

  var = g_dbus_proxy_get_cached_property (proxy, "Profiles");

  if (var != NULL)
  {
    GVariantIter iter;
    GVariant *profile;

    /* Iternate over the profile dictionary */
    g_variant_iter_init (&iter, var);
    while ((profile = g_variant_iter_next_value (&iter)) != NULL)
    {
      gchar *name;

      /* Get the profile name */
      if (g_variant_lookup (profile, "Profile", "s", &name))
        names = g_slist_append (names, name);
    }

    g_variant_unref (profile);
    g_variant_unref (var);
  }

  return names;
}
