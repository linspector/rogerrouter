/**
 * Roger Router
 * Copyright (c) 2012-2013 Jan-Michael Brummer
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

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libroutermanager/profile.h>
#include <libroutermanager/router.h>
#include <libroutermanager/call.h>
#include <libroutermanager/routermanager.h>
#include <libroutermanager/filter.h>
#include <libroutermanager/appobject.h>
#include <libroutermanager/file.h>
#include <libroutermanager/osdep.h>
#include <libroutermanager/voxplay.h>
#include <libroutermanager/appobject-emit.h>
#include <libroutermanager/lookup.h>
#include <libroutermanager/csv.h>
#include <libroutermanager/gstring.h>

#include <roger/main.h>
#include <roger/phone.h>
#include <roger/journal.h>
#include <roger/print.h>
#include <roger/contact_edit.h>
#include <roger/contacts.h>
#include <roger/icons.h>
#include <roger/application.h>

#define JOURNAL_OLD_ICONS 1
//#define JOURNAL_TOP_OLD_ICONS 1

GtkWidget *journal_win = NULL;
GtkWidget *journal_filter_box = NULL;
GSList *journal_list = NULL;
GApplication *journal_application = NULL;
GdkPixbuf *icon_call_in = NULL;
GdkPixbuf *icon_call_missed = NULL;
GdkPixbuf *icon_call_out = NULL;
GdkPixbuf *icon_fax = NULL;
GdkPixbuf *icon_voice = NULL;
struct filter *journal_filter = NULL;
struct filter *journal_search_filter = NULL;
static GtkWidget *spinner = NULL;
static GMutex journal_mutex;

void profile_select(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	g_debug("Called profile_select");
}

void change_profile_state(GSimpleAction *action, GVariant *state, gpointer user_data)
{
	g_debug("Called change_profile_state");
}

void journal_clear(void)
{
	GtkListStore *list_store;

	if (!journal_win) {
		return;
	}

	list_store = g_object_get_data(G_OBJECT(journal_win), "list_store");

	gtk_list_store_clear(list_store);
}

GdkPixbuf *journal_get_call_icon(gint type)
{
#ifdef JOURNAL_OLD_ICONS
	gchar *path = NULL;
	GError *error = NULL;
#endif

	if (icon_call_in == NULL) {
#ifdef JOURNAL_OLD_ICONS
		path = g_strconcat(get_directory(APP_DATA), G_DIR_SEPARATOR_S, "callin.png", NULL);
		icon_call_in = gdk_pixbuf_new_from_file(path, &error);
		if (!icon_call_in) {
			g_debug("ERROR!!!");
			g_debug("Message: %s", error->message);
		}
		g_free(path);
#else
		GtkIconTheme *icons;

		icons = gtk_icon_theme_get_default();
		icon_call_in = gtk_icon_theme_load_icon(icons, "call-end-symbolic", 18, 0, NULL);
#endif
	}

	if (icon_call_missed == NULL) {
#ifdef JOURNAL_OLD_ICONS
		path = g_strconcat(get_directory(APP_DATA), G_DIR_SEPARATOR_S, "callmissed.png", NULL);
		icon_call_missed = gdk_pixbuf_new_from_file(path, NULL);
		g_free(path);
#else
		GtkIconTheme *icons;

		icons = gtk_icon_theme_get_default();
		icon_call_missed = gtk_icon_theme_load_icon(icons, "call-missed-symbolic", 18, 0, NULL);
#endif
	}

	if (icon_call_out == NULL) {
#ifdef JOURNAL_OLD_ICONS
		path = g_strconcat(get_directory(APP_DATA), G_DIR_SEPARATOR_S, "callout.png", NULL);
		icon_call_out = gdk_pixbuf_new_from_file(path, NULL);
		g_free(path);
#else
		GtkIconTheme *icons;

		icons = gtk_icon_theme_get_default();
		icon_call_out = gtk_icon_theme_load_icon(icons, "call-start-symbolic", 18, 0, NULL);
#endif
	}

	if (icon_fax == NULL) {
		GtkIconTheme *icons;

		icons = gtk_icon_theme_get_default();
#ifdef JOURNAL_OLD_ICONS
		icon_fax = gtk_icon_theme_load_icon(icons, "document-open", 18, 0, NULL);
#else
		icon_fax = gtk_icon_theme_load_icon(icons, "folder-documents-symbolic", 18, 0, NULL);
#endif
	}

	if (icon_voice == NULL) {
		GtkIconTheme *icons;

		icons = gtk_icon_theme_get_default();
#ifdef JOURNAL_OLD_ICONS
		icon_voice = gtk_icon_theme_load_icon(icons, "media-record", 18, 0, NULL);
#else
		icon_voice = gtk_icon_theme_load_icon(icons, "folder-music-symbolic", 18, 0, NULL);
#endif
	}

	switch (type) {
	case CALL_TYPE_INCOMING:
		return icon_call_in;
	case CALL_TYPE_MISSED:
		return icon_call_missed;
	case CALL_TYPE_OUTGOING:
		return icon_call_out;
	case CALL_TYPE_FAX:
		return icon_fax;
	case CALL_TYPE_VOICE:
		return icon_voice;
	default:
		g_debug("Unknown icon type: %d", type);
		break;
	}

	return NULL;
}


void journal_add_call(struct call *call)
{
	GtkTreeIter iter;
	GdkPixbuf *out_icon;
	GtkListStore *list_store = g_object_get_data(G_OBJECT(journal_win), "list_store");

	out_icon = journal_get_call_icon(call->type);
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set(list_store, &iter, JOURNAL_COL_TYPE, out_icon, -1);
	gtk_list_store_set(list_store, &iter, JOURNAL_COL_DATETIME, call->date_time, -1);
	gtk_list_store_set(list_store, &iter, JOURNAL_COL_NAME, call->remote->name, -1);
	gtk_list_store_set(list_store, &iter, JOURNAL_COL_COMPANY, call->remote->company, -1);
	gtk_list_store_set(list_store, &iter, JOURNAL_COL_NUMBER, call->remote->number, -1);

	gtk_list_store_set(list_store, &iter, JOURNAL_COL_CITY, call->remote->city, -1);
	gtk_list_store_set(list_store, &iter, JOURNAL_COL_EXTENSION, call->local->name, -1);
	gtk_list_store_set(list_store, &iter, JOURNAL_COL_LINE, call->local->number, -1);
	gtk_list_store_set(list_store, &iter, JOURNAL_COL_DURATION, call->duration, -1);

	gtk_list_store_set(list_store, &iter, JOURNAL_COL_CALL_PTR, call, -1);
}

void journal_redraw(void)
{
	GSList *list;
	gint duration = 0;
	GtkWidget *status;
	gchar *text = NULL;
	gint count = 0;
	struct profile *profile;

	if (!journal_win) {
		return;
	}

	status = g_object_get_data(G_OBJECT(journal_win), "statusbar");

	/* Update liststore */
	for (list = journal_list; list != NULL; list = list->next) {
		struct call *call = list->data;

		g_assert(call != NULL);

		if (filter_rule_match(journal_filter, call) == FALSE) {
			continue;
		}

		if (filter_rule_match(journal_search_filter, call) == FALSE) {
			continue;
		}

		journal_add_call(call);

		if (strchr(call->duration, 's') != NULL) {
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

	profile = profile_get_active();
	text = g_strdup_printf(_("Profile: %s | Total: %d call(s) | Duration: %d:%2.2d"), profile ? profile->name : _("<No profile>"), count, duration / 60, duration % 60);
	gtk_statusbar_push(GTK_STATUSBAR(status), 0, text);
	g_free(text);
}

static gboolean reload_journal(gpointer user_data)
{
	GtkListStore *list_store = g_object_get_data(G_OBJECT(journal_win), "list_store");
	GtkTreeIter iter;
	gboolean valid;
	struct call *call;

	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list_store), &iter);
	while (valid) {
		gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, JOURNAL_COL_CALL_PTR, &call, -1);

		if (call->remote->lookup) {
			gtk_list_store_set(list_store, &iter, JOURNAL_COL_NAME, call->remote->name, -1);
		}
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store), &iter);
	}

	g_mutex_unlock(&journal_mutex);

	gtk_spinner_stop(GTK_SPINNER(spinner));
	gtk_widget_hide(spinner);

	return FALSE;
}

static gpointer lookup_journal(gpointer user_data)
{
	GSList *journal = user_data;
	GSList *list;
	gboolean found = FALSE;

	for (list = journal; list; list = list->next) {
		struct call *call = list->data;
		gchar *name;
		gchar *street;
		gchar *zip;
		gchar *city;

		if (EMPTY_STRING(call->remote->name)) {
			if (routermanager_lookup(call->remote->number, &name, &street, &zip, &city)) {
				call->remote->name = name;
				call->remote->street = street;
				call->remote->zip = zip;
				call->remote->city = city;
				call->remote->lookup = TRUE;
				found = TRUE;
			}
		}
	}

	if (found) {
		g_idle_add(reload_journal, journal_list);
	} else {
		g_mutex_unlock(&journal_mutex);
		gtk_spinner_stop(GTK_SPINNER(spinner));
		gtk_widget_hide(spinner);
	}

	return NULL;
}

void journal_loaded_cb(AppObject *obj, GSList *journal, gpointer unused)
{
	if (g_mutex_trylock(&journal_mutex) == FALSE) {
		//g_debug("Journal loading already in progress");
		return;
	}

	/* Clear existing liststore */
	journal_clear();

	/* Set new internal list */
	if (journal_list) {
		g_slist_free_full(journal_list, call_free);
	}
	journal_list = journal;

	journal_redraw();

	g_thread_new("Lookup journal", lookup_journal, journal_list);
}

static void journal_connection_notify_cb(AppObject *obj, struct connection *connection, gpointer user_data)
{
	if (connection->type & CONNECTION_TYPE_DISCONNECT) {
		router_load_journal(profile_get_active());
	}
}

static void journal_button_refresh_clicked_cb(GtkWidget *button, GtkWidget *window)
{
	struct profile *profile = profile_get_active();

	if (!profile) {
		return;
	}

	gtk_spinner_start(GTK_SPINNER(spinner));
	gtk_widget_show(spinner);
	//journal_clear();

	if (router_load_journal(profile_get_active()) == FALSE) {
		gtk_spinner_stop(GTK_SPINNER(spinner));
		gtk_widget_hide(spinner);
	}
}

void journal_button_print_clicked_cb(GtkWidget *button, GtkWidget *view)
{
	g_debug("Print list");
	journal_print(view);
}

void journal_button_clear_clicked_cb(GtkWidget *button, GtkWidget *view)
{
	GtkWidget *remove_dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, _("Do you want to delete the router journal?"));
	gtk_window_set_title(GTK_WINDOW(remove_dialog), _("Delete journal"));
	gtk_window_set_position(GTK_WINDOW(remove_dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gint result = gtk_dialog_run(GTK_DIALOG(remove_dialog));
	gtk_widget_destroy(remove_dialog);

	if (result != GTK_RESPONSE_OK) {
		return;
	}

	g_debug("Removing journal");
	router_clear_journal(profile_get_active());
}

void delete_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	struct call *call = NULL;
	GValue ptr = { 0 };

	gtk_tree_model_get_value(model, iter, JOURNAL_COL_CALL_PTR, &ptr);

	call = g_value_get_pointer(&ptr);

	switch (call->type) {
	case CALL_TYPE_VOICE:
		router_delete_voice(profile_get_active(), call->local->name);
		break;
	case CALL_TYPE_FAX:
		router_delete_fax(profile_get_active(), call->priv);
		break;
	default:
		journal_list = g_slist_remove(journal_list, call);
		g_debug("Deleting: '%s'", call->date_time);
		csv_save_journal(journal_list);
		break;
	}
}

void journal_button_delete_clicked_cb(GtkWidget *button, GtkWidget *view)
{
	GtkWidget *delete_dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, _("Do you want to delete the selected entry?"));
	gtk_window_set_title(GTK_WINDOW(delete_dialog), _("Delete entry"));
	gtk_window_set_position(GTK_WINDOW(delete_dialog), GTK_WIN_POS_CENTER);

	gint result = gtk_dialog_run(GTK_DIALOG(delete_dialog));
	gtk_widget_destroy(delete_dialog);

	if (result != GTK_RESPONSE_OK) {
		return;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

	gtk_tree_selection_selected_foreach(selection, delete_foreach, NULL);

	router_load_journal(profile_get_active());
}

void add_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	GtkWidget *add_dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, _("Where shall i add the number?"));
	GtkWidget *content;
	GtkWidget *grid;
	GtkWidget *radio1;
	GtkWidget *radio2;
	gboolean new_entry = FALSE;

	gtk_window_set_title(GTK_WINDOW(add_dialog), _("Add entry"));
	gtk_window_set_position(GTK_WINDOW(add_dialog), GTK_WIN_POS_CENTER);

	grid = gtk_grid_new();
	content = gtk_dialog_get_content_area(GTK_DIALOG(add_dialog));

	radio1 = gtk_radio_button_new_with_label(NULL, _("Existing entry"));
	radio2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio1), _("New entry"));

	gtk_grid_attach(GTK_GRID(grid), radio1, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), radio2, 0, 1, 1, 1);

	gtk_container_add(GTK_CONTAINER(content), grid);

	gtk_widget_show_all(grid);

	gint result = gtk_dialog_run(GTK_DIALOG(add_dialog));
	new_entry = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio2));
	gtk_widget_destroy(add_dialog);

	if (result != GTK_RESPONSE_OK) {
		return;
	}

	struct call *call = NULL;
	GValue ptr = { 0 };

	gtk_tree_model_get_value(model, iter, JOURNAL_COL_CALL_PTR, &ptr);

	call = g_value_get_pointer(&ptr);

	if (new_entry) {
		contact_add_number(call->remote, call->remote->number);
		if (call->remote->lookup) {
			contact_add_address(call->remote, call->remote->street, call->remote->zip, call->remote->city);
		} else {
			contact_add_address(call->remote, "", "", call->remote->city);
		}
		contact_editor(call->remote);
	} else {
		gtk_clipboard_set_text(gtk_clipboard_get(GDK_NONE), call->remote->number, -1);
		contacts();
	}
}

void journal_button_add_clicked_cb(GtkWidget *button, GtkWidget *view)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

	gtk_tree_selection_selected_foreach(selection, add_foreach, NULL);
}

static void journal_startup(GApplication *application)
{
	journal_application = application;
}

static void search_entry_changed(GtkEditable *entry, GtkTreeView *view)
{
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));

	if (journal_search_filter) {
		filter_free(journal_search_filter);
		journal_search_filter = NULL;
	}

	if (strlen(text)) {
#ifdef JOURNAL_TOP_OLD_ICONS
		gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry), GTK_ENTRY_ICON_SECONDARY, "edit-clear");
#else
		gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry), GTK_ENTRY_ICON_SECONDARY, "edit-clear-symbolic");
#endif

		journal_search_filter = filter_new("internal_search");

		if (g_ascii_isdigit(text[0])) {
			filter_rule_add(journal_search_filter, JOURNAL_COL_NUMBER, FILTER_CONTAINS, (gchar *)text);
		} else {
			filter_rule_add(journal_search_filter, JOURNAL_COL_NAME, FILTER_CONTAINS, (gchar *)text);
		}
	} else {
		gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry), GTK_ENTRY_ICON_SECONDARY, NULL);
	}

	journal_clear();
	journal_redraw();
}

void entry_icon_released(GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, gpointer user_data)
{
	switch (icon_pos) {
	case GTK_ENTRY_ICON_PRIMARY:
		break;
	case GTK_ENTRY_ICON_SECONDARY:
		gtk_entry_set_text(entry, "");
		break;
	}
}

void filter_box_changed(GtkComboBox *box, gpointer user_data)
{
	const gchar *text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(box));
	GSList *filter_list = filter_get_list();

	if (!text) {
		journal_filter = NULL;
		journal_clear();
		journal_redraw();
		return;
	}

	while (filter_list) {
		struct filter *filter = filter_list->data;

		if (!strcmp(filter->name, text)) {
			journal_filter = filter;
			journal_clear();
			journal_redraw();
			break;
		}

		filter_list = filter_list->next;
	}
}

void journal_update_filter(void)
{
	GSList *filter_list;

	filter_list = filter_get_list();

	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(journal_filter_box));

	while (filter_list) {
		struct filter *filter = filter_list->data;

		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(journal_filter_box), NULL, filter->name);
		filter_list = filter_list->next;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(journal_filter_box), 0);
}

void journal_play_voice(const gchar *name)
{
	GtkWidget *dialog;
	GtkWidget *content_area;
	GtkWidget *label;
	gsize len = 0;
	gchar *data = router_load_voice(profile_get_active(), name, &len);
	gpointer vox_data;

	if (!data || !len) {
		g_debug("could not load file!");
		g_free(data);
		return;
	}

	/* Create dialog */
	dialog = gtk_dialog_new_with_buttons(_("Voice box"), GTK_WINDOW(journal_win), GTK_DIALOG_MODAL, _("_Close"), GTK_RESPONSE_CLOSE, NULL);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	label = gtk_label_new(_("Playing voice box entry..."));

	gtk_container_add(GTK_CONTAINER(content_area), label);
	gtk_widget_set_size_request(dialog, 300, 150);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	vox_data = vox_play(data, len);
	gtk_widget_show_all(content_area);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	vox_stop(vox_data);

	g_free(data);
}

void row_activated_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	struct call *call;

	gtk_tree_model_get(model, iter, JOURNAL_COL_CALL_PTR, &call, -1);

	switch (call->type) {
	case CALL_TYPE_FAX: {
		gsize len = 0;
		gchar *data = router_load_fax(profile_get_active(), call->priv, &len);

		if (data && len) {
			gchar *path = g_build_filename(g_get_user_cache_dir(), G_DIR_SEPARATOR_S, "fax.pdf", NULL);
			file_save(path, data, len);

			os_execute(path);
			g_free(path);
		}
		g_free(data);
		break;
	}
	case CALL_TYPE_VOICE:
		journal_play_voice(call->local->name);
		break;
	default:
		app_show_phone_window(call->remote);
		break;
	}
}

void journal_row_activated_cb(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(view);

	gtk_tree_selection_selected_foreach(selection, row_activated_foreach, NULL);
}

gpointer load_journal_thread(gpointer user_data)
{
	journal_button_refresh_clicked_cb(NULL, user_data);
	return NULL;
}

static gboolean journal_hide_on_quit = FALSE;
static gboolean journal_hide_on_start = FALSE;

void journal_set_hide_on_quit(gboolean hide)
{
	journal_hide_on_quit = hide;
	if (hide && journal_win) {
		gtk_widget_set_visible(journal_win, FALSE);
	}
}

void journal_set_hide_on_start(gboolean hide)
{
	journal_hide_on_start = hide;
}

gint journal_delete_event_cb(GtkWidget *widget, GdkEvent event, gpointer data)
{
	if (journal_hide_on_quit == TRUE) {
		gtk_widget_hide(widget);
		return TRUE;
	}

	g_mutex_trylock(&journal_mutex);
	journal_win = NULL;

	return FALSE;
}

static gint journal_sort_by_date(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer data)
{
	struct call *call_a;
	struct call *call_b;

	gtk_tree_model_get(model, a, JOURNAL_COL_CALL_PTR, &call_a, -1);
	gtk_tree_model_get(model, b, JOURNAL_COL_CALL_PTR, &call_b, -1);

	return call_sort_by_date(call_a, call_b);
}

static void name_column_cell_data_func(GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	struct call *call;

	gtk_tree_model_get(model, iter, JOURNAL_COL_CALL_PTR, &call, -1);

	if (call && call->remote->lookup) {
		g_object_set(renderer, "foreground", "darkgrey", "foreground-set", TRUE, NULL);
	} else {
		g_object_set(renderer, "foreground-set", FALSE, NULL);
	}
}

static gboolean journal_configure_event_cb(GtkWidget *window, GdkEvent *event, gpointer user_data)
{
	if (!g_settings_get_boolean(app_settings, "maximized")) {
		GdkEventConfigure *ev = (GdkEventConfigure *) event;
		g_settings_set_uint(app_settings, "width", ev->width);
		g_settings_set_uint(app_settings, "height", ev->height);
	}

	return FALSE;
}

static gboolean journal_window_state_event_cb(GtkWidget *window, GdkEventWindowState *event)
{
	gboolean maximized = event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED;

	g_settings_set_boolean(app_settings, "maximized", maximized);

	return FALSE;
}

GtkWidget *journal_window(GApplication *app, GFile *file)
{
	GtkWidget *window, *grid, *scrolled, *view;
	GtkWidget *buttonbox;
	GtkWidget *status;
	GtkWidget *entry;
	GtkWidget *button;
	GtkWidget *print_button;
	GtkWidget *clear_button;
	GtkWidget *delete_button;
	GtkWidget *add_button;
	GtkWidget *label;
	GtkWidget *image;
	GtkListStore *list_store;
	GtkTreeModel *tree_model;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *type_column;
	GtkTreeViewColumn *date_time_column;
	GtkTreeViewColumn *name_column;
	GtkTreeViewColumn *company_column;
	GtkTreeViewColumn *number_column;
	GtkTreeViewColumn *city_column;
	GtkTreeViewColumn *local_name_column;
	GtkTreeViewColumn *local_number_column;
	GtkTreeViewColumn *duration_column;
	GtkTreeSortable *sortable;
	//gint toolbar_index = 0;

	journal_startup(app);

	window = gtk_application_window_new(GTK_APPLICATION(app));
	journal_win = window;
	gtk_window_set_default_size((GtkWindow *)window, g_settings_get_uint(app_settings, "width"), g_settings_get_uint(app_settings, "height"));
	if (g_settings_get_boolean(app_settings, "maximized")) {
		gtk_window_maximize((GtkWindow *)(window));
	}

	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

	grid = gtk_grid_new();

	gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
	gtk_container_add(GTK_CONTAINER(window), grid);

	buttonbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);

	button = gtk_button_new();
	gtk_widget_set_hexpand(button, FALSE);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	image = get_icon(APP_ICON_REFRESH, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_image(GTK_BUTTON(button), image);
	gtk_widget_set_tooltip_text(GTK_WIDGET(button), _("Refresh journal"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(journal_button_refresh_clicked_cb), window);
	gtk_box_pack_start(GTK_BOX(buttonbox), button, FALSE, FALSE, 0);
	gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(buttonbox), button, TRUE);

	print_button = gtk_button_new();
	gtk_widget_set_hexpand(print_button, FALSE);
	gtk_button_set_relief(GTK_BUTTON(print_button), GTK_RELIEF_NONE);
	image = get_icon(APP_ICON_PRINT, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_image(GTK_BUTTON(print_button), image);
	gtk_widget_set_tooltip_text(GTK_WIDGET(print_button), _("Print journal"));
	gtk_box_pack_start(GTK_BOX(buttonbox), print_button, FALSE, FALSE, 0);
	gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(buttonbox), print_button, TRUE);

	clear_button = gtk_button_new();
	gtk_widget_set_hexpand(clear_button, FALSE);
	gtk_button_set_relief(GTK_BUTTON(clear_button), GTK_RELIEF_NONE);
	image = get_icon(APP_ICON_CLEAR, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_image(GTK_BUTTON(clear_button), image);
	gtk_widget_set_tooltip_text(GTK_WIDGET(clear_button), _("Clear journal on router"));
	g_signal_connect(G_OBJECT(clear_button), "clicked", G_CALLBACK(journal_button_clear_clicked_cb), window);
	gtk_box_pack_start(GTK_BOX(buttonbox), clear_button, FALSE, FALSE, 0);
	gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(buttonbox), clear_button, TRUE);

	delete_button = gtk_button_new();
	gtk_widget_set_hexpand(delete_button, FALSE);
	gtk_button_set_relief(GTK_BUTTON(delete_button), GTK_RELIEF_NONE);
	image = get_icon(APP_ICON_REMOVE, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_image(GTK_BUTTON(delete_button), image);
	gtk_widget_set_tooltip_text(GTK_WIDGET(delete_button), _("Delete selected entry"));
	gtk_box_pack_start(GTK_BOX(buttonbox), delete_button, FALSE, FALSE, 0);
	gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(buttonbox), delete_button, TRUE);

	add_button = gtk_button_new();
	gtk_widget_set_hexpand(add_button, FALSE);
	gtk_button_set_relief(GTK_BUTTON(add_button), GTK_RELIEF_NONE);
	image = get_icon(APP_ICON_ADD, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_image(GTK_BUTTON(add_button), image);
	gtk_widget_set_tooltip_text(GTK_WIDGET(add_button), _("Add selected entry to address book"));
	gtk_box_pack_start(GTK_BOX(buttonbox), add_button, FALSE, FALSE, 0);
	gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(buttonbox), add_button, TRUE);

	gtk_grid_attach(GTK_GRID(grid), buttonbox, 0, 0, 1, 1);

	label = gtk_label_new(_("Search:"));
	gtk_grid_attach(GTK_GRID(grid), label, 1, 0, 1, 1);

	entry = gtk_entry_new();
	gtk_widget_set_tooltip_text(entry, _("Search in journal for name or number"));
	g_signal_connect(G_OBJECT(entry), "icon-release", G_CALLBACK(entry_icon_released), NULL);

#ifdef JOURNAL_TOP_OLD_ICONS
	gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, "edit-find");
#else
	gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");
#endif
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_grid_attach(GTK_GRID(grid), entry, 2, 0, 1, 1);

	label = gtk_label_new(_("Filter:"));
	gtk_grid_attach(GTK_GRID(grid), label, 3, 0, 1, 1);

	journal_filter_box = gtk_combo_box_text_new();
	gtk_widget_set_vexpand(journal_filter_box, FALSE);
	journal_update_filter();

	g_signal_connect(G_OBJECT(journal_filter_box), "changed", G_CALLBACK(filter_box_changed), NULL);
	gtk_grid_attach(GTK_GRID(grid), journal_filter_box, 4, 0, 1, 1);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_hexpand(scrolled, TRUE);
	gtk_widget_set_vexpand(scrolled, TRUE);

	view = gtk_tree_view_new();

	list_store = gtk_list_store_new(10,
	                                GDK_TYPE_PIXBUF,
	                                G_TYPE_STRING,
	                                G_TYPE_STRING,
	                                G_TYPE_STRING,
	                                G_TYPE_STRING,
	                                G_TYPE_STRING,
	                                G_TYPE_STRING,
	                                G_TYPE_STRING,
	                                G_TYPE_STRING,
	                                G_TYPE_POINTER);

	g_object_set_data(G_OBJECT(window), "list_store", list_store);

	tree_model = GTK_TREE_MODEL(list_store);
	sortable = GTK_TREE_SORTABLE(list_store);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(tree_model));

	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(search_entry_changed), view);

	renderer = gtk_cell_renderer_pixbuf_new();
	type_column = gtk_tree_view_column_new_with_attributes(_("Type"), renderer, "pixbuf", JOURNAL_COL_TYPE, NULL);
	gtk_tree_view_column_set_resizable(type_column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), type_column);

	renderer = gtk_cell_renderer_text_new();
	date_time_column = gtk_tree_view_column_new_with_attributes(_("Date/Time"), renderer, "text", JOURNAL_COL_DATETIME, NULL);
	gtk_tree_view_column_set_resizable(date_time_column, TRUE);
	//gtk_tree_view_column_set_reorderable(date_time_column, TRUE);
	gtk_tree_view_column_set_sort_column_id(date_time_column, JOURNAL_COL_DATETIME);
	gtk_tree_sortable_set_sort_func(sortable, JOURNAL_COL_DATETIME, journal_sort_by_date, 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), date_time_column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ellipsize-set", TRUE, NULL);
	name_column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "text", JOURNAL_COL_NAME, NULL);
	gtk_tree_view_column_set_cell_data_func(name_column, renderer, name_column_cell_data_func, NULL, NULL);
	gtk_tree_view_column_set_resizable(name_column, TRUE);
	//gtk_tree_view_column_set_reorderable(name_column, TRUE);
	gtk_tree_view_column_set_sort_column_id(name_column, JOURNAL_COL_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), name_column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ellipsize-set", TRUE, NULL);
	company_column = gtk_tree_view_column_new_with_attributes(_("Company"), renderer, "text", JOURNAL_COL_COMPANY, NULL);
	gtk_tree_view_column_set_resizable(company_column, TRUE);
	//gtk_tree_view_column_set_reorderable(company_column, TRUE);
	gtk_tree_view_column_set_sort_column_id(company_column, JOURNAL_COL_COMPANY);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), company_column);

	renderer = gtk_cell_renderer_text_new();
	number_column = gtk_tree_view_column_new_with_attributes(_("Number"), renderer, "text", JOURNAL_COL_NUMBER, NULL);
	gtk_tree_view_column_set_resizable(number_column, TRUE);
	//gtk_tree_view_column_set_reorderable(number_column, TRUE);
	gtk_tree_view_column_set_sort_column_id(number_column, JOURNAL_COL_NUMBER);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), number_column);

	renderer = gtk_cell_renderer_text_new();
	city_column = gtk_tree_view_column_new_with_attributes(_("City"), renderer, "text", JOURNAL_COL_CITY, NULL);
	gtk_tree_view_column_set_resizable(city_column, TRUE);
	//gtk_tree_view_column_set_reorderable(city_column, TRUE);
	gtk_tree_view_column_set_sort_column_id(city_column, JOURNAL_COL_CITY);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), city_column);

	renderer = gtk_cell_renderer_text_new();
	local_name_column = gtk_tree_view_column_new_with_attributes(_("Extension"), renderer, "text", JOURNAL_COL_EXTENSION, NULL);
	gtk_tree_view_column_set_resizable(local_name_column, TRUE);
	//gtk_tree_view_column_set_reorderable(local_name_column, TRUE);
	gtk_tree_view_column_set_sort_column_id(local_name_column, JOURNAL_COL_EXTENSION);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), local_name_column);

	renderer = gtk_cell_renderer_text_new();
	local_number_column = gtk_tree_view_column_new_with_attributes(_("Line"), renderer, "text", JOURNAL_COL_LINE, NULL);
	gtk_tree_view_column_set_resizable(local_number_column, TRUE);
	//gtk_tree_view_column_set_reorderable(local_number_column, TRUE);
	gtk_tree_view_column_set_sort_column_id(local_number_column, JOURNAL_COL_LINE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), local_number_column);

	renderer = gtk_cell_renderer_text_new();
	duration_column = gtk_tree_view_column_new_with_attributes(_("Duration"), renderer, "text", JOURNAL_COL_DURATION, NULL);
	gtk_tree_view_column_set_resizable(duration_column, TRUE);
	//gtk_tree_view_column_set_reorderable(duration_column, TRUE);
	gtk_tree_view_column_set_sort_column_id(duration_column, JOURNAL_COL_DURATION);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), duration_column);

	gtk_container_add(GTK_CONTAINER(scrolled), view);

	gtk_grid_attach(GTK_GRID(grid), scrolled, 0, 1, 6, 1);

	status = gtk_statusbar_new();
	spinner = gtk_spinner_new();
	gtk_widget_set_no_show_all(spinner, TRUE);
	gtk_box_pack_end(GTK_BOX(status), spinner, FALSE, FALSE, 0);

	g_object_set_data(G_OBJECT(window), "statusbar", status);
	gtk_grid_attach(GTK_GRID(grid), status, 0, 2, 6, 1);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_MULTIPLE);

	gtk_window_set_title(GTK_WINDOW(window), "Journal");

	g_signal_connect(G_OBJECT(print_button), "clicked", G_CALLBACK(journal_button_print_clicked_cb), view);
	g_signal_connect(G_OBJECT(delete_button), "clicked", G_CALLBACK(journal_button_delete_clicked_cb), view);
	g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(journal_button_add_clicked_cb), view);
	g_signal_connect(G_OBJECT(view), "row-activated", G_CALLBACK(journal_row_activated_cb), list_store);

	g_signal_connect(G_OBJECT(journal_win), "delete-event", G_CALLBACK(journal_delete_event_cb), NULL);
	g_signal_connect(G_OBJECT(app_object), "journal-loaded", G_CALLBACK(journal_loaded_cb), NULL);

	g_signal_connect(G_OBJECT(app_object), "connection-notify", G_CALLBACK(journal_connection_notify_cb), NULL);

	g_signal_connect(G_OBJECT(journal_win), "configure-event", G_CALLBACK(journal_configure_event_cb), NULL);
	g_signal_connect(G_OBJECT(journal_win), "window-state-event", G_CALLBACK(journal_window_state_event_cb), NULL);

	gtk_window_set_hide_titlebar_when_maximized(GTK_WINDOW(journal_win), TRUE);
	gtk_widget_hide_on_delete(journal_win);
	gtk_window_set_has_resize_grip(GTK_WINDOW(journal_win), FALSE);

	filter_box_changed(GTK_COMBO_BOX(journal_filter_box), NULL);

	g_thread_new("load journal", load_journal_thread, journal_win);

	gtk_widget_show_all(GTK_WIDGET(grid));

	if (!journal_hide_on_start) {
		gtk_widget_show(GTK_WIDGET(window));
	}

	return journal_win;
}

void journal_quit(void)
{
	gtk_widget_destroy(GTK_WIDGET(journal_win));
}

GtkWidget *journal_get_window(void)
{
	return journal_win;
}
