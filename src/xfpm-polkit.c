/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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

#include <dbus/dbus-glib.h>

#include "xfpm-polkit.h"
#include "xfpm-debug.h"

#include "xfpm-common.h"

static void xfpm_polkit_finalize   (GObject *object);

#define XFPM_POLKIT_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_POLKIT, XfpmPolkitPrivate))

struct XfpmPolkitPrivate
{
    DBusGConnection   *bus;

#ifdef ENABLE_POLKIT
    DBusGProxy        *proxy;
    GValueArray       *subject;
    GHashTable        *details;
    GHashTable        *subject_hash;

    GType              subject_gtype;
    GType              details_gtype;
    GType              result_gtype;

    gulong             destroy_id;
    gboolean           subject_valid;
#endif
};

enum
{
    AUTH_CHANGED,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XfpmPolkit, xfpm_polkit, G_TYPE_OBJECT)

#ifdef ENABLE_POLKIT
#if defined(__FreeBSD__)
/**
 * Taken from polkitunixprocess.c code to get process start
 * time from pid.
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 **/
static gboolean
get_kinfo_proc (pid_t pid, struct kinfo_proc *p)
{
    int mib[4];
    size_t len;
    
    len = 4;
    sysctlnametomib ("kern.proc.pid", mib, &len);
    
    len = sizeof (struct kinfo_proc);
    mib[3] = pid;
    
    if (sysctl (mib, 4, p, &len, NULL, 0) == -1)
	return FALSE;

    return TRUE;
}
#endif /*if defined(__FreeBSD__)*/

static guint64
get_start_time_for_pid (pid_t pid)
{
    guint64 start_time;
#if !defined(__FreeBSD__)
    gchar *filename;
    gchar *contents;
    size_t length;
    gchar **tokens;
    guint num_tokens;
    gchar *p;
    gchar *endp;
  
    start_time = 0;
    contents = NULL;
    
    filename = g_strdup_printf ("/proc/%d/stat", pid);
    
    if (!g_file_get_contents (filename, &contents, &length, NULL))
	goto out;

    /* start time is the token at index 19 after the '(process name)' entry - since only this
     * field can contain the ')' character, search backwards for this to avoid malicious
     * processes trying to fool us
     */
    p = strrchr (contents, ')');
    if (p == NULL)
    {
	goto out;
    }
    
    p += 2; /* skip ') ' */

    if (p - contents >= (int) length)
    {
	g_warning ("Error parsing file %s", filename);
	goto out;
    }
    
    tokens = g_strsplit (p, " ", 0);
    
    num_tokens = g_strv_length (tokens);
    
    if (num_tokens < 20)
    {
	g_warning ("Error parsing file %s", filename);
	goto out;
    }

    start_time = strtoull (tokens[19], &endp, 10);
    if (endp == tokens[19])
    {
	g_warning ("Error parsing file %s", filename);
	goto out;
    }
    g_strfreev (tokens);

 out:
    g_free (filename);
    g_free (contents);
    
#else /*if !defined(__FreeBSD__)*/

    struct kinfo_proc p;
    
    start_time = 0;

    if (! get_kinfo_proc (pid, &p))
    {
	g_warning ("Error obtaining start time for %d (%s)",
		   (gint) pid,
		   g_strerror (errno));
	goto out;
    }

    start_time = (guint64) p.ki_start.tv_sec;
    
out:
#endif
    
    return start_time;
}
#endif /*ENABLE_POLKIT*/


#ifdef ENABLE_POLKIT
static gboolean
xfpm_polkit_free_data (gpointer data)
{
    XfpmPolkit *polkit;
    
    polkit = XFPM_POLKIT (data);

    g_assert (polkit->priv->subject_valid);

    XFPM_DEBUG ("Destroying Polkit data");

    g_hash_table_destroy (polkit->priv->details);
    g_hash_table_destroy (polkit->priv->subject_hash);
    g_value_array_free   (polkit->priv->subject);
    
    polkit->priv->details      = NULL;
    polkit->priv->subject_hash = NULL;
    polkit->priv->subject      = NULL;

    polkit->priv->destroy_id = 0;
    polkit->priv->subject_valid = FALSE;
    
    return FALSE;
}

static void
xfpm_polkit_init_data (XfpmPolkit *polkit)
{
    //const gchar *consolekit_cookie;
    GValue hash_elem = { 0 };
    //gboolean subject_created = FALSE;

    if (polkit->priv->subject_valid)
	return;
    
    /**
     * This variable should be set by the session manager or by 
     * the login manager (gdm?). under clean Xfce environment
     * it is set by the session manager (4.8 and above)  
     * since we don't have a login manager, yet!
     **/
     /*
      *	
      *	Disable for the moment
      * 
    consolekit_cookie = g_getenv ("XDG_SESSION_COOKIE");
  
    if ( consolekit_cookie )
    {
	DBusGProxy *proxy;
	GError *error = NULL;
	gboolean ret;
	gchar *consolekit_session;
	
	proxy  = dbus_g_proxy_new_for_name_owner (polkit->priv->bus,
						  "org.freedesktop.ConsoleKit",
						  "/org/freedesktop/ConsoleKit/Manager",
						  "org.freedesktop.ConsoleKit.Manager",
						  NULL);

	if ( proxy )
	{
	    ret = dbus_g_proxy_call (proxy, "GetSessionForCookie", &error,
				     G_TYPE_STRING, consolekit_cookie,
				     G_TYPE_INVALID,
				     DBUS_TYPE_G_OBJECT_PATH, &consolekit_session,
				     G_TYPE_INVALID);
	    
	    if ( G_LIKELY (ret) )
	    {
		GValue val  = { 0 };
		
		polkit->priv->subject = g_value_array_new (2);
		polkit->priv->subject_hash = g_hash_table_new_full (g_str_hash, 
								    g_str_equal, 
								    g_free, 
								    NULL);
		g_value_init (&val, G_TYPE_STRING);
		g_value_set_string (&val, "unix-session");
		g_value_array_append (polkit->priv->subject, &val);
		
		g_value_unset (&val);
		g_value_init (&val, G_TYPE_STRING);
		g_value_set_string (&val, consolekit_session);
		
		g_hash_table_insert (polkit->priv->subject_hash, 
				     g_strdup ("session-id"), 
				     &val);
		
		g_free (consolekit_session);
		XFPM_DEBUG ("Using ConsoleKit session Polkit subject");
		subject_created = TRUE;
	    }
	    g_object_unref (proxy);
	}
	else if (error)
	{
	    g_warning ("'GetSessionForCookie' failed : %s", error->message);
	    g_error_free (error);
	}
	
    }
    */
    
    //if ( subject_created == FALSE )
    {
	gint pid;
	guint64 start_time;
    
	pid = getpid ();
	
	start_time = get_start_time_for_pid (pid);
	
	if ( G_LIKELY (start_time != 0 ) )
	{
	    GValue val = { 0 }, pid_val = { 0 }, start_time_val = { 0 };
	    
	    polkit->priv->subject = g_value_array_new (2);
	    polkit->priv->subject_hash = g_hash_table_new_full (g_str_hash, 
								g_str_equal, 
								g_free, 
								NULL);
	
	    g_value_init (&val, G_TYPE_STRING);
	    g_value_set_string (&val, "unix-process");
	    g_value_array_append (polkit->priv->subject, &val);
	    
	    g_value_unset (&val);
	    
	    g_value_init (&pid_val, G_TYPE_UINT);
	    g_value_set_uint (&pid_val, pid);
	    g_hash_table_insert (polkit->priv->subject_hash, 
				 g_strdup ("pid"), &pid_val);
	    
	    g_value_init (&start_time_val, G_TYPE_UINT64);
	    g_value_set_uint64 (&start_time_val, start_time);
	    g_hash_table_insert (polkit->priv->subject_hash, 
				 g_strdup ("start-time"), &start_time_val);
	    
	    XFPM_DEBUG ("Using unix session polkit subject");
	}
	else
	{
	    g_warning ("Unable to create polkit subject");
	}
    }
    
    g_value_init (&hash_elem, 
		  dbus_g_type_get_map ("GHashTable", 
				       G_TYPE_STRING, 
				       G_TYPE_VALUE));
    
    g_value_set_static_boxed (&hash_elem, polkit->priv->subject_hash);
    g_value_array_append (polkit->priv->subject, &hash_elem);
    
    /**
     * Polkit details, will leave it empty.
     **/
    polkit->priv->details = g_hash_table_new_full (g_str_hash, 
						   g_str_equal, 
						   g_free, 
						   g_free);
    
    /*Clean these data after 1 minute*/
    polkit->priv->destroy_id = 
	g_timeout_add_seconds (60, (GSourceFunc) xfpm_polkit_free_data, polkit);
    
    polkit->priv->subject_valid = TRUE;
}
#endif /*ENABLE_POLKIT*/

static gboolean
xfpm_polkit_check_auth_intern (XfpmPolkit *polkit, const gchar *action_id)
{
#ifdef ENABLE_POLKIT
    GValueArray *result;
    GValue result_val = { 0 };
    GError *error = NULL;
    gboolean is_authorized = FALSE;
    gboolean ret;
    
    /**
     * <method name="CheckAuthorization">      
     *   <arg type="(sa{sv})" name="subject" direction="in"/>      
     *   <arg type="s" name="action_id" direction="in"/>           
     *   <arg type="a{ss}" name="details" direction="in"/>         
     *   <arg type="u" name="flags" direction="in"/>               
     *   <arg type="s" name="cancellation_id" direction="in"/>     
     *   <arg type="(bba{ss})" name="result" direction="out"/>     
     * </method>
     *
     **/
    
    g_return_val_if_fail (polkit->priv->proxy != NULL, FALSE);
    g_return_val_if_fail (polkit->priv->subject_valid, FALSE);
     
    result = g_value_array_new (0);
    
    ret = dbus_g_proxy_call (polkit->priv->proxy, "CheckAuthorization", &error,
			     polkit->priv->subject_gtype, polkit->priv->subject,
			     G_TYPE_STRING, action_id,
			     polkit->priv->details_gtype, polkit->priv->details,
			     G_TYPE_UINT, 0, 
			     G_TYPE_STRING, NULL,
			     G_TYPE_INVALID,
			     polkit->priv->result_gtype, &result,
			     G_TYPE_INVALID);
    
    if ( G_LIKELY (ret) )
    {
	g_value_init (&result_val, polkit->priv->result_gtype);
	g_value_set_static_boxed (&result_val, result);
	
	dbus_g_type_struct_get (&result_val,
				0, &is_authorized,
				G_MAXUINT);
	g_value_unset (&result_val);
    }
    else if ( error )
    {
	g_warning ("'CheckAuthorization' failed with %s", error->message);
	g_error_free (error);
    }

    g_value_array_free (result);
    
    XFPM_DEBUG ("Action=%s is authorized=%s", action_id, xfpm_bool_to_string (is_authorized));
    
    return is_authorized;
#endif /*ENABLE_POLKIT*/
    return TRUE;
}

#ifdef ENABLE_POLKIT
static void
xfpm_polkit_changed_cb (DBusGProxy *proxy, XfpmPolkit *polkit)
{
    XFPM_DEBUG ("Auth changed");
    g_signal_emit (G_OBJECT (polkit), signals [AUTH_CHANGED], 0);
}
#endif

static void
xfpm_polkit_class_init (XfpmPolkitClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_polkit_finalize;

    signals [AUTH_CHANGED] = 
        g_signal_new ("auth-changed",
                      XFPM_TYPE_POLKIT,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmPolkitClass, auth_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    g_type_class_add_private (klass, sizeof (XfpmPolkitPrivate));
}

static void
xfpm_polkit_init (XfpmPolkit *polkit)
{
    GError *error = NULL;
    
    polkit->priv = XFPM_POLKIT_GET_PRIVATE (polkit);

#ifdef ENABLE_POLKIT
    polkit->priv->destroy_id   = 0;
    polkit->priv->subject_valid   = FALSE;
    polkit->priv->proxy        = NULL;
    polkit->priv->subject      = NULL;
    polkit->priv->details      = NULL;
    polkit->priv->subject_hash = NULL;
    
    polkit->priv->subject_gtype = 
        dbus_g_type_get_struct ("GValueArray", 
                                G_TYPE_STRING, 
                                dbus_g_type_get_map ("GHashTable", 
                                                     G_TYPE_STRING, 
                                                     G_TYPE_VALUE),
                                G_TYPE_INVALID);
    
    polkit->priv->details_gtype = dbus_g_type_get_map ("GHashTable", 
						       G_TYPE_STRING, 
						       G_TYPE_STRING);
    
    polkit->priv->result_gtype =
	dbus_g_type_get_struct ("GValueArray", 
				G_TYPE_BOOLEAN, 
				G_TYPE_BOOLEAN, 
				dbus_g_type_get_map ("GHashTable", 
						     G_TYPE_STRING, 
						     G_TYPE_STRING),
				G_TYPE_INVALID);
#endif /*ENABLE_POLKIT*/
    
    polkit->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    
    if ( error )
    {
	g_critical ("Error getting system bus connection : %s", error->message);
	g_error_free (error);
	goto out;
    }

#ifdef ENABLE_POLKIT
    polkit->priv->proxy = 
	dbus_g_proxy_new_for_name (polkit->priv->bus,
				   "org.freedesktop.PolicyKit1",
				   "/org/freedesktop/PolicyKit1/Authority",
				   "org.freedesktop.PolicyKit1.Authority");
    
    if (G_LIKELY (polkit->priv->proxy) )
    {
	dbus_g_proxy_add_signal (polkit->priv->proxy, "Changed", G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (polkit->priv->proxy, "Changed",
				     G_CALLBACK (xfpm_polkit_changed_cb), polkit, NULL);
    }
    else
    {
	g_warning ("Failed to create proxy for 'org.freedesktop.PolicyKit1'");
    }
#endif /*ENABLE_POLKIT*/

out:
    ;
}

static void
xfpm_polkit_finalize (GObject *object)
{
    XfpmPolkit *polkit;

    polkit = XFPM_POLKIT (object);

#ifdef ENABLE_POLKIT
    if ( polkit->priv->proxy )
    {
	dbus_g_proxy_disconnect_signal (polkit->priv->proxy, "Changed",
					G_CALLBACK (xfpm_polkit_changed_cb), polkit);
	g_object_unref (polkit->priv->proxy);
    }

    if ( polkit->priv->subject_valid )
    {
	xfpm_polkit_free_data (polkit);
	if (polkit->priv->destroy_id != 0 )
	    g_source_remove (polkit->priv->destroy_id);
    }
#endif /*ENABLE_POLKIT*/


    if ( polkit->priv->bus )
	dbus_g_connection_unref (polkit->priv->bus);

    G_OBJECT_CLASS (xfpm_polkit_parent_class)->finalize (object);
}

XfpmPolkit *
xfpm_polkit_get (void)
{
    static gpointer xfpm_polkit_obj = NULL;
    
    if ( G_LIKELY (xfpm_polkit_obj) )
    {
	g_object_ref (xfpm_polkit_obj);
    }
    else
    {
	xfpm_polkit_obj = g_object_new (XFPM_TYPE_POLKIT, NULL);
	g_object_add_weak_pointer (xfpm_polkit_obj, &xfpm_polkit_obj);
    }
    
    return XFPM_POLKIT (xfpm_polkit_obj);
}

gboolean xfpm_polkit_check_auth	(XfpmPolkit *polkit, const gchar *action_id)
{
#ifdef ENABLE_POLKIT
    xfpm_polkit_init_data (polkit);
#endif
    return xfpm_polkit_check_auth_intern (polkit, action_id);
}
