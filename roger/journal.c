/*
 * Roger Router
 * Copyright (c) 2012-2018 Jan-Michael Brummer
 *
 * This file is part of Roger Router.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <rm/rm.h>

#include "roger/main.h"
#include "roger/phone.h"
#include "roger/journal.h"
#include "roger/print.h"
#include "roger/contacts.h"
#include "roger/application.h"
#include "roger/uitools.h"
#include "roger/answeringmachine.h"

struct _RogerJournal {
	GtkApplicationWindow parent_instance;

  GtkWidget *headerbar;
  GtkWidget *filter_combobox;
  GtkWidget *view;
  GtkWidget *spinner;
  GtkListStore *list_store;
  RmFilter *filter;
  RmFilter *search_filter;
  GSList *list;
  GtkWidget *search_bar;
  GtkWidget *search_button;

  GtkWidget *col0;
  GtkWidget *col1;
  GtkWidget *col2;
  GtkWidget *col3;
  GtkWidget *col4;
  GtkWidget *col5;
  GtkWidget *col6;
  GtkWidget *col7;
  GtkWidget *col8;
  GtkCellRenderer *col2_renderer;

  GMutex mutex;

  gboolean hide_on_quit;
  gboolean hide_on_start;
};

G_DEFINE_TYPE(RogerJournal, roger_journal, GTK_TYPE_APPLICATION_WINDOW)

static GdkPixbuf *icon_call_in = NULL;
static GdkPixbuf *icon_call_missed = NULL;
static GdkPixbuf *icon_call_out = NULL;
static GdkPixbuf *icon_fax = NULL;
static GdkPixbuf *icon_fax_report = NULL;
static GdkPixbuf *icon_voice = NULL;
static GdkPixbuf *icon_record = NULL;
static GdkPixbuf *icon_blocked = NULL;

void
journal_clear (RogerJournal *journal)
{
	gtk_list_store_clear (journal->list_store);
}

static void
init_call_icons (void)
{
	gint width = 18;

	icon_call_in = gdk_pixbuf_new_from_resource_at_scale("/org/tabos/roger/images/roger-call-in.svg", width, width, TRUE, NULL);
	icon_call_missed = gdk_pixbuf_new_from_resource_at_scale("/org/tabos/roger/images/roger-call-missed.svg", width, width, TRUE, NULL);
	icon_call_out = gdk_pixbuf_new_from_resource_at_scale("/org/tabos/roger/images/roger-call-out.svg", width, width, TRUE, NULL);
	icon_fax = gdk_pixbuf_new_from_resource_at_scale("/org/tabos/roger/images/roger-fax.svg", width, width, TRUE, NULL);
	icon_fax_report = gdk_pixbuf_new_from_resource_at_scale("/org/tabos/roger/images/roger-fax-report.svg", width, width, TRUE, NULL);
	icon_voice = gdk_pixbuf_new_from_resource_at_scale("/org/tabos/roger/images/roger-call-voice.svg", width, width, TRUE, NULL);
	icon_record = gdk_pixbuf_new_from_resource_at_scale("/org/tabos/roger/images/roger-record.svg", width, width, TRUE, NULL);
	icon_blocked = gdk_pixbuf_new_from_resource_at_scale("/org/tabos/roger/images/roger-call-blocked.svg", width, width, TRUE, NULL);
}

static GdkPixbuf *
get_call_icon (gint type)
{
	switch (type) {
	case RM_CALL_ENTRY_TYPE_INCOMING:
		return icon_call_in;
	case RM_CALL_ENTRY_TYPE_MISSED:
		return icon_call_missed;
	case RM_CALL_ENTRY_TYPE_OUTGOING:
		return icon_call_out;
	case RM_CALL_ENTRY_TYPE_FAX:
		return icon_fax;
	case RM_CALL_ENTRY_TYPE_FAX_REPORT:
		return icon_fax_report;
	case RM_CALL_ENTRY_TYPE_VOICE:
		return icon_voice;
	case RM_CALL_ENTRY_TYPE_RECORD:
		return icon_record;
	case RM_CALL_ENTRY_TYPE_BLOCKED:
		return icon_blocked;
	default:
		g_debug ("%s(): Unknown icon type: %d", __FUNCTION__, type);
		break;
	}

	return NULL;
}

void
journal_redraw (RogerJournal *self)
{
	GSList *list;
	gint duration = 0;
	gchar *text = NULL;
	gint count = 0;
	RmProfile *profile;

	/* Update liststore */
	for (list = self->list; list != NULL; list = list->next) {
		GtkTreeIter iter;
		RmCallEntry *call = list->data;

		g_assert(call != NULL);

		if (rm_filter_rule_match(self->filter, call) == FALSE) {
			continue;
		}

		if (rm_filter_rule_match(self->search_filter, call) == FALSE) {
			continue;
		}

		gtk_list_store_insert_with_values(self->list_store, &iter, -1,
						  JOURNAL_COL_TYPE, get_call_icon(call->type),
						  JOURNAL_COL_DATETIME, call->date_time,
						  JOURNAL_COL_NAME, call->remote->name,
						  JOURNAL_COL_COMPANY, call->remote->company,
						  JOURNAL_COL_NUMBER, call->remote->number,
						  JOURNAL_COL_CITY, call->remote->city,
						  JOURNAL_COL_EXTENSION, call->local->name,
						  JOURNAL_COL_LINE, call->local->number,
						  JOURNAL_COL_DURATION, call->duration,
						  JOURNAL_COL_CALL_PTR, call,
						  -1);

		if (call->duration && strchr(call->duration, 's') != NULL) {
			/* Ignore voicebox duration */
		} else {
			if (call->duration != NULL && strlen(call->duration) > 0) {
				duration += (call->duration[0] - '0') * 60;
				duration += (call->duration[2] - '0') * 10;
				duration += call->duration[3] - '0';
			}
		}
		count++;
	}

	profile = rm_profile_get_active();

  gtk_header_bar_set_title (GTK_HEADER_BAR (self->headerbar), profile ? profile->name : _("<No profile>"));
	g_autofree gchar *markup = g_strdup_printf(_("%d calls, %d:%2.2dh"), count, duration / 60, duration % 60);
  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->headerbar), markup);

	g_free(text);
}

static gboolean
update_journal (gpointer user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
	GtkTreeIter iter;
	gboolean valid;
	RmCallEntry *call;

	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->list_store), &iter);
	while (valid) {
		gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &iter, JOURNAL_COL_CALL_PTR, &call, -1);

		if (call->remote->lookup) {
			gtk_list_store_set (self->list_store, &iter, JOURNAL_COL_NAME, call->remote->name, -1);
			gtk_list_store_set (self->list_store, &iter, JOURNAL_COL_CITY, call->remote->city, -1);
		}

		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->list_store), &iter);
	}

	gtk_spinner_stop (GTK_SPINNER (self->spinner));
	gtk_widget_hide (self->spinner);

	g_mutex_unlock (&self->mutex);

	return FALSE;
}

static gpointer
lookup_journal (gpointer user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
	GSList *list;

	for (list = self->list; list; list = list->next) {
		RmCallEntry *call = list->data;

		if (!RM_EMPTY_STRING (call->remote->name)) {
			continue;
		}

		if (rm_lookup_search (call->remote->number, call->remote)) {
			call->remote->lookup = TRUE;
		}
	}

	g_idle_add (update_journal, self);

	return NULL;
}

void journal_filter_box_changed(GtkComboBox *box, gpointer user_data);

static void
on_journal_loaded(RmObject *obj,
                  GSList   *list,
                  gpointer  user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
	GSList *old;

	if (g_mutex_trylock (&self->mutex) == FALSE) {
		g_debug ("%s(): Journal loading already in progress", __FUNCTION__);
		return;
	}

	journal_filter_box_changed( GTK_COMBO_BOX (self->filter_combobox), self);

	if (!gtk_widget_get_visible (self->spinner)) {
		gtk_spinner_start (GTK_SPINNER (self->spinner));
		gtk_widget_show (self->spinner);
	}

	/* Clear existing liststore */
	journal_clear (self);

	/* Set new internal list */
	old = self->list;
	self->list = list;

	if (old) {
		g_slist_free_full (old, rm_call_entry_free);
	}

	journal_redraw (self);

	g_thread_new ("Reverse Lookup Journal", lookup_journal, self);
}

static void
on_connection_changed (RmObject     *obj,
                       gint          type,
                       RmConnection *connection,
                       gpointer      user_data)
{
	if (type == RM_CONNECTION_TYPE_DISCONNECT) {
		rm_router_load_journal (rm_profile_get_active ());
	}
}

static gboolean
reload_journal_idle (gpointer user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
	RmProfile *profile = rm_profile_get_active();

	if (profile == NULL) {
		return FALSE;
	}

	if (rm_router_load_journal (profile) == FALSE) {
		gtk_spinner_stop (GTK_SPINNER (self->spinner));
		gtk_widget_hide (self->spinner);
	}

	return FALSE;
}

static void
reload_journal (RogerJournal *self)
{
	gtk_spinner_start (GTK_SPINNER (self->spinner));
	gtk_widget_show (self->spinner);

	g_idle_add (reload_journal_idle, NULL);
}

static void
clear_journal (RogerJournal *self)
{
	GtkWidget *dialog;
	gint flags = GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR;

	dialog = gtk_message_dialog_new (GTK_WINDOW (self), flags, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, _("Do you want to delete the router journal?"));

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Cancel"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Delete"), GTK_RESPONSE_OK);

	gint result = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (result != GTK_RESPONSE_OK) {
		return;
	}

	rm_router_clear_journal (rm_profile_get_active ());
}

void delete_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
	RmCallEntry *call = NULL;
	GValue ptr = { 0 };

	gtk_tree_model_get_value(model, iter, JOURNAL_COL_CALL_PTR, &ptr);

	call = g_value_get_pointer(&ptr);

	switch (call->type) {
	case RM_CALL_ENTRY_TYPE_RECORD:
		g_unlink(call->priv);
		break;
	case RM_CALL_ENTRY_TYPE_VOICE:
		rm_router_delete_voice(rm_profile_get_active(), call->priv);
		break;
	case RM_CALL_ENTRY_TYPE_FAX:
		rm_router_delete_fax(rm_profile_get_active(), call->priv);
		break;
	case RM_CALL_ENTRY_TYPE_FAX_REPORT:
		g_unlink(call->priv);
		break;
	default:
		self->list = g_slist_remove(self->list, call);
		g_debug("Deleting: '%s'", call->date_time);
		rm_journal_save(self->list);
		break;
	}
}

void journal_button_delete_clicked_cb(GtkWidget *button, gpointer user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
	GtkWidget *dialog;
	gint flags = GTK_DIALOG_MODAL |GTK_DIALOG_USE_HEADER_BAR;

	dialog = gtk_message_dialog_new(GTK_WINDOW(self), flags, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, _("Do you really want to delete the selected entry?"));

	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Cancel"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Delete"), GTK_RESPONSE_OK);

	gint result = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if (result != GTK_RESPONSE_OK) {
		return;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self->view));

	gtk_tree_selection_selected_foreach(selection, delete_foreach, NULL);

	rm_router_load_journal(rm_profile_get_active());
}

void journal_add_contact(RmCallEntry *call)
{
	app_contacts(call->remote);
}

void add_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	RmCallEntry *call = NULL;
	GValue ptr = { 0 };

	gtk_tree_model_get_value(model, iter, JOURNAL_COL_CALL_PTR, &ptr);

	call = g_value_get_pointer(&ptr);

	journal_add_contact(call);
}

void journal_button_add_clicked_cb(GtkWidget *button, GtkWidget *view)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

	gtk_tree_selection_selected_foreach(selection, add_foreach, NULL);
}

static void
on_search_entry_changed (GtkEditable *entry,
                         gpointer user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
  RmProfile *profile = rm_profile_get_active ();
	const gchar *text = gtk_entry_get_text (GTK_ENTRY (entry));

	if (self->search_filter != NULL) {
		rm_filter_remove (profile, self->search_filter);
		self->search_filter = NULL;
	}

	if (strlen(text) > 0) {
		self->search_filter = rm_filter_new(profile, "internal_search");

		if (g_ascii_isdigit (text[0])) {
			rm_filter_rule_add (self->search_filter, RM_FILTER_REMOTE_NUMBER, RM_FILTER_CONTAINS, (gchar*)text);
		} else {
			rm_filter_rule_add (self->search_filter, RM_FILTER_REMOTE_NAME, RM_FILTER_CONTAINS, (gchar*)text);
		}
	}

	journal_clear (self);
	journal_redraw (self);
}

void
journal_filter_box_changed(GtkComboBox *box,
                           gpointer     user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
	RmProfile *profile = rm_profile_get_active ();
	GSList *filter_list;
	const gchar *text = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (box));

	self->filter = NULL;
	journal_clear (self);

	if (text == NULL || profile == NULL) {
    return;
  }

	for (filter_list = rm_filter_get_list (profile); filter_list != NULL; filter_list = filter_list->next) {
		RmFilter *filter = filter_list->data;

		if (!strcmp (filter->name, text)) {
			self->filter = filter;
			break;
		}
	}

	journal_redraw (self);
}

static void
journal_update_filter_box (RogerJournal *self)
{
	GSList *filter_list;

	filter_list = rm_filter_get_list (rm_profile_get_active ());

	gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (self->filter_combobox));

	while (filter_list != NULL) {
		RmFilter *filter = filter_list->data;

		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (self->filter_combobox), NULL, filter->name);
		filter_list = filter_list->next;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (self->filter_combobox), 0);
}

int app_pdf(char *data, int length, gchar *filename);

void
row_activated_foreach (GtkTreeModel *model,
                       GtkTreePath  *path,
                       GtkTreeIter  *iter,
                       gpointer      data)
{
	RmCallEntry *call;
	GError *error = NULL;

	gtk_tree_model_get(model, iter, JOURNAL_COL_CALL_PTR, &call, -1);

	switch (call->type) {
	case RM_CALL_ENTRY_TYPE_FAX_REPORT:
		app_pdf(NULL, 0, call->priv);
		break;
	case RM_CALL_ENTRY_TYPE_FAX: {
		gsize len = 0;
		g_autofree gchar *data = NULL;

		data = rm_router_load_fax(rm_profile_get_active(), call->priv, &len);

		if (data && len) {
			g_autofree gchar *path = g_build_filename(rm_get_user_cache_dir(), G_DIR_SEPARATOR_S, "fax.pdf", NULL);
			g_autofree gchar *uri;

			rm_file_save(path, data, len);

			uri = g_strdup_printf("file:///%s", path);

			app_pdf(data, len, NULL);
		}
		break;
	}
	case RM_CALL_ENTRY_TYPE_RECORD: {
    gchar *tmp = call->priv;

    if (!gtk_show_uri_on_window(GTK_WINDOW(journal_get_window()), tmp, GDK_CURRENT_TIME, &error)) {
				g_debug("%s(): Could not open uri '%s'", __FUNCTION__, tmp);
				g_debug("%s(): '%s'", __FUNCTION__, error->message);
    } else {
				g_debug("%s(): Opened '%s'", __FUNCTION__, tmp);
	  }
		break;
  }
	case RM_CALL_ENTRY_TYPE_VOICE:
		app_answeringmachine(call->priv);
		break;
	default:
		app_phone(call->remote, NULL);
		break;
	}
}

static void
on_view_row_activated (GtkTreeView       *view,
                       GtkTreePath       *path,
                       GtkTreeViewColumn *column,
                       gpointer           user_data)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection (view);

	gtk_tree_selection_selected_foreach (selection, row_activated_foreach, NULL);
}

void
journal_set_hide_on_quit(RogerJournal *self,
                         gboolean      hide)
{
	self->hide_on_quit = hide;

  if (!hide) {
		gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
	}
}

void
journal_set_hide_on_start (RogerJournal *self,
                           gboolean      hide)
{
	self->hide_on_start = hide;

  if (hide) {
		gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
	}
}

static gboolean
on_delete_event (GtkWidget *widget,
                 GdkEvent  *event,
                 gpointer   user_data)
{
  RogerJournal *self = ROGER_JOURNAL (widget);

	if (self->hide_on_quit) {
		gtk_widget_hide (widget);

		return TRUE;
	}

	g_signal_handlers_disconnect_by_func (rm_object, on_journal_loaded, NULL);

	return FALSE;
}

static gint
journal_sort_by_date (GtkTreeModel *model,
                      GtkTreeIter  *a,
                      GtkTreeIter  *b,
                      gpointer     data)
{
	RmCallEntry *call_a;
	RmCallEntry *call_b;

	gtk_tree_model_get (model, a, JOURNAL_COL_CALL_PTR, &call_a, -1);
	gtk_tree_model_get (model, b, JOURNAL_COL_CALL_PTR, &call_b, -1);

	return rm_journal_sort_by_date (call_a, call_b);
}

static gint
journal_sort_by_type (GtkTreeModel *model,
                      GtkTreeIter  *a,
                      GtkTreeIter  *b,
                      gpointer      data)
{
	RmCallEntry *call_a;
	RmCallEntry *call_b;

	gtk_tree_model_get (model, a, JOURNAL_COL_CALL_PTR, &call_a, -1);
	gtk_tree_model_get (model, b, JOURNAL_COL_CALL_PTR, &call_b, -1);

	return call_a->type > call_b->type ? -1 : call_a->type < call_b->type ? 1 : 0;
}

static void
name_column_cell_data_func (GtkTreeViewColumn *column,
                            GtkCellRenderer   *renderer,
                            GtkTreeModel      *model,
                            GtkTreeIter       *iter,
                            gpointer           user_data)
{
	RmCallEntry *call;

	gtk_tree_model_get(model, iter, JOURNAL_COL_CALL_PTR, &call, -1);

	if (call != NULL && call->remote->lookup) {
		g_object_set(renderer, "foreground", "darkgrey", "foreground-set", TRUE, NULL);
	} else {
		g_object_set(renderer, "foreground-set", FALSE, NULL);
	}
}

static gboolean journal_column_header_button_pressed_cb(GtkTreeViewColumn *column, GdkEventButton *event, gpointer user_data)
{
	GtkMenu *menu = GTK_MENU(user_data);

	if (event->button == GDK_BUTTON_SECONDARY) {
#if GTK_CHECK_VERSION(3, 21, 0)
		gtk_menu_popup_at_pointer(menu, (GdkEvent*)event);
#else
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
#endif
		return TRUE;
	}

	return FALSE;
}

void journal_popup_copy_number(GtkWidget *widget, RmCallEntry *call)
{
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_NONE), call->remote->number, -1);
}

void journal_popup_call_number(GtkWidget *widget, RmCallEntry *call)
{
	app_phone(call->remote, NULL);
}

void journal_popup_add_contact(GtkWidget *widget, RmCallEntry *call)
{
	journal_add_contact(call);
}

void journal_popup_delete_entry(GtkWidget *widget, RmCallEntry *call)
{
	//journal_button_delete_clicked_cb(NULL, journal_view);
}

void journal_popup_menu(GtkWidget *treeview, GdkEventButton *event, gpointer user_data)
{
	GtkWidget *menu, *menuitem;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeModel *model;
	GList *list;
	GtkTreeIter iter;
	RmCallEntry *call = NULL;

	if (gtk_tree_selection_count_selected_rows(selection) != 1) {
		return;
	}
	list = gtk_tree_selection_get_selected_rows(selection, &model);
	gtk_tree_model_get_iter(model, &iter, (GtkTreePath*)list->data);
	gtk_tree_model_get(model, &iter, JOURNAL_COL_CALL_PTR, &call, -1);

	menu = gtk_menu_new();

	/* Copy phone number */
	menuitem = gtk_menu_item_new_with_label(_("Copy number"));
	g_signal_connect(menuitem, "activate", (GCallback)journal_popup_copy_number, call);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	/* Call phone number */
	menuitem = gtk_menu_item_new_with_label(_("Call number"));
	g_signal_connect(menuitem, "activate", (GCallback)journal_popup_call_number, call);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	/* Separator */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	/* Add contact */
	menuitem = gtk_menu_item_new_with_label(_("Add contact"));
	g_signal_connect(menuitem, "activate", (GCallback)journal_popup_add_contact, call);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	/* Separator */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	/* Delete entry */
	menuitem = gtk_menu_item_new_with_label(_("Delete entry"));
	g_signal_connect(menuitem, "activate", (GCallback)journal_popup_delete_entry, call);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gtk_widget_show_all(menu);

	gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
}

gboolean
on_view_button_press_event (GtkWidget      *treeview,
                            GdkEventButton *event,
                            gpointer        user_data)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY) {
		GtkTreeSelection *selection;
		GtkTreePath *path;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

		if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), (gint)event->x, (gint)event->y, &path, NULL, NULL, NULL)) {
			gtk_tree_selection_unselect_all (selection);
			gtk_tree_selection_select_path (selection, path);
			gtk_tree_path_free (path);
		}

		journal_popup_menu (treeview, event, user_data);

		return TRUE;
	}

	return FALSE;
}

static void
window_cmd_refresh (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);

	reload_journal (self);
}

static void
window_cmd_print (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);

 	print_journal (self->view);
}

static void
window_cmd_clear (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);

	clear_journal (self);
}

static void
window_cmd_export (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);
	g_autoptr (GtkFileChooserNative) native = NULL;
	GtkFileChooser *chooser;
	gint res;

	native = gtk_file_chooser_native_new (_("Export journal"), GTK_WINDOW (self), GTK_FILE_CHOOSER_ACTION_SAVE, _("Save"), _("Cancel"));
	chooser = GTK_FILE_CHOOSER (native);
	gtk_file_chooser_set_current_name (chooser, "journal.csv");

  res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));
	if (res == GTK_RESPONSE_ACCEPT) {
		g_autofree gchar *file = gtk_file_chooser_get_filename (chooser);

		rm_journal_save_as (self->list, file);
	}
}

static void
on_contacts_changed(RmObject *object,
                    gpointer  user_data)
{
  RogerJournal *self = ROGER_JOURNAL (user_data);

	reload_journal (self);
}

GtkWidget *journal_get_window()
{
  return NULL;
}

void journal_set_visible(RogerJournal *self, gboolean state)
{
	gtk_widget_set_visible (GTK_WIDGET (self), state);
}

#if 0
void journal_column_restore_default(GtkMenuItem *item, gpointer user_data)
{
	gint index;

	for (index = JOURNAL_COL_DATETIME; index <= JOURNAL_COL_DURATION; index++) {
		GtkTreeViewColumn *column;
		gchar *key;

		key = g_strdup_printf("col-%d-width", index);
		g_settings_set_uint(app_settings, key, 0);
		g_free(key);

		key = g_strdup_printf("col-%d-visible", index);
		g_settings_set_boolean(app_settings, key, TRUE);
		g_free(key);

		column = gtk_tree_view_get_column(GTK_TREE_VIEW(user_data), index);
		gtk_tree_view_column_set_visible(column, TRUE);
		gtk_tree_view_column_set_fixed_width(column, -1);
	}
}
#endif

static gboolean
on_key_press_event (GtkWidget *widget,
                    GdkEvent  *event,
                    gpointer   user_data)
{
  RogerJournal *self = ROGER_JOURNAL (widget);
	GdkEventKey *key = (GdkEventKey*)event;
	gint ret;

	ret = gtk_search_bar_handle_event(GTK_SEARCH_BAR(self->search_bar), event);

	if (ret != GDK_EVENT_STOP) {
		if (key->keyval == GDK_KEY_Escape) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->search_button), FALSE);
		}
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->search_button), TRUE);
	}

	return ret;
}

static const GActionEntry window_entries [] =
{
	{ "refresh", window_cmd_refresh },
	{ "print", window_cmd_print },
	{ "clear", window_cmd_clear },
	{ "export", window_cmd_export },
	/*
	{ "contacts-edit-phone-home", contacts_add_detail_activated },
	{ "contacts-edit-phone-work", contacts_add_detail_activated },
	{ "contacts-edit-phone-mobile", contacts_add_detail_activated },
	{ "contacts-edit-phone-home-fax", contacts_add_detail_activated },
	{ "contacts-edit-phone-work-fax", contacts_add_detail_activated },
	{ "contacts-edit-phone-pager", contacts_add_detail_activated },
	{ "contacts-edit-address-home", contacts_add_detail_activated },
	{ "contacts-edit-address-work", contacts_add_detail_activated },*/
};

static void
roger_journal_class_init (RogerJournalClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tabos/roger/ui/roger-journal.ui");

 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, headerbar);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, filter_combobox);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, view);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, list_store);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, spinner);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, search_bar);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, search_button);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, col0);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, col1);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, col2);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, col3);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, col4);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, col5);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, col6);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, col7);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, col8);
 	gtk_widget_class_bind_template_child (widget_class, RogerJournal, col2_renderer);

  gtk_widget_class_bind_template_callback (widget_class, on_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_delete_event);
}

static void
roger_journal_init (RogerJournal *self)
{
  GtkTreeSortable *sortable;
  GSimpleActionGroup *simple_action_group;

	gtk_widget_init_template (GTK_WIDGET (self));

  /* Add actions */
  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   window_entries,
                                   G_N_ELEMENTS (window_entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "win",
                                  G_ACTION_GROUP (simple_action_group));


  init_call_icons();

	if (g_settings_get_boolean (app_settings, "maximized")) {
		gtk_window_maximize (GTK_WINDOW (self));
	}

  journal_update_filter_box (self);

	gtk_widget_hide_on_delete (GTK_WIDGET (self));

	journal_filter_box_changed (GTK_COMBO_BOX (self->filter_combobox), self);

  sortable = GTK_TREE_SORTABLE(self->list_store);

  g_settings_bind (app_settings, "col-0-width", self->col0, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-0-visible", self->col0, "visible", G_SETTINGS_BIND_DEFAULT);
  gtk_tree_sortable_set_sort_func(sortable, JOURNAL_COL_TYPE, journal_sort_by_type, 0, NULL);

  g_settings_bind (app_settings, "col-1-width", self->col1, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-1-visible", self->col1, "visible", G_SETTINGS_BIND_DEFAULT);
	gtk_tree_sortable_set_sort_func(sortable, JOURNAL_COL_DATETIME, journal_sort_by_date, 0, NULL);

  g_settings_bind (app_settings, "col-2-width", self->col2, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-2-visible", self->col2, "visible", G_SETTINGS_BIND_DEFAULT);
	gtk_tree_view_column_set_cell_data_func(GTK_TREE_VIEW_COLUMN (self->col2), self->col2_renderer, name_column_cell_data_func, NULL, NULL);

  g_settings_bind (app_settings, "col-3-width", self->col3, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-3-visible", self->col3, "visible", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-4-width", self->col4, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-4-visible", self->col4, "visible", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-5-width", self->col5, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-5-visible", self->col5, "visible", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-6-width", self->col6, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-6-visible", self->col6, "visible", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-7-width", self->col7, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-7-visible", self->col7, "visible", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-8-width", self->col8, "fixed-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "col-8-visible", self->col8, "visible", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (app_settings, "width", self, "default-width", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (app_settings, "height", self, "default-heght", G_SETTINGS_BIND_DEFAULT);

  g_object_bind_property (self->search_button, "active", self->search_bar, "search-mode-enabled", 0);

  g_signal_connect (rm_object, "journal-loaded", G_CALLBACK (on_journal_loaded), self);
	g_signal_connect (rm_object, "connection-changed", G_CALLBACK (on_connection_changed), self);
	g_signal_connect (rm_object, "contacts-changed", G_CALLBACK (on_contacts_changed), self);

  if (!self->hide_on_start) {
		gtk_widget_show (GTK_WIDGET (self));
	}
}

RogerJournal *
roger_journal_new (void)
{
  return g_object_new (ROGER_TYPE_JOURNAL, NULL);
}

