/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <glib-object.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <fcntl.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdio.h>

#define EXIT_CODE_SUCCESS		0
#define EXIT_CODE_FAILED		1
#define EXIT_CODE_ARGUMENTS_INVALID	3
#define EXIT_CODE_INVALID_USER		4
#define EXIT_CODE_NO_BRIGHTNESS_SWITCH	5

#define BACKLIGHT_SYSFS_LOCATION	"/sys/class/backlight"
#define BRIGHTNESS_SWITCH_LOCATION	"/sys/module/video/parameters/brightness_switch_enabled"


/*
 * Find best backlight using an ordered interface list
 */
static gchar *
backlight_helper_get_best_backlight (void)
{
	gchar *filename;
	guint i;
	gboolean ret;
	GDir *dir = NULL;
	GError *error = NULL;
	const gchar *first_device;

	/* available kernel interfaces in priority order */
	static const gchar *backlight_interfaces[] = {
		"nv_backlight",
		"asus_laptop",
		"toshiba",
		"eeepc",
		"thinkpad_screen",
		"intel_backlight",
		"acpi_video1",
		"mbp_backlight",
		"acpi_video0",
		"fujitsu-laptop",
		"sony",
		"samsung",
		NULL,
	};

	/* search each one */
	for (i=0; backlight_interfaces[i] != NULL; i++) {
		filename = g_build_filename (BACKLIGHT_SYSFS_LOCATION,
					     backlight_interfaces[i], NULL);
		ret = g_file_test (filename, G_FILE_TEST_EXISTS);
		if (ret)
			goto out;
		g_free (filename);
	}

	/* nothing found in the ordered list */
	filename = NULL;

	/* find any random ones */
	dir = g_dir_open (BACKLIGHT_SYSFS_LOCATION, 0, &error);
	if (dir == NULL) {
		g_warning ("failed to find any devices: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get first device if any */
	first_device = g_dir_read_name (dir);
	if (first_device != NULL) {
		filename = g_build_filename (BACKLIGHT_SYSFS_LOCATION,
					     first_device, NULL);
	}
out:
	if (dir != NULL)
		g_dir_close (dir);
	return filename;
}

/*
 * Write a value to a sysfs entry
 */
static gboolean
backlight_helper_write (const gchar *filename, gint value, GError **error)
{
	gchar *text = NULL;
	gint retval;
	gint length;
	gint fd = -1;
	gboolean ret = TRUE;

	fd = open (filename, O_WRONLY);
	if (fd < 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to open filename: %s", filename);
		goto out;
	}

	/* convert to text */
	text = g_strdup_printf ("%i", value);
	length = strlen (text);

	/* write to device file */
	retval = write (fd, text, length);
	if (retval != length) {
		ret = FALSE;
		g_set_error (error, 1, 0, "writing '%s' to %s failed", text, filename);
		goto out;
	}
out:
	if (fd >= 0)
		close (fd);
	g_free (text);
	return ret;
}

/*
 * Backlight helper main function
 */
gint
main (gint argc, gchar *argv[])
{
	GOptionContext *context;
	gint uid;
	gint euid;
	guint retval = 0;
	const gchar *pkexec_uid_str;
	GError *error = NULL;
	gboolean ret = FALSE;
	gint set_brightness = -1;
	gboolean get_brightness = FALSE;
	gboolean get_max_brightness = FALSE;
	gint set_brightness_switch = -1;
	gboolean get_brightness_switch = FALSE;
	gchar *filename = NULL;
	gchar *filename_file = NULL;
	gchar *contents = NULL;

	const GOptionEntry options[] = {
		{ "set-brightness", '\0', 0, G_OPTION_ARG_INT, &set_brightness,
		   /* command line argument */
		  "Set the current brightness", NULL },
		{ "get-brightness", '\0', 0, G_OPTION_ARG_NONE, &get_brightness,
		   /* command line argument */
		  "Get the current brightness", NULL },
		{ "get-max-brightness", '\0', 0, G_OPTION_ARG_NONE, &get_max_brightness,
		   /* command line argument */
		  "Get the number of brightness levels supported", NULL },
		{ "set-brightness-switch", '\0', 0, G_OPTION_ARG_INT, &set_brightness_switch,
                  /* command line argument */
		  "Enable or disable ACPI video brightness switch handling", NULL },
		{ "get-brightness-switch", '\0', 0, G_OPTION_ARG_NONE, &get_brightness_switch,
                  /* command line argument */
		  "Get the current setting of the ACPI video brightness switch handling", NULL },
		{ NULL }
	};

	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, "XFCE Power Manager Backlight Helper");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* no input */
	if (set_brightness == -1 && !get_brightness && !get_max_brightness &&
	    set_brightness_switch == -1 && !get_brightness_switch) {
		puts ("No valid option was specified");
		retval = EXIT_CODE_ARGUMENTS_INVALID;
		goto out;
	}

	/* for brightness switch modifications, check for existence of the sysfs entry */
	if (set_brightness_switch != -1 || get_brightness_switch) {
		ret = g_file_test (BRIGHTNESS_SWITCH_LOCATION, G_FILE_TEST_EXISTS);
		if (!ret) {
			g_print ("Video brightness switch setting not available.\n");
			retval = EXIT_CODE_NO_BRIGHTNESS_SWITCH;
			goto out;
		}
	} else {  /* find backlight device */
		filename = backlight_helper_get_best_backlight ();
		if (filename == NULL) {
			puts ("No backlights were found on your system");
			retval = EXIT_CODE_INVALID_USER;
			goto out;
		}
	}

	/* get the current setting of the ACPI video brightness switch handling */
	if (get_brightness_switch) {
		ret = g_file_get_contents (BRIGHTNESS_SWITCH_LOCATION, &contents, NULL, &error);
		if (!ret) {
			g_print ("Could not get the value of the brightness switch: %s\n",
				 error->message);
			g_error_free (error);
			retval = EXIT_CODE_ARGUMENTS_INVALID;
			goto out;
		}

		/* just print the contents to stdout */
		g_print ("%s", contents);
		retval = EXIT_CODE_SUCCESS;
		goto out;
	}

	/* get current brightness level */
	if (get_brightness) {
		filename_file = g_build_filename (filename, "brightness", NULL);
		ret = g_file_get_contents (filename_file, &contents, NULL, &error);
		if (!ret) {
			g_print ("Could not get the value of the backlight: %s\n", error->message);
			g_error_free (error);
			retval = EXIT_CODE_ARGUMENTS_INVALID;
			goto out;
		}

		/* just print the contents to stdout */
		g_print ("%s", contents);
		retval = EXIT_CODE_SUCCESS;
		goto out;
	}

	/* get maximum brightness level */
	if (get_max_brightness) {
		filename_file = g_build_filename (filename, "max_brightness", NULL);
		ret = g_file_get_contents (filename_file, &contents, NULL, &error);
		if (!ret) {
			g_print ("Could not get the maximum value of the backlight: %s\n", error->message);
			g_error_free (error);
			retval = EXIT_CODE_ARGUMENTS_INVALID;
			goto out;
		}

		/* just print the contents to stdout */
		g_print ("%s", contents);
		retval = EXIT_CODE_SUCCESS;
		goto out;
	}

	/* get calling process */
	uid = getuid ();
	euid = geteuid ();
	if (uid != 0 || euid != 0) {
		puts ("This program can only be used by the root user");
		retval = EXIT_CODE_ARGUMENTS_INVALID;
		goto out;
	}

	/* check we're not being spoofed */
	pkexec_uid_str = g_getenv ("PKEXEC_UID");
	if (pkexec_uid_str == NULL) {
		puts ("This program must only be run through pkexec");
		retval = EXIT_CODE_INVALID_USER;
		goto out;
	}

	/* set the brightness level */
	if (set_brightness != -1) {
		filename_file = g_build_filename (filename, "brightness", NULL);
		ret = backlight_helper_write (filename_file, set_brightness, &error);
		if (!ret) {
			g_print ("Could not set the value of the backlight: %s\n", error->message);
			g_error_free (error);
			retval = EXIT_CODE_ARGUMENTS_INVALID;
			goto out;
		}
		retval = EXIT_CODE_SUCCESS;
		goto out;
	}

	/* enable or disable ACPI video brightness switch handling */
	if (set_brightness_switch != -1) {
		ret = backlight_helper_write (BRIGHTNESS_SWITCH_LOCATION,
					      set_brightness_switch, &error);
		if (!ret) {
			g_print ("Could not set the value of the brightness switch: %s\n",
				 error->message);
			g_error_free (error);
			retval = EXIT_CODE_ARGUMENTS_INVALID;
			goto out;
		}
	}

	/* success */
	retval = EXIT_CODE_SUCCESS;
out:
	g_free (filename);
	g_free (filename_file);
	g_free (contents);
	return retval;
}

