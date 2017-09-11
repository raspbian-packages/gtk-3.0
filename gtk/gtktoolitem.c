/* gtktoolitem.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtktoolitem.h"

#include <string.h>

#include "gtkmarshalers.h"
#include "gtktoolshell.h"
#include "gtkseparatormenuitem.h"
#include "gtksizerequest.h"
#include "deprecated/gtkactivatable.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

/**
 * SECTION:gtktoolitem
 * @short_description: The base class of widgets that can be added to GtkToolShell
 * @Title: GtkToolItem
 * @see_also: #GtkToolbar, #GtkToolButton, #GtkSeparatorToolItem
 *
 * #GtkToolItems are widgets that can appear on a toolbar. To
 * create a toolbar item that contain something else than a button, use
 * gtk_tool_item_new(). Use gtk_container_add() to add a child
 * widget to the tool item.
 *
 * For toolbar items that contain buttons, see the #GtkToolButton,
 * #GtkToggleToolButton and #GtkRadioToolButton classes.
 *
 * See the #GtkToolbar class for a description of the toolbar widget, and
 * #GtkToolShell for a description of the tool shell interface.
 */

/**
 * GtkToolItem:
 *
 * The GtkToolItem struct contains only private data.
 * It should only be accessed through the functions described below.
 */

enum {
  CREATE_MENU_PROXY,
  TOOLBAR_RECONFIGURED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_VISIBLE_HORIZONTAL,
  PROP_VISIBLE_VERTICAL,
  PROP_IS_IMPORTANT,

  /* activatable properties */
  PROP_ACTIVATABLE_RELATED_ACTION,
  PROP_ACTIVATABLE_USE_ACTION_APPEARANCE
};


struct _GtkToolItemPrivate
{
  gchar *tip_text;
  gchar *tip_private;

  guint visible_horizontal    : 1;
  guint visible_vertical      : 1;
  guint homogeneous           : 1;
  guint expand                : 1;
  guint use_drag_window       : 1;
  guint is_important          : 1;
  guint use_action_appearance : 1;

  GdkWindow *drag_window;
  gchar *menu_item_id;
  GtkWidget *menu_item;

  GtkAction *action;
};

static void gtk_tool_item_finalize     (GObject         *object);
static void gtk_tool_item_dispose      (GObject         *object);
static void gtk_tool_item_parent_set   (GtkWidget       *toolitem,
				        GtkWidget       *parent);
static void gtk_tool_item_set_property (GObject         *object,
					guint            prop_id,
					const GValue    *value,
					GParamSpec      *pspec);
static void gtk_tool_item_get_property (GObject         *object,
					guint            prop_id,
					GValue          *value,
					GParamSpec      *pspec);
static void gtk_tool_item_property_notify (GObject      *object,
					   GParamSpec   *pspec);
static void gtk_tool_item_realize       (GtkWidget      *widget);
static void gtk_tool_item_unrealize     (GtkWidget      *widget);
static void gtk_tool_item_map           (GtkWidget      *widget);
static void gtk_tool_item_unmap         (GtkWidget      *widget);
static void gtk_tool_item_get_preferred_width
                                        (GtkWidget      *widget,
                                         gint           *minimum,
                                         gint           *natural);
static void gtk_tool_item_get_preferred_height
                                        (GtkWidget      *widget,
                                         gint           *minimum,
                                         gint           *natural);
static void gtk_tool_item_size_allocate (GtkWidget      *widget,
					 GtkAllocation  *allocation);

static void gtk_tool_item_activatable_interface_init (GtkActivatableIface  *iface);
static void gtk_tool_item_update                     (GtkActivatable       *activatable,
						      GtkAction            *action,
						      const gchar          *property_name);
static void gtk_tool_item_sync_action_properties     (GtkActivatable       *activatable,
						      GtkAction            *action);
static void gtk_tool_item_set_related_action         (GtkToolItem          *item, 
						      GtkAction            *action);
static void gtk_tool_item_set_use_action_appearance  (GtkToolItem          *item, 
						      gboolean              use_appearance);

static guint toolitem_signals[LAST_SIGNAL] = { 0 };

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
G_DEFINE_TYPE_WITH_CODE (GtkToolItem, gtk_tool_item, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkToolItem)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
						gtk_tool_item_activatable_interface_init))
G_GNUC_END_IGNORE_DEPRECATIONS;

static void
gtk_tool_item_class_init (GtkToolItemClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;

  object_class->set_property = gtk_tool_item_set_property;
  object_class->get_property = gtk_tool_item_get_property;
  object_class->finalize     = gtk_tool_item_finalize;
  object_class->dispose      = gtk_tool_item_dispose;
  object_class->notify       = gtk_tool_item_property_notify;

  widget_class->realize       = gtk_tool_item_realize;
  widget_class->unrealize     = gtk_tool_item_unrealize;
  widget_class->map           = gtk_tool_item_map;
  widget_class->unmap         = gtk_tool_item_unmap;
  widget_class->get_preferred_width = gtk_tool_item_get_preferred_width;
  widget_class->get_preferred_height = gtk_tool_item_get_preferred_height;
  widget_class->size_allocate = gtk_tool_item_size_allocate;
  widget_class->parent_set    = gtk_tool_item_parent_set;

  gtk_container_class_handle_border_width (GTK_CONTAINER_CLASS (klass));

  klass->create_menu_proxy = _gtk_tool_item_create_menu_proxy;
  
  g_object_class_install_property (object_class,
				   PROP_VISIBLE_HORIZONTAL,
				   g_param_spec_boolean ("visible-horizontal",
							 P_("Visible when horizontal"),
							 P_("Whether the toolbar item is visible when the toolbar is in a horizontal orientation."),
							 TRUE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class,
				   PROP_VISIBLE_VERTICAL,
				   g_param_spec_boolean ("visible-vertical",
							 P_("Visible when vertical"),
							 P_("Whether the toolbar item is visible when the toolbar is in a vertical orientation."),
							 TRUE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class,
 				   PROP_IS_IMPORTANT,
 				   g_param_spec_boolean ("is-important",
 							 P_("Is important"),
 							 P_("Whether the toolbar item is considered important. When TRUE, toolbar buttons show text in GTK_TOOLBAR_BOTH_HORIZ mode"),
 							 FALSE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_override_property (object_class, PROP_ACTIVATABLE_RELATED_ACTION, "related-action");
  g_object_class_override_property (object_class, PROP_ACTIVATABLE_USE_ACTION_APPEARANCE, "use-action-appearance");


/**
 * GtkToolItem::create-menu-proxy:
 * @tool_item: the object the signal was emitted on
 *
 * This signal is emitted when the toolbar needs information from @tool_item
 * about whether the item should appear in the toolbar overflow menu. In
 * response the tool item should either
 * 
 * - call gtk_tool_item_set_proxy_menu_item() with a %NULL
 *   pointer and return %TRUE to indicate that the item should not appear
 *   in the overflow menu
 * 
 * - call gtk_tool_item_set_proxy_menu_item() with a new menu
 *   item and return %TRUE, or 
 *
 * - return %FALSE to indicate that the signal was not handled by the item.
 *   This means that the item will not appear in the overflow menu unless
 *   a later handler installs a menu item.
 *
 * The toolbar may cache the result of this signal. When the tool item changes
 * how it will respond to this signal it must call gtk_tool_item_rebuild_menu()
 * to invalidate the cache and ensure that the toolbar rebuilds its overflow
 * menu.
 *
 * Returns: %TRUE if the signal was handled, %FALSE if not
 **/
  toolitem_signals[CREATE_MENU_PROXY] =
    g_signal_new (I_("create-menu-proxy"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkToolItemClass, create_menu_proxy),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

/**
 * GtkToolItem::toolbar-reconfigured:
 * @tool_item: the object the signal was emitted on
 *
 * This signal is emitted when some property of the toolbar that the
 * item is a child of changes. For custom subclasses of #GtkToolItem,
 * the default handler of this signal use the functions
 * - gtk_tool_shell_get_orientation()
 * - gtk_tool_shell_get_style()
 * - gtk_tool_shell_get_icon_size()
 * - gtk_tool_shell_get_relief_style()
 * to find out what the toolbar should look like and change
 * themselves accordingly.
 **/
  toolitem_signals[TOOLBAR_RECONFIGURED] =
    g_signal_new (I_("toolbar-reconfigured"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkToolItemClass, toolbar_reconfigured),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, "toolitem");
}

static void
gtk_tool_item_init (GtkToolItem *toolitem)
{
  gtk_widget_set_can_focus (GTK_WIDGET (toolitem), FALSE);

  toolitem->priv = gtk_tool_item_get_instance_private (toolitem);
  toolitem->priv->visible_horizontal = TRUE;
  toolitem->priv->visible_vertical = TRUE;
  toolitem->priv->homogeneous = FALSE;
  toolitem->priv->expand = FALSE;
  toolitem->priv->use_action_appearance = TRUE;
}

static void
gtk_tool_item_finalize (GObject *object)
{
  GtkToolItem *item = GTK_TOOL_ITEM (object);

  g_free (item->priv->menu_item_id);

  if (item->priv->menu_item)
    g_object_unref (item->priv->menu_item);

  G_OBJECT_CLASS (gtk_tool_item_parent_class)->finalize (object);
}

static void
gtk_tool_item_dispose (GObject *object)
{
  GtkToolItem *item = GTK_TOOL_ITEM (object);

  if (item->priv->action)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (item), NULL);      
      G_GNUC_END_IGNORE_DEPRECATIONS;
      item->priv->action = NULL;
    }
  G_OBJECT_CLASS (gtk_tool_item_parent_class)->dispose (object);
}


static void
gtk_tool_item_parent_set (GtkWidget   *toolitem,
			  GtkWidget   *prev_parent)
{
  if (gtk_widget_get_parent (GTK_WIDGET (toolitem)) != NULL)
    gtk_tool_item_toolbar_reconfigured (GTK_TOOL_ITEM (toolitem));
}

static void
gtk_tool_item_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  GtkToolItem *toolitem = GTK_TOOL_ITEM (object);

  switch (prop_id)
    {
    case PROP_VISIBLE_HORIZONTAL:
      gtk_tool_item_set_visible_horizontal (toolitem, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE_VERTICAL:
      gtk_tool_item_set_visible_vertical (toolitem, g_value_get_boolean (value));
      break;
    case PROP_IS_IMPORTANT:
      gtk_tool_item_set_is_important (toolitem, g_value_get_boolean (value));
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      gtk_tool_item_set_related_action (toolitem, g_value_get_object (value));
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      gtk_tool_item_set_use_action_appearance (toolitem, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tool_item_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  GtkToolItem *toolitem = GTK_TOOL_ITEM (object);

  switch (prop_id)
    {
    case PROP_VISIBLE_HORIZONTAL:
      g_value_set_boolean (value, toolitem->priv->visible_horizontal);
      break;
    case PROP_VISIBLE_VERTICAL:
      g_value_set_boolean (value, toolitem->priv->visible_vertical);
      break;
    case PROP_IS_IMPORTANT:
      g_value_set_boolean (value, toolitem->priv->is_important);
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      g_value_set_object (value, toolitem->priv->action);
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      g_value_set_boolean (value, toolitem->priv->use_action_appearance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tool_item_property_notify (GObject    *object,
			       GParamSpec *pspec)
{
  GtkToolItem *tool_item = GTK_TOOL_ITEM (object);

  if (tool_item->priv->menu_item && strcmp (pspec->name, "sensitive") == 0)
    gtk_widget_set_sensitive (tool_item->priv->menu_item,
			      gtk_widget_get_sensitive (GTK_WIDGET (tool_item)));

  if (G_OBJECT_CLASS (gtk_tool_item_parent_class)->notify)
    G_OBJECT_CLASS (gtk_tool_item_parent_class)->notify (object, pspec);
}

static void
create_drag_window (GtkToolItem *toolitem)
{
  GtkAllocation allocation;
  GtkWidget *widget;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (toolitem->priv->use_drag_window == TRUE);

  widget = GTK_WIDGET (toolitem);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  toolitem->priv->drag_window = gdk_window_new (gtk_widget_get_parent_window (widget),
					  &attributes, attributes_mask);
  gtk_widget_register_window (widget, toolitem->priv->drag_window);
}

static void
gtk_tool_item_realize (GtkWidget *widget)
{
  GtkToolItem *toolitem;
  GdkWindow *window;

  toolitem = GTK_TOOL_ITEM (widget);
  gtk_widget_set_realized (widget, TRUE);

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  if (toolitem->priv->use_drag_window)
    create_drag_window(toolitem);
}

static void
destroy_drag_window (GtkToolItem *toolitem)
{
  if (toolitem->priv->drag_window)
    {
      gtk_widget_unregister_window (GTK_WIDGET (toolitem), toolitem->priv->drag_window);
      gdk_window_destroy (toolitem->priv->drag_window);
      toolitem->priv->drag_window = NULL;
    }
}

static void
gtk_tool_item_unrealize (GtkWidget *widget)
{
  GtkToolItem *toolitem;

  toolitem = GTK_TOOL_ITEM (widget);

  destroy_drag_window (toolitem);
  
  GTK_WIDGET_CLASS (gtk_tool_item_parent_class)->unrealize (widget);
}

static void
gtk_tool_item_map (GtkWidget *widget)
{
  GtkToolItem *toolitem;

  toolitem = GTK_TOOL_ITEM (widget);
  GTK_WIDGET_CLASS (gtk_tool_item_parent_class)->map (widget);
  if (toolitem->priv->drag_window)
    gdk_window_show (toolitem->priv->drag_window);
}

static void
gtk_tool_item_unmap (GtkWidget *widget)
{
  GtkToolItem *toolitem;

  toolitem = GTK_TOOL_ITEM (widget);
  if (toolitem->priv->drag_window)
    gdk_window_hide (toolitem->priv->drag_window);
  GTK_WIDGET_CLASS (gtk_tool_item_parent_class)->unmap (widget);
}

static void
gtk_tool_item_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
  GtkWidget *child;

  *minimum = *natural = 0;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    gtk_widget_get_preferred_width (child, minimum, natural);
}

static void
gtk_tool_item_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  GtkWidget *child;

  *minimum = *natural = 0;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    gtk_widget_get_preferred_height (child, minimum, natural);
}

static void
gtk_tool_item_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkToolItem *toolitem = GTK_TOOL_ITEM (widget);
  GtkAllocation child_allocation;
  GtkWidget *child;

  gtk_widget_set_allocation (widget, allocation);

  if (toolitem->priv->drag_window)
    gdk_window_move_resize (toolitem->priv->drag_window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      child_allocation.x = allocation->x;
      child_allocation.y = allocation->y;
      child_allocation.width = allocation->width;
      child_allocation.height = allocation->height;
      
      gtk_widget_size_allocate (child, &child_allocation);
    }

  _gtk_widget_set_simple_clip (widget, NULL);
}

gboolean
_gtk_tool_item_create_menu_proxy (GtkToolItem *item)
{
  GtkWidget *menu_item;
  gboolean visible_overflown;
  gboolean ret = FALSE;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (item->priv->action)
    {
      g_object_get (item->priv->action, "visible-overflown", &visible_overflown, NULL);
    
      if (visible_overflown)
	{
	  menu_item = gtk_action_create_menu_item (item->priv->action);

	  g_object_ref_sink (menu_item);
      	  gtk_tool_item_set_proxy_menu_item (item, "gtk-action-menu-item", menu_item);
	  g_object_unref (menu_item);
	}
      else
	gtk_tool_item_set_proxy_menu_item (item, "gtk-action-menu-item", NULL);

      ret = TRUE;
    }

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return ret;
}

static void
gtk_tool_item_activatable_interface_init (GtkActivatableIface *iface)
{
  iface->update = gtk_tool_item_update;
  iface->sync_action_properties = gtk_tool_item_sync_action_properties;
}

static void
gtk_tool_item_update (GtkActivatable *activatable,
		      GtkAction      *action,
	     	      const gchar    *property_name)
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (strcmp (property_name, "visible") == 0)
    {
      if (gtk_action_is_visible (action))
	gtk_widget_show (GTK_WIDGET (activatable));
      else
	gtk_widget_hide (GTK_WIDGET (activatable));
    }
  else if (strcmp (property_name, "sensitive") == 0)
    gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));
  else if (strcmp (property_name, "tooltip") == 0)
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (activatable),
				    gtk_action_get_tooltip (action));
  else if (strcmp (property_name, "visible-horizontal") == 0)
    gtk_tool_item_set_visible_horizontal (GTK_TOOL_ITEM (activatable),
					  gtk_action_get_visible_horizontal (action));
  else if (strcmp (property_name, "visible-vertical") == 0)
    gtk_tool_item_set_visible_vertical (GTK_TOOL_ITEM (activatable),
					gtk_action_get_visible_vertical (action));
  else if (strcmp (property_name, "is-important") == 0)
    gtk_tool_item_set_is_important (GTK_TOOL_ITEM (activatable),
				    gtk_action_get_is_important (action));

  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
gtk_tool_item_sync_action_properties (GtkActivatable *activatable,
				      GtkAction      *action)
{
  if (!action)
    return;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (gtk_action_is_visible (action))
    gtk_widget_show (GTK_WIDGET (activatable));
  else
    gtk_widget_hide (GTK_WIDGET (activatable));
  
  gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));
  
  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (activatable),
				  gtk_action_get_tooltip (action));
  gtk_tool_item_set_visible_horizontal (GTK_TOOL_ITEM (activatable),
					gtk_action_get_visible_horizontal (action));
  gtk_tool_item_set_visible_vertical (GTK_TOOL_ITEM (activatable),
				      gtk_action_get_visible_vertical (action));
  gtk_tool_item_set_is_important (GTK_TOOL_ITEM (activatable),
				  gtk_action_get_is_important (action));

  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
gtk_tool_item_set_related_action (GtkToolItem *item, 
				  GtkAction   *action)
{
  if (item->priv->action == action)
    return;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (item), action);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  item->priv->action = action;

  if (action)
    {
      gtk_tool_item_rebuild_menu (item);
    }
}

static void
gtk_tool_item_set_use_action_appearance (GtkToolItem *item,
					 gboolean     use_appearance)
{
  if (item->priv->use_action_appearance != use_appearance)
    {
      item->priv->use_action_appearance = use_appearance;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_activatable_sync_action_properties (GTK_ACTIVATABLE (item), item->priv->action);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}


/**
 * gtk_tool_item_new:
 * 
 * Creates a new #GtkToolItem
 * 
 * Returns: the new #GtkToolItem
 * 
 * Since: 2.4
 **/
GtkToolItem *
gtk_tool_item_new (void)
{
  GtkToolItem *item;

  item = g_object_new (GTK_TYPE_TOOL_ITEM, NULL);

  return item;
}

/**
 * gtk_tool_item_get_ellipsize_mode:
 * @tool_item: a #GtkToolItem
 *
 * Returns the ellipsize mode used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function to find out how text should
 * be ellipsized.
 *
 * Returns: a #PangoEllipsizeMode indicating how text in @tool_item
 * should be ellipsized.
 *
 * Since: 2.20
 **/
PangoEllipsizeMode
gtk_tool_item_get_ellipsize_mode (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), PANGO_ELLIPSIZE_NONE);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return PANGO_ELLIPSIZE_NONE;

  return gtk_tool_shell_get_ellipsize_mode (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_icon_size:
 * @tool_item: a #GtkToolItem
 * 
 * Returns the icon size used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function to find out what size icons
 * they should use.
 * 
 * Returns: (type int): a #GtkIconSize indicating the icon size
 * used for @tool_item
 * 
 * Since: 2.4
 **/
GtkIconSize
gtk_tool_item_get_icon_size (GtkToolItem *tool_item)
{
  GtkWidget *parent;

  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_ICON_SIZE_LARGE_TOOLBAR);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return GTK_ICON_SIZE_LARGE_TOOLBAR;

  return gtk_tool_shell_get_icon_size (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_orientation:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns the orientation used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function to find out what size icons
 * they should use.
 * 
 * Returns: a #GtkOrientation indicating the orientation
 * used for @tool_item
 * 
 * Since: 2.4
 **/
GtkOrientation
gtk_tool_item_get_orientation (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_ORIENTATION_HORIZONTAL);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return GTK_ORIENTATION_HORIZONTAL;

  return gtk_tool_shell_get_orientation (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_toolbar_style:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns the toolbar style used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function in the handler of the
 * GtkToolItem::toolbar_reconfigured signal to find out in what style
 * the toolbar is displayed and change themselves accordingly 
 *
 * Possibilities are:
 * - %GTK_TOOLBAR_BOTH, meaning the tool item should show
 *   both an icon and a label, stacked vertically
 * - %GTK_TOOLBAR_ICONS, meaning the toolbar shows only icons
 * - %GTK_TOOLBAR_TEXT, meaning the tool item should only show text
 * - %GTK_TOOLBAR_BOTH_HORIZ, meaning the tool item should show
 *   both an icon and a label, arranged horizontally
 * 
 * Returns: A #GtkToolbarStyle indicating the toolbar style used
 * for @tool_item.
 * 
 * Since: 2.4
 **/
GtkToolbarStyle
gtk_tool_item_get_toolbar_style (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_TOOLBAR_ICONS);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return GTK_TOOLBAR_ICONS;

  return gtk_tool_shell_get_style (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_relief_style:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns the relief style of @tool_item. See gtk_button_set_relief().
 * Custom subclasses of #GtkToolItem should call this function in the handler
 * of the #GtkToolItem::toolbar_reconfigured signal to find out the
 * relief style of buttons.
 * 
 * Returns: a #GtkReliefStyle indicating the relief style used
 * for @tool_item.
 * 
 * Since: 2.4
 **/
GtkReliefStyle 
gtk_tool_item_get_relief_style (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_RELIEF_NONE);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return GTK_RELIEF_NONE;

  return gtk_tool_shell_get_relief_style (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_text_alignment:
 * @tool_item: a #GtkToolItem: 
 * 
 * Returns the text alignment used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function to find out how text should
 * be aligned.
 * 
 * Returns: a #gfloat indicating the horizontal text alignment
 * used for @tool_item
 * 
 * Since: 2.20
 **/
gfloat
gtk_tool_item_get_text_alignment (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_ORIENTATION_HORIZONTAL);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return 0.5;

  return gtk_tool_shell_get_text_alignment (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_text_orientation:
 * @tool_item: a #GtkToolItem
 *
 * Returns the text orientation used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function to find out how text should
 * be orientated.
 *
 * Returns: a #GtkOrientation indicating the text orientation
 * used for @tool_item
 *
 * Since: 2.20
 */
GtkOrientation
gtk_tool_item_get_text_orientation (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_ORIENTATION_HORIZONTAL);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return GTK_ORIENTATION_HORIZONTAL;

  return gtk_tool_shell_get_text_orientation (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_get_text_size_group:
 * @tool_item: a #GtkToolItem
 *
 * Returns the size group used for labels in @tool_item.
 * Custom subclasses of #GtkToolItem should call this function
 * and use the size group for labels.
 *
 * Returns: (transfer none): a #GtkSizeGroup
 *
 * Since: 2.20
 */
GtkSizeGroup *
gtk_tool_item_get_text_size_group (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), NULL);

  parent = gtk_widget_get_parent (GTK_WIDGET (tool_item));
  if (!parent || !GTK_IS_TOOL_SHELL (parent))
    return NULL;

  return gtk_tool_shell_get_text_size_group (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_set_expand:
 * @tool_item: a #GtkToolItem
 * @expand: Whether @tool_item is allocated extra space
 *
 * Sets whether @tool_item is allocated extra space when there
 * is more room on the toolbar then needed for the items. The
 * effect is that the item gets bigger when the toolbar gets bigger
 * and smaller when the toolbar gets smaller.
 *
 * Since: 2.4
 */
void
gtk_tool_item_set_expand (GtkToolItem *tool_item,
			  gboolean     expand)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));
    
  expand = expand != FALSE;

  if (tool_item->priv->expand != expand)
    {
      tool_item->priv->expand = expand;
      gtk_widget_child_notify (GTK_WIDGET (tool_item), "expand");
      gtk_widget_queue_resize (GTK_WIDGET (tool_item));
    }
}

/**
 * gtk_tool_item_get_expand:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns whether @tool_item is allocated extra space.
 * See gtk_tool_item_set_expand().
 * 
 * Returns: %TRUE if @tool_item is allocated extra space.
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_expand (GtkToolItem *tool_item)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), FALSE);

  return tool_item->priv->expand;
}

/**
 * gtk_tool_item_set_homogeneous:
 * @tool_item: a #GtkToolItem 
 * @homogeneous: whether @tool_item is the same size as other homogeneous items
 * 
 * Sets whether @tool_item is to be allocated the same size as other
 * homogeneous items. The effect is that all homogeneous items will have
 * the same width as the widest of the items.
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_homogeneous (GtkToolItem *tool_item,
			       gboolean     homogeneous)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));
    
  homogeneous = homogeneous != FALSE;

  if (tool_item->priv->homogeneous != homogeneous)
    {
      tool_item->priv->homogeneous = homogeneous;
      gtk_widget_child_notify (GTK_WIDGET (tool_item), "homogeneous");
      gtk_widget_queue_resize (GTK_WIDGET (tool_item));
    }
}

/**
 * gtk_tool_item_get_homogeneous:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns whether @tool_item is the same size as other homogeneous
 * items. See gtk_tool_item_set_homogeneous().
 * 
 * Returns: %TRUE if the item is the same size as other homogeneous
 * items.
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_homogeneous (GtkToolItem *tool_item)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), FALSE);

  return tool_item->priv->homogeneous;
}

/**
 * gtk_tool_item_get_is_important:
 * @tool_item: a #GtkToolItem
 * 
 * Returns whether @tool_item is considered important. See
 * gtk_tool_item_set_is_important()
 * 
 * Returns: %TRUE if @tool_item is considered important.
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_is_important (GtkToolItem *tool_item)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), FALSE);

  return tool_item->priv->is_important;
}

/**
 * gtk_tool_item_set_is_important:
 * @tool_item: a #GtkToolItem
 * @is_important: whether the tool item should be considered important
 * 
 * Sets whether @tool_item should be considered important. The #GtkToolButton
 * class uses this property to determine whether to show or hide its label
 * when the toolbar style is %GTK_TOOLBAR_BOTH_HORIZ. The result is that
 * only tool buttons with the “is_important” property set have labels, an
 * effect known as “priority text”
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_is_important (GtkToolItem *tool_item, gboolean is_important)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  is_important = is_important != FALSE;

  if (is_important != tool_item->priv->is_important)
    {
      tool_item->priv->is_important = is_important;

      gtk_widget_queue_resize (GTK_WIDGET (tool_item));

      g_object_notify (G_OBJECT (tool_item), "is-important");
    }
}

/**
 * gtk_tool_item_set_tooltip_text:
 * @tool_item: a #GtkToolItem 
 * @text: text to be used as tooltip for @tool_item
 *
 * Sets the text to be displayed as tooltip on the item.
 * See gtk_widget_set_tooltip_text().
 *
 * Since: 2.12
 **/
void
gtk_tool_item_set_tooltip_text (GtkToolItem *tool_item,
			        const gchar *text)
{
  GtkWidget *child;

  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  child = gtk_bin_get_child (GTK_BIN (tool_item));
  if (child)
    gtk_widget_set_tooltip_text (child, text);
}

/**
 * gtk_tool_item_set_tooltip_markup:
 * @tool_item: a #GtkToolItem 
 * @markup: markup text to be used as tooltip for @tool_item
 *
 * Sets the markup text to be displayed as tooltip on the item.
 * See gtk_widget_set_tooltip_markup().
 *
 * Since: 2.12
 **/
void
gtk_tool_item_set_tooltip_markup (GtkToolItem *tool_item,
				  const gchar *markup)
{
  GtkWidget *child;

  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  child = gtk_bin_get_child (GTK_BIN (tool_item));
  if (child)
    gtk_widget_set_tooltip_markup (child, markup);
}

/**
 * gtk_tool_item_set_use_drag_window:
 * @tool_item: a #GtkToolItem 
 * @use_drag_window: Whether @tool_item has a drag window.
 * 
 * Sets whether @tool_item has a drag window. When %TRUE the
 * toolitem can be used as a drag source through gtk_drag_source_set().
 * When @tool_item has a drag window it will intercept all events,
 * even those that would otherwise be sent to a child of @tool_item.
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_use_drag_window (GtkToolItem *toolitem,
				   gboolean     use_drag_window)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (toolitem));

  use_drag_window = use_drag_window != FALSE;

  if (toolitem->priv->use_drag_window != use_drag_window)
    {
      toolitem->priv->use_drag_window = use_drag_window;
      
      if (use_drag_window)
	{
	  if (!toolitem->priv->drag_window &&
              gtk_widget_get_realized (GTK_WIDGET (toolitem)))
	    {
	      create_drag_window(toolitem);
	      if (gtk_widget_get_mapped (GTK_WIDGET (toolitem)))
		gdk_window_show (toolitem->priv->drag_window);
	    }
	}
      else
	{
	  destroy_drag_window (toolitem);
	}
    }
}

/**
 * gtk_tool_item_get_use_drag_window:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns whether @tool_item has a drag window. See
 * gtk_tool_item_set_use_drag_window().
 * 
 * Returns: %TRUE if @tool_item uses a drag window.
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_use_drag_window (GtkToolItem *toolitem)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (toolitem), FALSE);

  return toolitem->priv->use_drag_window;
}

/**
 * gtk_tool_item_set_visible_horizontal:
 * @tool_item: a #GtkToolItem
 * @visible_horizontal: Whether @tool_item is visible when in horizontal mode
 * 
 * Sets whether @tool_item is visible when the toolbar is docked horizontally.
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_visible_horizontal (GtkToolItem *toolitem,
				      gboolean     visible_horizontal)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (toolitem));

  visible_horizontal = visible_horizontal != FALSE;

  if (toolitem->priv->visible_horizontal != visible_horizontal)
    {
      toolitem->priv->visible_horizontal = visible_horizontal;

      g_object_notify (G_OBJECT (toolitem), "visible-horizontal");

      gtk_widget_queue_resize (GTK_WIDGET (toolitem));
    }
}

/**
 * gtk_tool_item_get_visible_horizontal:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns whether the @tool_item is visible on toolbars that are
 * docked horizontally.
 * 
 * Returns: %TRUE if @tool_item is visible on toolbars that are
 * docked horizontally.
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_visible_horizontal (GtkToolItem *toolitem)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (toolitem), FALSE);

  return toolitem->priv->visible_horizontal;
}

/**
 * gtk_tool_item_set_visible_vertical:
 * @tool_item: a #GtkToolItem 
 * @visible_vertical: whether @tool_item is visible when the toolbar
 * is in vertical mode
 *
 * Sets whether @tool_item is visible when the toolbar is docked
 * vertically. Some tool items, such as text entries, are too wide to be
 * useful on a vertically docked toolbar. If @visible_vertical is %FALSE
 * @tool_item will not appear on toolbars that are docked vertically.
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_visible_vertical (GtkToolItem *toolitem,
				    gboolean     visible_vertical)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (toolitem));

  visible_vertical = visible_vertical != FALSE;

  if (toolitem->priv->visible_vertical != visible_vertical)
    {
      toolitem->priv->visible_vertical = visible_vertical;

      g_object_notify (G_OBJECT (toolitem), "visible-vertical");

      gtk_widget_queue_resize (GTK_WIDGET (toolitem));
    }
}

/**
 * gtk_tool_item_get_visible_vertical:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns whether @tool_item is visible when the toolbar is docked vertically.
 * See gtk_tool_item_set_visible_vertical().
 * 
 * Returns: Whether @tool_item is visible when the toolbar is docked vertically
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_visible_vertical (GtkToolItem *toolitem)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (toolitem), FALSE);

  return toolitem->priv->visible_vertical;
}

/**
 * gtk_tool_item_retrieve_proxy_menu_item:
 * @tool_item: a #GtkToolItem 
 * 
 * Returns the #GtkMenuItem that was last set by
 * gtk_tool_item_set_proxy_menu_item(), ie. the #GtkMenuItem
 * that is going to appear in the overflow menu.
 *
 * Returns: (transfer none): The #GtkMenuItem that is going to appear in the
 * overflow menu for @tool_item.
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_tool_item_retrieve_proxy_menu_item (GtkToolItem *tool_item)
{
  gboolean retval;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), NULL);

  g_signal_emit (tool_item, toolitem_signals[CREATE_MENU_PROXY], 0,
		 &retval);
  
  return tool_item->priv->menu_item;
}

/**
 * gtk_tool_item_get_proxy_menu_item:
 * @tool_item: a #GtkToolItem
 * @menu_item_id: a string used to identify the menu item
 *
 * If @menu_item_id matches the string passed to
 * gtk_tool_item_set_proxy_menu_item() return the corresponding #GtkMenuItem.
 *
 * Custom subclasses of #GtkToolItem should use this function to
 * update their menu item when the #GtkToolItem changes. That the
 * @menu_item_ids must match ensures that a #GtkToolItem
 * will not inadvertently change a menu item that they did not create.
 *
 * Returns: (transfer none) (nullable): The #GtkMenuItem passed to
 *     gtk_tool_item_set_proxy_menu_item(), if the @menu_item_ids
 *     match.
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_tool_item_get_proxy_menu_item (GtkToolItem *tool_item,
				   const gchar *menu_item_id)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), NULL);
  g_return_val_if_fail (menu_item_id != NULL, NULL);

  if (tool_item->priv->menu_item_id && strcmp (tool_item->priv->menu_item_id, menu_item_id) == 0)
    return tool_item->priv->menu_item;

  return NULL;
}

/**
 * gtk_tool_item_rebuild_menu:
 * @tool_item: a #GtkToolItem
 *
 * Calling this function signals to the toolbar that the
 * overflow menu item for @tool_item has changed. If the
 * overflow menu is visible when this function it called,
 * the menu will be rebuilt.
 *
 * The function must be called when the tool item changes what it
 * will do in response to the #GtkToolItem::create-menu-proxy signal.
 *
 * Since: 2.6
 */
void
gtk_tool_item_rebuild_menu (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  widget = GTK_WIDGET (tool_item);

  parent = gtk_widget_get_parent (widget);
  if (GTK_IS_TOOL_SHELL (parent))
    gtk_tool_shell_rebuild_menu (GTK_TOOL_SHELL (parent));
}

/**
 * gtk_tool_item_set_proxy_menu_item:
 * @tool_item: (nullable): a #GtkToolItem
 * @menu_item_id: a string used to identify @menu_item
 * @menu_item: a #GtkMenuItem to be used in the overflow menu
 * 
 * Sets the #GtkMenuItem used in the toolbar overflow menu. The
 * @menu_item_id is used to identify the caller of this function and
 * should also be used with gtk_tool_item_get_proxy_menu_item().
 *
 * See also #GtkToolItem::create-menu-proxy.
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_proxy_menu_item (GtkToolItem *tool_item,
				   const gchar *menu_item_id,
				   GtkWidget   *menu_item)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));
  g_return_if_fail (menu_item == NULL || GTK_IS_MENU_ITEM (menu_item));
  g_return_if_fail (menu_item_id != NULL);

  g_free (tool_item->priv->menu_item_id);
      
  tool_item->priv->menu_item_id = g_strdup (menu_item_id);

  if (tool_item->priv->menu_item != menu_item)
    {
      if (tool_item->priv->menu_item)
	g_object_unref (tool_item->priv->menu_item);
      
      if (menu_item)
	{
	  g_object_ref_sink (menu_item);

	  gtk_widget_set_sensitive (menu_item,
				    gtk_widget_get_sensitive (GTK_WIDGET (tool_item)));
	}
      
      tool_item->priv->menu_item = menu_item;
    }
}

/**
 * gtk_tool_item_toolbar_reconfigured:
 * @tool_item: a #GtkToolItem
 *
 * Emits the signal #GtkToolItem::toolbar_reconfigured on @tool_item.
 * #GtkToolbar and other #GtkToolShell implementations use this function
 * to notify children, when some aspect of their configuration changes.
 *
 * Since: 2.14
 **/
void
gtk_tool_item_toolbar_reconfigured (GtkToolItem *tool_item)
{
  /* The slightely inaccurate name "gtk_tool_item_toolbar_reconfigured" was
   * choosen over "gtk_tool_item_tool_shell_reconfigured", since the function
   * emits the "toolbar-reconfigured" signal, not "tool-shell-reconfigured".
   * It's not possible to rename the signal, and emitting another name than
   * indicated by the function name would be quite confusing. That's the
   * price of providing stable APIs.
   */
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  g_signal_emit (tool_item, toolitem_signals[TOOLBAR_RECONFIGURED], 0);
  
  if (tool_item->priv->drag_window)
    gdk_window_raise (tool_item->priv->drag_window);

  gtk_widget_queue_resize (GTK_WIDGET (tool_item));
}
