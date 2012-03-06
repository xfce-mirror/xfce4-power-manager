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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "xfpm-icons.h"
#include "xfpm-power-common.h"
#include "xfpm-enum-glib.h"

#include "xfpm-unique.h"

typedef struct
{
    UpWakeups *up_wakeups;

    guint      timeout_id;

    GtkWidget *dialog;
    GtkWidget *notebook;
    GtkWidget *sideview; /*Sidebar tree view*/

    GtkWidget *wakeups; /* Tree view processor wakeups*/

} XfpmInfo;

enum
{
    XFPM_DEVICE_INFO_NAME,
    XFPM_DEVICE_INFO_VALUE,
    XFPM_DEVICE_INFO_LAST
};

enum
{
    COL_SIDEBAR_ICON,
    COL_SIDEBAR_NAME,
    COL_SIDEBAR_INT,
    NCOLS_SIDEBAR

};

enum
{
    COL_WAKEUPS_TYPE,
    COL_WAKEUPS_PID,
    COL_WAKEUPS_CMD,
    COL_WAKEUPS_VALUE,
    COL_WAKEUPS_DETAILS,
    NCOLS_WAKEUPS
};

static void G_GNUC_NORETURN
show_version (void)
{
    g_print (_("\n"
             "Xfce Power Manager %s\n\n"
             "Part of the Xfce Goodies Project\n"
             "http://goodies.xfce.org\n\n"
             "Licensed under the GNU GPL.\n\n"), VERSION);

    exit (EXIT_SUCCESS);
}


/**
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
 * gpm_stats_format_cmdline:
 **/
static gchar *
gpm_stats_format_cmdline (const gchar *command, gboolean is_userspace)
{
        gchar *found;
        gchar *temp = NULL;
        gchar *cmdline;
        const gchar *temp_ptr;

        /* nothing */
        if (command == NULL) {
                /* TRANSLATORS: the command line was not provided */
                temp_ptr = _("No data");
                goto out;
        }

        /* common kernel cmd names */
        if (g_strcmp0 (command, "insmod") == 0) {
                /* TRANSLATORS: kernel module, usually a device driver */
                temp_ptr = _("Kernel module");
                goto out;
        }
        if (g_strcmp0 (command, "modprobe") == 0) {
                /* TRANSLATORS: kernel module, usually a device driver */
                temp_ptr = _("Kernel module");
                goto out;
        }
        if (g_strcmp0 (command, "swapper") == 0) {
                /* TRANSLATORS: kernel housekeeping */
                temp_ptr = _("Kernel core");
                goto out;
        }
        if (g_strcmp0 (command, "kernel-ipi") == 0) {
                /* TRANSLATORS: interrupt between processors */
                temp_ptr = _("Interprocessor interrupt");
                goto out;
        }
        if (g_strcmp0 (command, "interrupt") == 0) {
                /* TRANSLATORS: unknown interrupt */
                temp_ptr = _("Interrupt");
                goto out;
        }

        /* truncate at first space or ':' */
        temp = g_strdup (command);
        found = strstr (temp, ":");
        if (found != NULL)
                *found = '\0';
        found = strstr (temp, " ");
        if (found != NULL)
                *found = '\0';

        /* remove path */
        found = g_strrstr (temp, "/");
        if (found != NULL && strncmp (temp, "event", 5) != 0)
                temp_ptr = found + 1;
        else
                temp_ptr = temp;

out:
        /* format command line */
        if (is_userspace)
                cmdline = g_markup_escape_text (temp_ptr, -1);
        else
                cmdline = g_markup_printf_escaped ("<i>%s</i>", temp_ptr);
        g_free (temp);

        /* return */
        return cmdline;
}

/**
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
 * gpm_stats_format_details:
 **/
static gchar *
gpm_stats_format_details (const gchar *command_details)
{
        gchar *details;

        /* replace common driver names */
        if (g_strcmp0 (command_details, "i8042") == 0) {
                /* TRANSLATORS: the keyboard and mouse device event */
                details = g_strdup (_("PS/2 keyboard/mouse/touchpad"));
        } else if (g_strcmp0 (command_details, "acpi") == 0) {
                /* TRANSLATORS: ACPI, the Intel power standard on laptops and desktops */
                details = g_strdup (_("ACPI"));
        } else if (g_strcmp0 (command_details, "ata_piix") == 0) {
                /* TRANSLATORS: serial ATA is a new style of hard disk interface */
                details = g_strdup (_("Serial ATA"));
        } else if (g_strcmp0 (command_details, "libata") == 0) {
                /* TRANSLATORS: this is the old-style ATA interface */
                details = g_strdup (_("ATA host controller"));
        } else if (g_strcmp0 (command_details, "iwl3945") == 0 || g_strcmp0 (command_details, "iwlagn") == 0) {
                /* TRANSLATORS: 802.11 wireless adaptor */
                details = g_strdup (_("Intel wireless adaptor"));

        /* try to make the wakeup type nicer */
        } else if (g_str_has_prefix (command_details, "__mod_timer")) {
                /* TRANSLATORS: a timer is something that fires periodically */
                details = g_strdup_printf (_("Timer %s"), command_details+12);
        } else if (g_str_has_prefix (command_details, "mod_timer")) {
                /* TRANSLATORS: a timer is something that fires periodically */
                details = g_strdup_printf (_("Timer %s"), command_details+10);
        } else if (g_str_has_prefix (command_details, "hrtimer_start_expires")) {
                /* TRANSLATORS: a timer is something that fires periodically */
                details = g_strdup_printf (_("Timer %s"), command_details+22);
        } else if (g_str_has_prefix (command_details, "hrtimer_start")) {
                /* TRANSLATORS: a timer is something that fires periodically */
                details = g_strdup_printf (_("Timer %s"), command_details+14);
        } else if (g_str_has_prefix (command_details, "do_setitimer")) {
                /* TRANSLATORS: a timer is something that fires periodically */
                details = g_strdup_printf (_("Timer %s"), command_details+10);
        } else if (g_str_has_prefix (command_details, "do_nanosleep")) {
                /* TRANSLATORS: this is a task that's woken up from sleeping */
                details = g_strdup_printf (_("Sleep %s"), command_details+13);
        } else if (g_str_has_prefix (command_details, "enqueue_task_rt")) {
                /* TRANSLATORS: this is a new realtime task */
                details = g_strdup_printf (_("New task %s"), command_details+16);
        } else if (g_str_has_prefix (command_details, "futex_wait")) {
                /* TRANSLATORS: this is a task thats woken to check state */
                details = g_strdup_printf (_("Wait %s"), command_details+11);
        } else if (g_str_has_prefix (command_details, "queue_delayed_work_on")) {
                /* TRANSLATORS: a work queue is a list of work that has to be done */
                details = g_strdup_printf (_("Work queue %s"), command_details+22);
        } else if (g_str_has_prefix (command_details, "queue_delayed_work")) {
                /* TRANSLATORS: a work queue is a list of work that has to be done */
                details = g_strdup_printf (_("Work queue %s"), command_details+19);
        } else if (g_str_has_prefix (command_details, "dst_run_gc")) {
                /* TRANSLATORS: this is when the networking subsystem clears out old entries */
                details = g_strdup_printf (_("Network route flush %s"), command_details+11);
        } else if (g_str_has_prefix (command_details, "usb_hcd_poll_rh_status")) {
                /* TRANSLATORS: activity on the USB bus */
                details = g_strdup_printf (_("USB activity %s"), command_details+23);
        } else if (g_str_has_prefix (command_details, "schedule_hrtimeout_range")) {
                /* TRANSLATORS: we've timed out of an aligned timer */
                details = g_strdup_printf (_("Wakeup %s"), command_details+25);
        } else if (g_str_has_prefix (command_details, "Local timer interrupts")) {
                /* TRANSLATORS: interupts on the system required for basic operation */
                details = g_strdup (_("Local interrupts"));
        } else if (g_str_has_prefix (command_details, "Rescheduling interrupts")) {
                /* TRANSLATORS: interrupts when a task gets moved from one core to another */
                details = g_strdup (_("Rescheduling interrupts"));
        } else
                details = g_markup_escape_text (command_details, -1);

        return details;
}

static gchar *
xfpm_info_get_energy_property (gdouble energy, const gchar *unit)
{
    if (energy > 0)
        return g_strdup_printf ("%.1f %s", energy, unit);
    else
        return NULL;
}

static void
xfpm_info_add_sidebar_icon (XfpmInfo *info, const gchar *name, const gchar *icon_name)
{
    GtkListStore *list_store;
    GtkTreeIter iter;
    GdkPixbuf *pix;
    guint nt;

    list_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (info->sideview)));

    nt = gtk_notebook_get_n_pages (GTK_NOTEBOOK (info->notebook));

    pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                    icon_name,
                                    48,
                                    GTK_ICON_LOOKUP_USE_BUILTIN,
                                    NULL);

    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter,
                        COL_SIDEBAR_ICON, pix,
                        COL_SIDEBAR_NAME, name,
                        COL_SIDEBAR_INT, nt,
                        -1);

    if ( pix )
        g_object_unref (pix);
}

static void
xfpm_info_add_device (XfpmInfo *info, UpDevice *device)
{
    GtkWidget *view;
    GtkListStore *list_store;
    GtkTreeIter iter;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    UpDeviceKind kind;
    gboolean power_supply;
    gchar *model;
    UpDeviceTechnology technology;
    gdouble energy_full_design;
    gdouble energy_empty;
    gdouble energy_full;
    gdouble voltage;
    gchar *vendor;
    gchar *serial;
    gchar *str;

    g_return_if_fail (UP_IS_DEVICE (device));

    view = gtk_tree_view_new ();

    list_store = gtk_list_store_new (XFPM_DEVICE_INFO_LAST, G_TYPE_STRING, G_TYPE_STRING);

    gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (list_store));

    renderer = gtk_cell_renderer_text_new ();

    /*Device Attribute*/
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "text", XFPM_DEVICE_INFO_NAME, NULL);
    gtk_tree_view_column_set_title (col, _("Attribute"));
    gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);

    /*Device Attribute Value*/
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "text", XFPM_DEVICE_INFO_VALUE, NULL);
    gtk_tree_view_column_set_title (col, _("Value"));

    gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);

    g_object_get (device,
                  "kind", &kind,
                  "power-supply", &power_supply,
                  "model", &model,
                  "technology", &technology,
                  "energy-full-design", &energy_full_design,
                  "energy-empty", &energy_empty,
                  "energy-full", &energy_full,
                  "voltage", &voltage,
                  "vendor", &vendor,
                  "serial", &serial,
                  NULL);

    /**
     * Add Device information:
     **/
    /* Device */
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter,
                        XFPM_DEVICE_INFO_NAME, _("Device"),
                        XFPM_DEVICE_INFO_VALUE, "TODO",
                        -1);

    /* Kind */
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter,
                        XFPM_DEVICE_INFO_NAME, _("Kind"),
                        XFPM_DEVICE_INFO_VALUE, xfpm_power_translate_kind (kind),
                        -1);

    /* PowerSupply */
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter,
                        XFPM_DEVICE_INFO_NAME, _("PowerSupply"),
                        XFPM_DEVICE_INFO_VALUE, power_supply ? _("True") : _("False"),
                        -1);

    if ( kind != UP_DEVICE_KIND_LINE_POWER)
    {
        /* Model */
        if ( model )
        {
            gtk_list_store_append (list_store, &iter);
            gtk_list_store_set (list_store, &iter,
                                XFPM_DEVICE_INFO_NAME, _("Model"),
                                XFPM_DEVICE_INFO_VALUE, model,
                                -1);
        }

        /* Technology */
        if ( technology != UP_DEVICE_TECHNOLOGY_UNKNOWN )
        {
            gtk_list_store_append (list_store, &iter);
            gtk_list_store_set (list_store, &iter,
                                XFPM_DEVICE_INFO_NAME, _("Technology"),
                                XFPM_DEVICE_INFO_VALUE, xfpm_power_translate_technology (technology),
                                -1);
        }

        /* TRANSLATORS: Unit here is What hour */
        str = xfpm_info_get_energy_property (energy_full_design, _("Wh"));
        if ( str )
        {
            gtk_list_store_append (list_store, &iter);
            gtk_list_store_set (list_store, &iter,
                                XFPM_DEVICE_INFO_NAME, _("Energy full design"),
                                XFPM_DEVICE_INFO_VALUE, str,
                                -1);
            g_free (str);
        }

        /* TRANSLATORS: Unit here is What hour */
        str = xfpm_info_get_energy_property (energy_full, _("Wh"));
        if ( str )
        {
            gtk_list_store_append (list_store, &iter);
            gtk_list_store_set (list_store, &iter,
                                XFPM_DEVICE_INFO_NAME, _("Energy full"),
                                XFPM_DEVICE_INFO_VALUE, str,
                                -1);
            g_free (str);
        }

        /* TRANSLATORS: Unit here is What hour */
        str = xfpm_info_get_energy_property (energy_empty, _("Wh"));
        if ( str )
        {
            gtk_list_store_append (list_store, &iter);
            gtk_list_store_set (list_store, &iter,
                                XFPM_DEVICE_INFO_NAME, _("Energy empty"),
                                XFPM_DEVICE_INFO_VALUE, str,
                                -1);
            g_free (str);
        }

        /* TRANSLATORS: Unit here is volt */
        str = xfpm_info_get_energy_property (voltage, _("V"));
        if ( str )
        {
            gtk_list_store_append (list_store, &iter);
            gtk_list_store_set (list_store, &iter,
                                XFPM_DEVICE_INFO_NAME, _("Voltage"),
                                XFPM_DEVICE_INFO_VALUE, str,
                                -1);
            g_free (str);
        }

        /* Vendor */
        if ( vendor )
        {
            gtk_list_store_append (list_store, &iter);
            gtk_list_store_set (list_store, &iter,
                                XFPM_DEVICE_INFO_NAME, _("Vendor"),
                                XFPM_DEVICE_INFO_VALUE, vendor,
                                -1);
        }

        /* Serial */
        if ( serial )
        {
            gtk_list_store_append (list_store, &iter);
            gtk_list_store_set (list_store, &iter,
                                XFPM_DEVICE_INFO_NAME, _("Serial"),
                                XFPM_DEVICE_INFO_VALUE, serial,
                                -1);
        }
    }

    g_free (model);
    g_free (vendor);
    g_free (serial);

    xfpm_info_add_sidebar_icon (info,
                                xfpm_power_translate_kind (kind),
				xfpm_power_get_icon_name (kind));

    gtk_notebook_append_page (GTK_NOTEBOOK (info->notebook), view, NULL);

    gtk_widget_show (view);
}

static void
xfpm_info_power_devices (XfpmInfo *info)
{
    UpClient *up_client;
    GPtrArray *array;
    guint i;
    UpDevice *device;
    GError *error = NULL;

    up_client = up_client_new ();
    if ( !up_client )
        return;

    if (!up_client_enumerate_devices_sync (up_client, NULL, &error))
    {
	g_object_unref (up_client);
	g_critical ("Failed to get devices: %s", error->message);
	g_error_free (error);
	
	return;
    }

    array = up_client_get_devices (up_client);

    if ( array )
    {
        for ( i = 0; i < array->len; i++)
        {
            device =  g_ptr_array_index (array, i);
            xfpm_info_add_device (info, device);
        }

        g_ptr_array_unref (array);
    }

    g_object_unref (up_client);
}


/**
 *
 * Method GetData on /org/freedesktop/DeviceKit/Power/Wakeups
 *
 * <method name="GetData">
 *     <arg name="data" type="a(budss)" direction="out"/> (1)
 * </method>
 *  (1): array | boolean        Wheter the proceess on userspace
 *             | uint           PID
 *             | double         Wakeups value
 *             | string         command line
 *             | string         details
 **/
static void
xfpm_info_update_wakeups (XfpmInfo *info)
{
    GtkListStore *store;
    GError *error = NULL;
    GPtrArray *array;
    UpWakeupItem *item;
    guint pid;
    gboolean userspace;
    guint i;
    gchar *pid_str;
    const gchar *icon;
    gchar *value_str;
    gchar *formatted_cmd;
    gchar *formatted_details;
    GtkTreeIter iter;

    array = up_wakeups_get_data_sync (info->up_wakeups, NULL, &error);

    if ( !array )
    {
        g_warning ("Failed to get wakeups data: %s", error ? error->message : "No error");
        g_clear_error (&error);
        return;
    }

    store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (info->wakeups)));

    gtk_list_store_clear (GTK_LIST_STORE (store));

    for ( i = 0; i < array->len; i++ )
    {
        item = g_ptr_array_index (array, i);
        g_return_if_fail (UP_IS_WAKEUP_ITEM (item));

        pid = up_wakeup_item_get_id (item);
        userspace = up_wakeup_item_get_is_userspace (item);

        if ( userspace )
        {
            pid_str = g_strdup_printf ("%i", pid);
            icon = "application-x-executable";
        }
        else
        {
            if ( pid < 0xff0 )
            {
                pid_str = g_strdup_printf ("IRQ%i", pid);
            }
            else
            {
                pid_str = g_strdup("IRQx");
            }

            icon = "applications-system";
        }

        value_str = g_strdup_printf ("%.1f", up_wakeup_item_get_value (item));
        formatted_cmd = gpm_stats_format_cmdline (up_wakeup_item_get_cmdline (item), userspace);
        formatted_details = gpm_stats_format_details (up_wakeup_item_get_details (item));

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COL_WAKEUPS_TYPE, icon,
                            COL_WAKEUPS_PID, pid_str,
                            COL_WAKEUPS_VALUE, value_str,
                            COL_WAKEUPS_CMD, formatted_cmd,
                            COL_WAKEUPS_DETAILS, formatted_details,
                            -1);

        g_free (formatted_cmd);
        g_free (formatted_details);
        g_free (value_str);
        g_free (pid_str);
    }

    g_ptr_array_unref (array);
}

static gboolean
xfpm_info_update_wakeups_idle (gpointer data)
{
    XfpmInfo *info = data;

    if ( GTK_WIDGET_VISIBLE (info->wakeups) )
        xfpm_info_update_wakeups (info);

    return TRUE;
}

static void
xfpm_info_cpu_wakeups (XfpmInfo *info)
{
    GtkWidget *vbox;
    GtkWidget *scrolled;
    GtkListStore *list_store;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    
    info->up_wakeups = up_wakeups_new ();

    list_store = gtk_list_store_new (NCOLS_WAKEUPS,
                                     G_TYPE_STRING, /*type*/
                                     G_TYPE_STRING, /*pid*/
                                     G_TYPE_STRING, /*value*/
                                     G_TYPE_STRING, /*command*/
                                     G_TYPE_STRING); /*details*/

    info->wakeups = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));

    xfpm_info_add_sidebar_icon (info, _("Processor"), XFPM_PROCESSOR_ICON);

    col = gtk_tree_view_column_new ();
    renderer = gtk_cell_renderer_pixbuf_new ();
    g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_BUTTON, NULL);
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "icon-name", COL_WAKEUPS_TYPE, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (info->wakeups), col);
    gtk_tree_view_column_set_title (col, _("Type"));

    renderer = gtk_cell_renderer_text_new ();

    col = gtk_tree_view_column_new ();
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "text", COL_WAKEUPS_PID, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (info->wakeups), col);
    /* TANSLATORS: PID, is the process id, e.g what ps x gives*/
    gtk_tree_view_column_set_title (col, _("PID"));

    col = gtk_tree_view_column_new ();
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "markup", COL_WAKEUPS_VALUE, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (info->wakeups), col);
    gtk_tree_view_column_set_title (col, _("Wakeups"));

    col = gtk_tree_view_column_new ();
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "markup", COL_WAKEUPS_CMD, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (info->wakeups), col);
    gtk_tree_view_column_set_title (col, _("Command"));

    col = gtk_tree_view_column_new ();
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "markup", COL_WAKEUPS_DETAILS, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (info->wakeups), col);
    gtk_tree_view_column_set_title (col, _("Details"));

    vbox = gtk_vbox_new (FALSE, 4);
    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scrolled), info->wakeups);

    gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);
    gtk_notebook_append_page (GTK_NOTEBOOK (info->notebook), vbox, NULL);

    xfpm_info_update_wakeups (info);

    /* TODO use data-changed and totals-changed signals? */
    info->timeout_id = g_timeout_add_seconds (4, xfpm_info_update_wakeups_idle, info);

    gtk_widget_show (vbox);
}

static void
view_cursor_changed_cb (GtkTreeView *view, XfpmInfo *info)
{
    GtkTreeSelection *sel;
    GtkTreeModel     *model;
    GtkTreeIter       selected_row;
    gint int_data = 0;

    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

    if ( !gtk_tree_selection_get_selected (sel, &model, &selected_row))
        return;

    gtk_tree_model_get(model,
                       &selected_row,
                       COL_SIDEBAR_INT,
                       &int_data,
                       -1);

    gtk_notebook_set_current_page (GTK_NOTEBOOK (info->notebook), int_data);
}

static void
xfpm_info_create (XfpmInfo *info)
{
    GtkWidget *content_area;
    GtkWidget *hbox;
    GtkWidget *viewport;
    GtkListStore *list_store;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;

    info->dialog = xfce_titled_dialog_new_with_buttons (_("Power Information"),
                                                        NULL,
                                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                                        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                                        NULL);
    gtk_window_set_position (GTK_WINDOW (info->dialog), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_default_size (GTK_WINDOW (info->dialog), -1, 400);

    gtk_window_set_icon_name (GTK_WINDOW (info->dialog), GTK_STOCK_INFO);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (info->dialog));

    hbox = gtk_hbox_new (FALSE, 4);

    gtk_container_add (GTK_CONTAINER (content_area), hbox);

    viewport = gtk_viewport_new (NULL, NULL);
    info->sideview = gtk_tree_view_new ();
    list_store = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT);

    gtk_tree_view_set_model (GTK_TREE_VIEW (info->sideview), GTK_TREE_MODEL (list_store));

    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (info->sideview),TRUE);
    col = gtk_tree_view_column_new ();

    renderer = gtk_cell_renderer_pixbuf_new ();

    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "pixbuf", 0, NULL);

    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (col, renderer, FALSE);
    gtk_tree_view_column_set_attributes (col, renderer, "markup", 1, NULL);

    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (info->sideview), FALSE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (info->sideview), col);

    g_signal_connect (info->sideview, "cursor-changed",
                      G_CALLBACK (view_cursor_changed_cb), info);

    gtk_container_add (GTK_CONTAINER (viewport), info->sideview);

    gtk_box_pack_start (GTK_BOX (hbox), viewport, FALSE, FALSE, 0);

    info->notebook = gtk_notebook_new ();
    gtk_box_pack_start (GTK_BOX (hbox), info->notebook, TRUE, TRUE, 0);
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (info->notebook), FALSE);

    /* Show power devices information, AC adapter, batteries */
    xfpm_info_power_devices (info);

    /* Show CPU wakeups */
    xfpm_info_cpu_wakeups (info);

    g_object_set (G_OBJECT (hbox),
                  "border-width", 4,
                  NULL);

    gtk_widget_show_all (hbox);
}

static XfpmInfo *
xfpm_info_new (void)
{
    return g_slice_new0 (XfpmInfo);
}

static void
xfpm_info_free (XfpmInfo *info)
{
    if ( info->timeout_id )
        g_source_remove (info->timeout_id);

    if ( info->up_wakeups )
        g_object_unref (info->up_wakeups);

    g_slice_free (XfpmInfo, info);
}

int main (int argc, char **argv)
{
    XfpmInfo *info;
    XfpmUnique *unique;

    GError *error = NULL;
    gboolean version = FALSE;

    GOptionEntry option_entries[] =
    {
        { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version, N_("Version information"), NULL },
        { NULL, },
    };

    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    g_set_application_name (PACKAGE_NAME);

    if (!gtk_init_with_args (&argc, &argv, (gchar *)"", option_entries, (gchar *)PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            g_printerr ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_printerr (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_printerr ("\n");
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    if ( version )
    {
        show_version ();
    }


    unique = xfpm_unique_new ("org.Xfce.PowerManager.Info");

    if ( !xfpm_unique_app_is_running (unique ) )
    {
        info = xfpm_info_new ();
        xfpm_info_create (info);

        g_signal_connect_swapped (unique, "ping-received",
                                  G_CALLBACK (gtk_window_present), info->dialog);

        gtk_dialog_run (GTK_DIALOG (info->dialog));

        gtk_widget_destroy (info->dialog);

        xfpm_info_free (info);
    }

    g_object_unref (unique);

    return EXIT_SUCCESS;
}
