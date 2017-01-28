/*
 * This file is part of graphene-desktop, the desktop environment of VeltOS.
 * Copyright (C) 2016 Velt Technologies, Aidan Shafran <zelbrium@gmail.com>
 * Licensed under the Apache License 2 <www.apache.org/licenses/LICENSE-2.0>.
 */
 
#include "dialog.h"
#include "cmk/shadow.h"
#include "cmk/cmk-icon.h"
#include <glib.h>
#include <math.h>

#define ICON_SIZE 48

typedef struct _GrapheneDialogPrivate GrapheneDialogPrivate;
struct _GrapheneDialogPrivate
{
	ClutterText *message;
	ClutterActor *content;
	ClutterActor *icon;
	ClutterActor *buttonBox;
	GList *buttons; // List of CmkButton actors
	CmkButton *highlighted; // This button should also appear in the 'buttons' list
	gboolean allowEsc;
};

enum
{
	PROP_CONTENT = 1,
	PROP_ALLOW_ESC,
	PROP_LAST
};

enum
{
	SIGNAL_SELECT = 1,
	SIGNAL_LAST
};

static GParamSpec *properties[PROP_LAST];
static guint signals[SIGNAL_LAST];

static void graphene_dialog_get_preferred_width(ClutterActor *self_, gfloat forHeight, gfloat *minWidth, gfloat *natWidth);
static void graphene_dialog_get_preferred_height(ClutterActor *self_, gfloat forWidth, gfloat *minHeight, gfloat *natHeight);
static void graphene_dialog_allocate(ClutterActor *self_, const ClutterActorBox *box, ClutterAllocationFlags flags);
static void graphene_dialog_set_property(GObject *self_, guint propertyId, const GValue *value, GParamSpec *pspec);
static void graphene_dialog_get_property(GObject *self_, guint propertyId, GValue *value, GParamSpec *pspec);
static void on_style_changed(CmkWidget *self_);
static void on_size_changed(ClutterActor *self, GParamSpec *spec, ClutterCanvas *canvas);
static gboolean on_draw_canvas(ClutterCanvas *canvas, cairo_t *cr, int width, int height, GrapheneDialog *self);

G_DEFINE_TYPE_WITH_PRIVATE(GrapheneDialog, graphene_dialog, CMK_TYPE_WIDGET);
#define PRIVATE(dialog) ((GrapheneDialogPrivate *)graphene_dialog_get_instance_private(dialog))


GrapheneDialog * graphene_dialog_new()
{
	return GRAPHENE_DIALOG(g_object_new(GRAPHENE_TYPE_DIALOG, NULL));
}

GrapheneDialog * graphene_dialog_new_simple(const gchar *message, const gchar *icon, ...)
{
	GrapheneDialog *dialog = graphene_dialog_new();
	graphene_dialog_set_message(dialog, message);
	graphene_dialog_set_icon(dialog, icon);
	
	va_list args;
	va_start(args, icon);
	guint numButtons = 0;
	while(va_arg(args, const gchar *) != NULL)
		numButtons ++;
	va_end(args);
	
	const gchar **buttons = g_new(const gchar *, numButtons+1);
	buttons[numButtons] = NULL;
	
	va_start(args, icon);
	for(guint i=0;i<numButtons;++i)
	{
		const gchar *name = va_arg(args, const gchar *);
		buttons[i] = name;
	}
	va_end(args);

	graphene_dialog_set_buttons(dialog, buttons);
	g_free(buttons);
	return dialog;
}

static void graphene_dialog_class_init(GrapheneDialogClass *class)
{
	CLUTTER_ACTOR_CLASS(class)->get_preferred_width = graphene_dialog_get_preferred_width;
	CLUTTER_ACTOR_CLASS(class)->get_preferred_height = graphene_dialog_get_preferred_height;
	CLUTTER_ACTOR_CLASS(class)->allocate = graphene_dialog_allocate;
	
	CMK_WIDGET_CLASS(class)->style_changed = on_style_changed;

	signals[SIGNAL_SELECT] = g_signal_new("select", G_TYPE_FROM_CLASS(class), G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET(GrapheneDialogClass, select), NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void graphene_dialog_init(GrapheneDialog *self)
{	
	clutter_actor_set_reactive(CLUTTER_ACTOR(self), TRUE);

	ClutterContent *canvas = clutter_canvas_new();
	g_signal_connect(canvas, "draw", G_CALLBACK(on_draw_canvas), self);
	clutter_actor_set_content(CLUTTER_ACTOR(self), canvas);

	GrapheneDialogPrivate *private = PRIVATE(self);
	private->buttonBox = clutter_actor_new();
	ClutterLayoutManager *buttonLayout = clutter_box_layout_new();
	clutter_box_layout_set_orientation(CLUTTER_BOX_LAYOUT(buttonLayout), CLUTTER_ORIENTATION_HORIZONTAL);
	clutter_actor_set_layout_manager(private->buttonBox, buttonLayout);
	clutter_actor_set_x_expand(private->buttonBox, TRUE);
	clutter_actor_set_x_align(private->buttonBox, CLUTTER_ACTOR_ALIGN_END);
	clutter_actor_add_child(CLUTTER_ACTOR(self), private->buttonBox);

	cmk_widget_set_background_color_name(CMK_WIDGET(self), "background");

	g_signal_connect(CLUTTER_ACTOR(self), "notify::size", G_CALLBACK(on_size_changed), canvas);
}

static void graphene_dialog_get_preferred_width(ClutterActor *self_, gfloat forHeight, gfloat *minWidth, gfloat *natWidth)
{
	GrapheneDialog *self = GRAPHENE_DIALOG(self_);
	GrapheneDialogPrivate *private = PRIVATE(self);
	float padding = cmk_widget_style_get_padding(CMK_WIDGET(self_));
	float scale = cmk_widget_style_get_scale_factor(CMK_WIDGET(self_));

	gfloat width = 0;
	width += padding * 2; // edges

	if(private->icon)
		width += ICON_SIZE*scale + padding; // Icon gets padding/2 extra padding

	gfloat messageWidthNat = 0;
	gfloat contentWidthNat = 0;
	gfloat min; // unused

	if(private->message)
	{
		clutter_actor_get_preferred_width(CLUTTER_ACTOR(private->message), forHeight, &min, &messageWidthNat);
		messageWidthNat += padding*2; // give message extra padding
	}

	if(private->content)
	{
		clutter_actor_get_preferred_width(private->content, forHeight, &min, &contentWidthNat);
		contentWidthNat += padding*2; // extra padding
	}

	// Content and Message are vertically aligned, so the width is
	// whichever is bigger
	width += MAX(messageWidthNat, contentWidthNat);
	
	// Make sure it doesn't get too big
	width = CLAMP(width, 100*scale, 450*scale);
	
	// Make sure all the buttons have room
	gfloat bbWidthNat = 0;
	clutter_actor_get_preferred_width(private->buttonBox, -1, &min, &bbWidthNat);
	if(bbWidthNat + padding*2 > width)
		width = bbWidthNat + padding*2;

	*natWidth = width;
	*minWidth = width;
}

static void graphene_dialog_get_preferred_height(ClutterActor *self_, gfloat forWidth, gfloat *minHeight, gfloat *natHeight)
{
	GrapheneDialog *self = GRAPHENE_DIALOG(self_);
	GrapheneDialogPrivate *private = PRIVATE(self);
	float padding = cmk_widget_style_get_padding(CMK_WIDGET(self_));
	float scale = cmk_widget_style_get_scale_factor(CMK_WIDGET(self_));

	gfloat height = 0;
	height += padding * 2; // edges

	gfloat messageHeightNat = 0;
	gfloat contentHeightNat = 0;
	gfloat iconHeight = 0;
	gfloat min; // unused

	if(private->message)
		clutter_actor_get_preferred_height(CLUTTER_ACTOR(private->message), forWidth, &min, &messageHeightNat);
	if(private->content)
		clutter_actor_get_preferred_height(private->content, forWidth, &min, &contentHeightNat);
	if(private->icon)
		iconHeight = ICON_SIZE*scale + padding*2;

	gfloat bodyHeight = messageHeightNat + contentHeightNat;
	if(bodyHeight > 0)
		bodyHeight += padding*3; // extra top padding + double bottom padding
	if(bodyHeight > 0 && private->message && private->content)
		bodyHeight += padding*2; // double separation padding

	// Whichever is taller: icon or body (message + padding + content)
	height += MAX(iconHeight, bodyHeight);
	
	// Height for buttons
	gfloat bbHeightNat = 0;
	clutter_actor_get_preferred_height(private->buttonBox, -1, &min, &bbHeightNat);
	height += bbHeightNat;

	*natHeight = height;
	*minHeight = height;
}

static gboolean _clutter_actor_box_valid(const ClutterActorBox *box)
{
	return (box->x2 - box->x1) > 0 && (box->y2 - box->y1) > 0;
}

static void graphene_dialog_allocate(ClutterActor *self_, const ClutterActorBox *box, ClutterAllocationFlags flags)
{
	/*
	 * ------------------------------------  <- 
	 * |                                  |   |
	 * |  [Icon]  [                    ]  |   |
	 * |  [    ]  [      Message       ]  |   |
	 * |          [                    ]  |   | min/nat height
	 * |                                  |   |
	 * |          [      Content       ]  |   |
	 * |                                  |   |
	 * |      [Button] [Button] [Button]  |   |
	 * |                                  |   |
	 * ------------------------------------  <-
	 * ^---------min/nat width------------^
	 * Any item can be missing, causing allocations to adjust.
	 * For example, if the icon is missing, the message and content
	 * will fill the entire width. The dialog's size is always
	 * at least as great as the button box's size + padding.
	 */

	GrapheneDialog *self = GRAPHENE_DIALOG(self_);
	GrapheneDialogPrivate *private = PRIVATE(self);

	float padding = cmk_widget_style_get_padding(CMK_WIDGET(self_));
	float scale = cmk_widget_style_get_scale_factor(CMK_WIDGET(self_));
	
	// The dialog always has a padding
	ClutterActorBox padBox = {padding, padding, (box->x2-box->x1)-padding, (box->y2-box->y1)-padding};
	if(!_clutter_actor_box_valid(&padBox)) // Make sure the box isn't inverted
		goto allocate_exit;

	ClutterActorBox bodyBox = padBox;

	if(private->icon)
	{
		// Give icon a margin of padding/2
		ClutterActorBox iconBox = {padBox.x1+padding/2,padBox.y1+padding/2,padBox.x1+ICON_SIZE*scale+padding/2,padBox.y1+ICON_SIZE*scale+padding/2};
		bodyBox.x1 = iconBox.x2 + padding/2; // Shrink the body
		clutter_actor_allocate(private->icon, &iconBox, flags);
	}
	
	// Allocate buttons
	gfloat bbHeightNat = 0;
	gfloat min;
	clutter_actor_get_preferred_height(private->buttonBox, -1, &bbHeightNat, &min);

	ClutterActorBox buttonBox = {padBox.x1, padBox.y2-bbHeightNat, padBox.x2, padBox.y2};
	bodyBox.y2 = buttonBox.y1 - padding; // Shrink body
	clutter_actor_allocate(private->buttonBox, &buttonBox, flags);

	// Place message
	gfloat messageHeightNat = 0;
	clutter_actor_get_preferred_height(CLUTTER_ACTOR(private->message), (bodyBox.x2-bodyBox.x1-padding-padding), &messageHeightNat, &min);
	ClutterActorBox messageBox = {bodyBox.x1+padding,
		bodyBox.y1+padding,
		bodyBox.x2-padding,
		MIN(bodyBox.y2-padding, bodyBox.y1+padding+messageHeightNat)
	};
	if(!_clutter_actor_box_valid(&messageBox))
		goto allocate_exit;
	clutter_actor_allocate(CLUTTER_ACTOR(private->message), &messageBox, flags);
	
	bodyBox.y1 = messageBox.y2;
	
	ClutterActorBox contentBox = {bodyBox.x1+padding,
		bodyBox.y1+padding,
		bodyBox.x2-padding,
		bodyBox.y2-padding
	};
	if(!_clutter_actor_box_valid(&contentBox))
		goto allocate_exit;
	clutter_actor_allocate(CLUTTER_ACTOR(private->content), &contentBox, flags);

allocate_exit:
	CLUTTER_ACTOR_CLASS(graphene_dialog_parent_class)->allocate(self_, box, flags);
}

static void on_style_changed(CmkWidget *self_)
{
	clutter_content_invalidate(clutter_actor_get_content(CLUTTER_ACTOR(self_)));
	clutter_actor_queue_relayout(CLUTTER_ACTOR(self_));
	
	GrapheneDialogPrivate *private = PRIVATE(GRAPHENE_DIALOG(self_));
	if(private->message)
	{
		const ClutterColor *color = cmk_widget_get_foreground_color(self_);
		clutter_text_set_color(private->message, color);
	}

	CMK_WIDGET_CLASS(graphene_dialog_parent_class)->style_changed(self_);
}

static void on_size_changed(ClutterActor *self, GParamSpec *spec, ClutterCanvas *canvas)
{
	gfloat width, height;
	clutter_actor_get_size(self, &width, &height);
	clutter_canvas_set_size(CLUTTER_CANVAS(canvas), width, height);
}

static gboolean on_draw_canvas(ClutterCanvas *canvas, cairo_t *cr, int width, int height, GrapheneDialog *self)
{
	double radius = cmk_widget_style_get_bevel_radius(CMK_WIDGET(self));
	double degrees = M_PI / 180.0;

	cairo_save(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_restore(cr);

	cairo_new_sub_path(cr);
	cairo_arc(cr, width - radius, radius, radius, -90 * degrees, 0 * degrees);
	cairo_arc(cr, width - radius, height - radius, radius, 0 * degrees, 90 * degrees);
	cairo_arc(cr, radius, height - radius, radius, 90 * degrees, 180 * degrees);
	cairo_arc(cr, radius, radius, radius, 180 * degrees, 270 * degrees);
	cairo_close_path(cr);

	cairo_set_source_clutter_color(cr, cmk_widget_get_background_color(CMK_WIDGET(self)));
	cairo_fill(cr);
	return TRUE;
}

static void on_button_activate(GrapheneDialog *self, CmkButton *button)
{
	g_signal_emit(self, signals[SIGNAL_SELECT], 0, cmk_button_get_name(button));
}

void graphene_dialog_set_message(GrapheneDialog *self, const gchar *message)
{
	g_return_if_fail(GRAPHENE_IS_DIALOG(self));
	GrapheneDialogPrivate *private = PRIVATE(self);

	if(!message)
	{
		clutter_actor_destroy(CLUTTER_ACTOR(private->message));
		private->message = NULL;
		return;
	}
	
	if(!private->message)
	{
		private->message = CLUTTER_TEXT(clutter_text_new());	
		const ClutterColor *color = cmk_widget_get_foreground_color(CMK_WIDGET(self));
		clutter_text_set_color(private->message, color);
		clutter_text_set_line_wrap(private->message, TRUE);
		clutter_text_set_text(private->message, message);
		clutter_actor_set_x_align(CLUTTER_ACTOR(private->message), CLUTTER_ACTOR_ALIGN_START);
		clutter_actor_add_child(CLUTTER_ACTOR(self), CLUTTER_ACTOR(private->message));
	}
	else
	{
		clutter_text_set_text(private->message, message);
	}
}

void graphene_dialog_set_content(GrapheneDialog *self, ClutterActor *content)
{
	g_return_if_fail(GRAPHENE_IS_DIALOG(self));
	GrapheneDialogPrivate *private = PRIVATE(self);
	if(private->content == content)
		return;

	if(private->content)
	{
		clutter_actor_remove_child(CLUTTER_ACTOR(self), private->content);
		private->content = NULL;
	}
	
	if(content)
	{
		private->content = content;
		clutter_actor_add_child(CLUTTER_ACTOR(self), content);
	}
}

void graphene_dialog_set_buttons(GrapheneDialog *self, const gchar * const *buttons)
{
	g_return_if_fail(GRAPHENE_IS_DIALOG(self));
	GrapheneDialogPrivate *private = PRIVATE(self);
	clutter_actor_destroy_all_children(private->buttonBox);

	const gchar *name = NULL;
	guint i=0;
	while((name = buttons[i++]) != NULL)
	{
		CmkButton *button = cmk_button_new_full(name, CMK_BUTTON_TYPE_BEVELED);
		cmk_widget_set_style_parent(CMK_WIDGET(button), CMK_WIDGET(self));
		g_signal_connect_swapped(button, "activate", G_CALLBACK(on_button_activate), self);
		clutter_actor_add_child(private->buttonBox, CLUTTER_ACTOR(button));
	}
}

void graphene_dialog_set_icon(GrapheneDialog *self, const gchar *iconName)
{
	g_return_if_fail(GRAPHENE_IS_DIALOG(self));
	GrapheneDialogPrivate *private = PRIVATE(self);
	
	if(private->icon)
	{
		clutter_actor_destroy(private->icon);
		private->icon = NULL;
	}

	if(iconName)
	{
		private->icon = CLUTTER_ACTOR(cmk_icon_new_from_name(iconName));
		clutter_actor_add_child(CLUTTER_ACTOR(self), private->icon);
	}
}
