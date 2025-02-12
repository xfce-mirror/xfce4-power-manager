/* -*- c-basic-offset: 2 -*- vi:set ts=2 sts=2 sw=2:
 * * Copyright (C) 2014 Eric Koegel <eric@xfce.org>
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
 * Based on the scale menu item implementation of the indicator applet:
 * Authors:
 *    Cody Russell <crussell@canonical.com>
 * http://bazaar.launchpad.net/~indicator-applet-developers/ido/trunk.14.10/view/head:/src/idoscalemenuitem.h
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "scalemenuitem.h"

#include <gdk/gdkkeysyms.h>



static gboolean
scale_menu_item_button_press_event (GtkWidget *menuitem,
                                    GdkEventButton *event);
static gboolean
scale_menu_item_button_release_event (GtkWidget *menuitem,
                                      GdkEventButton *event);
static gboolean
scale_menu_item_motion_notify_event (GtkWidget *menuitem,
                                     GdkEventMotion *event);
static gboolean
scale_menu_item_grab_broken (GtkWidget *menuitem,
                             GdkEventGrabBroken *event);
static void
scale_menu_item_parent_set (GtkWidget *item,
                            GtkWidget *previous_parent);
static void
update_packing (XfpmScaleMenuItem *self);



struct _XfpmScaleMenuItem
{
  GtkImageMenuItem __parent__;

  GtkWidget *scale;
  GtkWidget *description_label;
  GtkWidget *percentage_label;
  GtkWidget *vbox;
  GtkWidget *hbox;
  gboolean grabbed;
  gboolean ignore_value_changed;
};



enum
{
  SLIDER_GRABBED,
  SLIDER_RELEASED,
  VALUE_CHANGED,
  LAST_SIGNAL
};



static guint signals[LAST_SIGNAL] = { 0 };

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
G_DEFINE_TYPE (XfpmScaleMenuItem, xfpm_scale_menu_item, GTK_TYPE_IMAGE_MENU_ITEM)
G_GNUC_END_IGNORE_DEPRECATIONS



static void
scale_menu_item_scale_value_changed (GtkRange *range,
                                     gpointer user_data)
{
  XfpmScaleMenuItem *self = user_data;

  /* The signal is not sent when it was set through
   * scale_menu_item_set_value().  */

  if (!self->ignore_value_changed)
    g_signal_emit (self, signals[VALUE_CHANGED], 0, gtk_range_get_value (range));
}

static void
xfpm_scale_menu_item_class_init (XfpmScaleMenuItemClass *item_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (item_class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (item_class);

  widget_class->button_press_event = scale_menu_item_button_press_event;
  widget_class->button_release_event = scale_menu_item_button_release_event;
  widget_class->motion_notify_event = scale_menu_item_motion_notify_event;
  widget_class->grab_broken_event = scale_menu_item_grab_broken;
  widget_class->parent_set = scale_menu_item_parent_set;


  /**
   * XfpmScaleMenuItem::slider-grabbed:
   * @menuitem: The #XfpmScaleMenuItem emitting the signal.
   *
   * The ::slider-grabbed signal is emitted when the pointer selects the slider.
   */
  signals[SLIDER_GRABBED] = g_signal_new ("slider-grabbed",
                                          G_OBJECT_CLASS_TYPE (gobject_class),
                                          G_SIGNAL_RUN_FIRST,
                                          0,
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE, 0);

  /**
   * XfpmScaleMenuItem::slider-released:
   * @menuitem: The #XfpmScaleMenuItem emitting the signal.
   *
   * The ::slider-released signal is emitted when the pointer releases the slider.
   */
  signals[SLIDER_RELEASED] = g_signal_new ("slider-released",
                                           G_OBJECT_CLASS_TYPE (gobject_class),
                                           G_SIGNAL_RUN_FIRST,
                                           0,
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__VOID,
                                           G_TYPE_NONE, 0);


  /**
   * XfpmScaleMenuItem::value-changed:
   * @menuitem: the #XfpmScaleMenuItem for which the value changed
   * @value: the new value
   *
   * Emitted whenever the value of the contained scale changes because
   * of user input.
   */
  signals[VALUE_CHANGED] = g_signal_new ("value-changed",
                                         XFPM_TYPE_SCALE_MENU_ITEM,
                                         G_SIGNAL_RUN_LAST,
                                         0, NULL, NULL,
                                         g_cclosure_marshal_VOID__DOUBLE,
                                         G_TYPE_NONE,
                                         1, G_TYPE_DOUBLE);
}

static void
remove_children (GtkContainer *container)
{
  GList *children = gtk_container_get_children (container);
  GList *l;
  for (l = children; l != NULL; l = l->next)
    gtk_container_remove (container, l->data);
  g_list_free (children);
}

static void
update_packing (XfpmScaleMenuItem *self)
{
  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  if (self->hbox)
    remove_children (GTK_CONTAINER (self->hbox));
  if (self->vbox)
  {
    remove_children (GTK_CONTAINER (self->vbox));
    gtk_container_remove (GTK_CONTAINER (self), self->vbox);
  }

  self->hbox = GTK_WIDGET (hbox);
  self->vbox = GTK_WIDGET (vbox);

  /* add the new layout */
  if (self->description_label && self->percentage_label)
  {
    /* [IC]  Description
     * [ON]  <----slider----> [percentage]%
     */
    gtk_box_pack_start (GTK_BOX (vbox), self->description_label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), self->hbox, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), self->scale, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), self->percentage_label, FALSE, FALSE, 0);
  }
  else if (self->description_label)
  {
    /* [IC]  Description
     * [ON]  <----slider---->
     */
    gtk_box_pack_start (GTK_BOX (vbox), self->description_label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), self->hbox, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), self->scale, TRUE, TRUE, 0);
  }
  else if (self->percentage_label)
  {
    /* [ICON]  <----slider----> [percentage]%  */
    gtk_box_pack_start (GTK_BOX (vbox), self->hbox, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), self->scale, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), self->percentage_label, FALSE, FALSE, 0);
  }
  else
  {
    /* [ICON]  <----slider---->  */
    gtk_box_pack_start (GTK_BOX (vbox), self->hbox, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), self->scale, TRUE, TRUE, 0);
  }

  gtk_widget_show_all (self->vbox);
  gtk_widget_show_all (self->hbox);

  gtk_container_add (GTK_CONTAINER (self), self->vbox);
}

static void
xfpm_scale_menu_item_init (XfpmScaleMenuItem *self)
{
}

static gboolean
scale_menu_item_button_press_event (GtkWidget *menuitem,
                                    GdkEventButton *event)
{
  XfpmScaleMenuItem *self = XFPM_SCALE_MENU_ITEM (menuitem);
  GtkAllocation alloc;
  gint x, y;

  gtk_widget_get_allocation (self->scale, &alloc);
  gtk_widget_translate_coordinates (menuitem, self->scale, event->x, event->y, &x, &y);

  if (x > 0 && x < alloc.width && y > 0 && y < alloc.height)
    gtk_widget_event (self->scale, (GdkEvent *) event);

  if (!self->grabbed)
  {
    self->grabbed = TRUE;
    g_signal_emit (menuitem, signals[SLIDER_GRABBED], 0);
  }

  return TRUE;
}

static gboolean
scale_menu_item_button_release_event (GtkWidget *menuitem,
                                      GdkEventButton *event)
{
  XfpmScaleMenuItem *self = XFPM_SCALE_MENU_ITEM (menuitem);

  gtk_widget_event (self->scale, (GdkEvent *) event);

  if (self->grabbed)
  {
    self->grabbed = FALSE;
    g_signal_emit (menuitem, signals[SLIDER_RELEASED], 0);
  }

  return TRUE;
}

static gboolean
scale_menu_item_motion_notify_event (GtkWidget *menuitem,
                                     GdkEventMotion *event)
{
  XfpmScaleMenuItem *self = XFPM_SCALE_MENU_ITEM (menuitem);
  GtkWidget *scale = self->scale;
  GtkAllocation alloc;
  gint x, y;

  gtk_widget_get_allocation (self->scale, &alloc);
  gtk_widget_translate_coordinates (menuitem, self->scale, event->x, event->y, &x, &y);

  /* don't translate coordinates when the scale has the "grab" -
   * GtkRange expects coords relative to its event window in that case
   */
  if (!self->grabbed)
  {
    event->x = x;
    event->y = y;
  }

  if (self->grabbed || (x > 0 && x < alloc.width && y > 0 && y < alloc.height))
    gtk_widget_event (scale, (GdkEvent *) event);

  return TRUE;
}

static gboolean
scale_menu_item_grab_broken (GtkWidget *menuitem,
                             GdkEventGrabBroken *event)
{
  XfpmScaleMenuItem *self = XFPM_SCALE_MENU_ITEM (menuitem);

  GTK_WIDGET_GET_CLASS (self->scale)->grab_broken_event (self->scale, event);

  return TRUE;
}

static void
menu_hidden (GtkWidget *menu,
             XfpmScaleMenuItem *self)
{
  if (self->grabbed)
  {
    self->grabbed = FALSE;
    g_signal_emit (self, signals[SLIDER_RELEASED], 0);
  }
}

static void
scale_menu_item_parent_set (GtkWidget *item,
                            GtkWidget *previous_parent)

{
  GtkWidget *parent;

  if (previous_parent)
  {
    g_signal_handlers_disconnect_by_func (previous_parent, menu_hidden, item);
  }

  parent = gtk_widget_get_parent (item);

  if (parent)
  {
    g_signal_connect (parent, "hide", G_CALLBACK (menu_hidden), item);
  }
}



GtkWidget *
xfpm_scale_menu_item_new_with_range (gdouble min,
                                     gdouble max,
                                     gdouble step)
{
  XfpmScaleMenuItem *menuitem = XFPM_SCALE_MENU_ITEM (g_object_new (XFPM_TYPE_SCALE_MENU_ITEM, NULL));

  menuitem->scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, min, max, step);
  menuitem->vbox = NULL;
  menuitem->hbox = NULL;

  g_signal_connect (menuitem->scale, "value-changed", G_CALLBACK (scale_menu_item_scale_value_changed), menuitem);
  g_object_ref (menuitem->scale);
  gtk_widget_set_size_request (menuitem->scale, 100, -1);
  gtk_range_set_inverted (GTK_RANGE (menuitem->scale), FALSE);
  gtk_scale_set_draw_value (GTK_SCALE (menuitem->scale), FALSE);

  update_packing (menuitem);

  gtk_widget_add_events (GTK_WIDGET (menuitem),
                         GDK_SCROLL_MASK
                           | GDK_POINTER_MOTION_MASK
                           | GDK_BUTTON_MOTION_MASK);

  return GTK_WIDGET (menuitem);
}

/**
 * scale_menu_item_get_scale:
 * @menuitem: The #XfpmScaleMenuItem
 *
 * Retrieves the scale widget.
 *
 * Return Value: (transfer none)
 **/
GtkWidget *
xfpm_scale_menu_item_get_scale (XfpmScaleMenuItem *menuitem)
{
  g_return_val_if_fail (XFPM_IS_SCALE_MENU_ITEM (menuitem), NULL);

  return menuitem->scale;
}

/**
 * scale_menu_item_get_description_label:
 * @menuitem: The #XfpmScaleMenuItem
 *
 * Retrieves a string of the text for the description label widget.
 *
 * Return Value: The label text.
 **/
const gchar *
xfpm_scale_menu_item_get_description_label (XfpmScaleMenuItem *menuitem)
{
  g_return_val_if_fail (XFPM_IS_SCALE_MENU_ITEM (menuitem), NULL);

  return gtk_label_get_text (GTK_LABEL (menuitem->description_label));
}

/**
 * scale_menu_item_get_percentage_label:
 * @menuitem: The #XfpmScaleMenuItem
 *
 * Retrieves a string of the text for the percentage label widget.
 *
 * Return Value: The label text.
 **/
const gchar *
xfpm_scale_menu_item_get_percentage_label (XfpmScaleMenuItem *menuitem)
{
  g_return_val_if_fail (XFPM_IS_SCALE_MENU_ITEM (menuitem), NULL);

  return gtk_label_get_text (GTK_LABEL (menuitem->percentage_label));
}

/**
 * scale_menu_item_set_description_label:
 * @menuitem: The #XfpmScaleMenuItem
 * @label: The label text
 *
 * Sets the text for the description label widget. If label is NULL
 * then the description label is removed from the #XfpmScaleMenuItem.
 **/
void
xfpm_scale_menu_item_set_description_label (XfpmScaleMenuItem *menuitem,
                                            const gchar *label)
{
  g_return_if_fail (XFPM_IS_SCALE_MENU_ITEM (menuitem));

  if (label == NULL && menuitem->description_label)
  {
    /* remove label */
    g_object_unref (menuitem->description_label);
    menuitem->description_label = NULL;
    return;
  }

  if (menuitem->description_label && label)
  {
    gtk_label_set_markup (GTK_LABEL (menuitem->description_label), label);
  }
  else if (label)
  {
    /* create label */
    menuitem->description_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (menuitem->description_label), label);

    /* align left */
    gtk_widget_set_halign (GTK_WIDGET (menuitem->description_label), GTK_ALIGN_START);
  }

  update_packing (menuitem);
}


/**
 * scale_menu_item_set_percentage_label:
 * @menuitem: The #XfpmScaleMenuItem
 * @label: The label text
 *
 * Sets the text for the percentage label widget. If label is NULL
 * then the percentage label is removed from the #XfpmScaleMenuItem.
 **/
void
xfpm_scale_menu_item_set_percentage_label (XfpmScaleMenuItem *menuitem,
                                           const gchar *label)
{
  g_return_if_fail (XFPM_IS_SCALE_MENU_ITEM (menuitem));

  if (label == NULL && menuitem->percentage_label)
  {
    /* remove label */
    g_object_unref (menuitem->percentage_label);
    menuitem->percentage_label = NULL;
    return;
  }

  if (menuitem->percentage_label && label)
  {
    gtk_label_set_text (GTK_LABEL (menuitem->percentage_label), label);
  }
  else if (label)
  {
    /* create label */
    menuitem->percentage_label = gtk_label_new (label);
    /* align left */
    gtk_widget_set_halign (GTK_WIDGET (menuitem->percentage_label), GTK_ALIGN_START);
  }

  update_packing (menuitem);
}


/**
 *  scale_menu_item_set_value:
 *
 * Sets the value of the scale inside @item to @value, without emitting
 * "value-changed".
 */
void
xfpm_scale_menu_item_set_value (XfpmScaleMenuItem *menuitem,
                                gdouble value)
{
  /* set ignore_value_changed to signify to the scale menu item that it
   * should not emit its own value-changed signal, as that should only
   * be emitted when the value is changed by the user. */

  menuitem->ignore_value_changed = TRUE;
  gtk_range_set_value (GTK_RANGE (menuitem->scale), value);
  menuitem->ignore_value_changed = FALSE;
}
