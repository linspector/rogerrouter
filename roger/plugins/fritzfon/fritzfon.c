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

#include <gtk/gtk.h>

#include <libroutermanager/plugins.h>
#include <libroutermanager/appobject.h>
#include <libroutermanager/address-book.h>
#include <libroutermanager/profile.h>
#include <libroutermanager/router.h>
#include <libroutermanager/network.h>
#include <libroutermanager/number.h>
#include <libroutermanager/gstring.h>
#include <libroutermanager/ftp.h>
#include <libroutermanager/xml.h>
#include <libroutermanager/logging.h>

#include <roger/main.h>

#define ROUTERMANAGER_TYPE_FRITZFON_PLUGIN        (routermanager_fritzfon_plugin_get_type ())
#define ROUTERMANAGER_FRITZFON_PLUGIN(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), ROUTERMANAGER_TYPE_FRITZFON_PLUGIN, RouterManagerFritzFonPlugin))

typedef struct {
	guint signal_id;
} RouterManagerFritzFonPluginPrivate;

ROUTERMANAGER_PLUGIN_REGISTER_CONFIGURABLE(ROUTERMANAGER_TYPE_FRITZFON_PLUGIN, RouterManagerFritzFonPlugin, routermanager_fritzfon_plugin)

void pref_notebook_add_page(GtkWidget *notebook, GtkWidget *page, gchar *title);
GtkWidget *pref_group_create(GtkWidget *box, gchar *title_str, gboolean hexpand, gboolean vexpand);

static GSList *contacts = NULL;
static GSettings *fritzfon_settings = NULL;
static GHashTable *table = NULL;

static xmlnode *master_node = NULL;

struct fritzfon_book {
	gchar *id;
	gchar *name;
};

struct fritzfon_priv {
	gchar *services;
	gchar *setup;
	gchar *unique_id;
};

static GSList *fritzfon_books = NULL;

static void contact_add(struct profile *profile, xmlnode *node, gint count)
{
	xmlnode *person;
	xmlnode *name;
	xmlnode *image;
	xmlnode *telephony;
	xmlnode *child;
	xmlnode *tmp;
	gchar *number = NULL;
	gchar *image_ptr;
	struct contact *contact;
	struct phone_number *phone_number;
	struct fritzfon_priv *priv;

	/* Get person entry */
	person = xmlnode_get_child(node, "person");
	if (person == NULL) {
		return;
	}

	/* Get real name entry */
	name = xmlnode_get_child(person, "realName");
	if (name == NULL) {
		return;
	}

	contact = g_malloc0(sizeof(struct contact));

	contact->name = xmlnode_get_data(name);
	contact->numbers = NULL;

	priv = g_slice_new0(struct fritzfon_priv);
	contact->priv = priv;

	telephony = xmlnode_get_child(node, "telephony");
	if (telephony) {
		/* Check for numbers */
		for (child = xmlnode_get_child(telephony, "number"); child != NULL; child = xmlnode_get_next_twin(child)) {
			const gchar *type;

			type = xmlnode_get_attrib(child, "type");
			if (type == NULL) {
				continue;
			}

			number = xmlnode_get_data(child);
			if (!EMPTY_STRING(number)) {
				phone_number = g_slice_new(struct phone_number);
				if (strcmp(type, "mobile") == 0) {
					phone_number->type = PHONE_NUMBER_MOBILE;
				} else if (strcmp(type, "home") == 0) {
					phone_number->type = PHONE_NUMBER_HOME;
				} else if (strcmp(type, "work") == 0) {
					phone_number->type = PHONE_NUMBER_WORK;
				} else if (strcmp(type, "fax_work") == 0) {
					phone_number->type = PHONE_NUMBER_FAX;
				} else {
					phone_number->type = -1;
				}
				phone_number->number = call_full_number(number, FALSE);
				contact->numbers = g_slist_prepend(contact->numbers, phone_number);
			}

			g_free(number);
		}
	}

	/* Get image */
	image = xmlnode_get_child(person, "imageURL");
	if (image != NULL) {
		image_ptr = xmlnode_get_data(image);
		if (image_ptr != NULL) {
			/* file:///var/InternerSpeicher/FRITZ/fonpix/946684999-0.jpg */
			if (!strncmp(image_ptr, "file://", 7) && strlen(image_ptr) > 28) {
				gchar *url = strstr(image_ptr, "/ftp/");
				gsize len;
				guchar *buffer;
				struct ftp *client;
				GdkPixbufLoader *loader;

				if (!url) {
					url = strstr(image_ptr, "/FRITZ/");
				} else {
					url += 5;
				}

				client = ftp_init(router_get_host(profile_get_active()));
				ftp_login(client, router_get_ftp_user(profile), router_get_ftp_password(profile));
				ftp_passive(client);
				buffer = (guchar *) ftp_get_file(client, url, &len);

				loader = gdk_pixbuf_loader_new();
				if (gdk_pixbuf_loader_write(loader, buffer, len, NULL)) {
					contact->image = gdk_pixbuf_loader_get_pixbuf(loader);
					contact->image_len = len;
				}
				gdk_pixbuf_loader_close(loader, NULL);
			}/* else {
				g_debug("Unhandled image utl: '%s'", image_ptr);
			}*/
			g_free(image_ptr);
		}
	}

	tmp = xmlnode_get_child(person, "unique_id");
	priv->unique_id = xmlnode_get_data(tmp);

	contacts = g_slist_insert_sorted(contacts, contact, contact_name_compare);
}

static void phonebook_add(struct profile *profile, xmlnode *node) {
	xmlnode *child;
	gint count = 0;

	for (child = xmlnode_get_child(node, "contact"); child != NULL; child = xmlnode_get_next_twin(child)) {
		contact_add(profile, child, count++);
	}
}

static gint fritzfon_read_book(void) {
	gchar uri[1024];
	xmlnode *node = NULL;
	xmlnode *child;
	struct profile *profile = profile_get_active();

	if (!router_login(profile)) {
		return -1;
	}

	snprintf(uri, sizeof(uri), "http://%s/cgi-bin/firmwarecfg", router_get_host(profile));

	SoupMultipart *multipart = soup_multipart_new(SOUP_FORM_MIME_TYPE_MULTIPART);
	soup_multipart_append_form_string(multipart, "sid", profile->router_info->session_id);
	soup_multipart_append_form_string(multipart, "PhonebookId", g_settings_get_string(fritzfon_settings, "book"));
	soup_multipart_append_form_string(multipart, "PhonebookExportName", "Dummy");
	soup_multipart_append_form_string(multipart, "PhonebookExport", "");
	SoupMessage *msg = soup_form_request_new_from_multipart(uri, multipart);

	soup_session_send_message(soup_session_sync, msg);
	if (msg->status_code != 200) {
		g_warning("Could not read boxinfo file");
		g_object_unref(msg);
		g_free(uri);
		return FALSE;
	}
	
	const gchar *data = msg->response_body->data;
	gint read = msg->response_body->length;

	g_return_val_if_fail(data != NULL, -2);
	if (read > 0) {
		log_save_data("test-in.xml", data, read);
	}


	node = xmlnode_from_str(data, read);
	if (node == NULL) {
		g_object_unref(msg);
		return -1;
	}

	master_node = node;

	for (child = xmlnode_get_child(node, "phonebook"); child != NULL; child = xmlnode_get_next_twin(child)) {
		phonebook_add(profile, child);
	}

	g_object_unref(msg);

	//router_logout(profile);

	return 0;
}

GSList *fritzfon_get_contacts(void)
{
	GSList *list = contacts;

	return list;
}

static gint fritzfon_get_books(void) {
	gchar *url;
	struct profile *profile = profile_get_active();
	SoupMessage *msg;
	struct fritzfon_book *book = NULL;

	if (!router_login(profile)) {
		return -1;
	}

	url = g_strdup_printf("http://%s/fon_num/fonbook_select.lua", router_get_host(profile));
	msg = soup_form_request_new(SOUP_METHOD_GET, url,
		"sid", profile->router_info->session_id,
		NULL);
	g_free(url);

	soup_session_send_message(soup_session_sync, msg);
	if (msg->status_code != 200) {
		g_warning("Could not read boxinfo file");
		g_object_unref(msg);
		goto end;
	}
	
	const gchar *data = msg->response_body->data;

	//gint read = msg->response_body->length;
	//log_save_data("fritzfon-getbooks.html", data, read);

	g_return_val_if_fail(data != NULL, -2);

	gchar *pos = (gchar *)data;
	do {
		pos = strstr(pos, "<label for=\"uiBookid:");
		if (pos) {
			/* Extract ID */
			gchar *end = strstr(pos + 22, "\"");
			g_assert(end != NULL);
			gint len = end - pos - 21;
			gchar *num = g_malloc0(len + 1);
			strncpy(num, pos + 21, len);

			/* Extract Name */
			pos = end;
			end = strstr(pos + 2, "\n");
			g_assert(end != NULL);
			len = end - pos - 1;
			gchar *name = g_malloc0(len);
			strncpy(name, pos + 2, len - 1);
			pos = end;

			book = g_slice_new(struct fritzfon_book);
			book->id = num;
			book->name = name;

			fritzfon_books = g_slist_prepend(fritzfon_books, book);
		} else {
			break;
		}

		pos++;
	} while (pos != NULL);

	g_object_unref(msg);

end:
	if (g_slist_length(fritzfon_books) == 0) {
		book = g_slice_new(struct fritzfon_book);
		book->id = g_strdup("0");
		book->name = g_strdup("Telefonbuch");

		fritzfon_books = g_slist_prepend(fritzfon_books, book);
	}

	//router_logout(profile);

	return 0;
}

void fritzfon_combobox_changed_cb(GtkComboBox *widget, gpointer user_data)
{
	/* GSettings has not written the changed value to its container, so we explicit set it here */
	GtkWidget *combo_box = user_data;

	g_settings_set_string(fritzfon_settings, "book", gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo_box)));

	contacts = NULL;
	fritzfon_read_book();
}

gint fritzfon_number_compare(gconstpointer a, gconstpointer b)
{
	struct contact *contact = (struct contact *)a;
	gchar *number = (gchar *)b;
	GSList *list = contact->numbers;

	while (list) {
		struct phone_number *phone_number = list->data;
		if (g_strcmp0(phone_number->number, number) == 0) {
			return 0;
		}

		list = list->next;
	}

	return -1;
}

void fritzfon_contact_process_cb(AppObject *obj, struct contact *contact, gpointer user_data)
{
	struct contact *fritz_contact;

	if (EMPTY_STRING(contact->number)) {
		/* Contact number is not present, abort */
		return;
	}

	fritz_contact = g_hash_table_lookup(table, contact->number);
	if (fritz_contact) {
		if (!EMPTY_STRING(fritz_contact->name)) {
			contact_copy(fritz_contact, contact);
		} else {
			/* Previous lookup done but no result found */
			return;
		}
	} else {
		GSList *list;
		gchar *full_number = call_full_number(contact->number, FALSE);

		list = g_slist_find_custom(contacts, full_number, fritzfon_number_compare);
		if (list) {
			fritz_contact = list->data;

			g_hash_table_insert(table, contact->number, fritz_contact);

			contact_copy(fritz_contact, contact);
		} else {
			/* We have found no entry, mark it in table to speedup further lookup */
			fritz_contact = g_malloc0(sizeof(struct contact));
			g_hash_table_insert(table, contact->number, fritz_contact);
		}

		g_free(full_number);
	}
}

gboolean fritzfon_reload_contacts(void)
{
	return fritzfon_read_book() == 0;
}


xmlnode *create_phone(char *type, char *number) {
	xmlnode *number_node;

	number_node = xmlnode_new("number");
	xmlnode_set_attrib(number_node, "type", type);

	/* For the meantime set priority to 0, TODO */
	xmlnode_set_attrib(number_node, "prio", "0");
	xmlnode_insert_data(number_node, number, -1);

	return number_node;
}

/**
 * \brief Convert person structure to xml node
 * \param contact person structure
 * \return xml node
 */
static xmlnode *contact_to_xmlnode(struct contact *contact) {
	xmlnode *node;
	xmlnode *contact_node;
	xmlnode *realname_node;
	xmlnode *image_node;
	xmlnode *telephony_node;
	xmlnode *category_node;
	xmlnode *tmp_node;
	GSList *list;
	gchar *tmp;
	struct fritzfon_priv *priv = contact->priv;

	/* Main contact entry */
	node = xmlnode_new("contact");

	/* Category */
	category_node = xmlnode_new("category");
	/* FIXME: Save and set category */
	//xmlnode_insert_data(category_node, "0"/*contact->priv->category*/, -1);
	xmlnode_insert_child(node, category_node);

	/* Person */
	contact_node = xmlnode_new("person");

	realname_node = xmlnode_new("realName");
	xmlnode_insert_data(realname_node, contact->name, -1);
	xmlnode_insert_child(contact_node, realname_node);

	/* ImageURL stub */
	if (contact->image) {
		image_node = xmlnode_new("ImageURL");
		xmlnode_insert_child(contact_node, image_node);
	}

	/* Insert person to main node */
	xmlnode_insert_child(node, contact_node);

	/* Telephony */
	if (contact->numbers) {
		gboolean first = TRUE;
		gint id = 0;
		gchar *tmp = g_strdup_printf("%d", g_slist_length(contact->numbers));

		telephony_node = xmlnode_new("telephony");
		xmlnode_set_attrib(telephony_node, "nid", tmp);
		g_free(tmp);

		for (list = contact->numbers; list != NULL; list = list->next) {
			struct phone_number *number = list->data;
			xmlnode *number_node;

			number_node = xmlnode_new("number");

			switch (number->type) {
				case PHONE_NUMBER_HOME:
					xmlnode_set_attrib(number_node, "type", "home");
					break;
				case PHONE_NUMBER_WORK:
					xmlnode_set_attrib(number_node, "type", "work");
					break;
				case PHONE_NUMBER_MOBILE:
					xmlnode_set_attrib(number_node, "type", "mobile");
					break;
				case PHONE_NUMBER_FAX:
					xmlnode_set_attrib(number_node, "type", "fax_work");
					break;
				default:
					continue;
			}

			if (first) {
				/* For the meantime set priority to 1 */
				xmlnode_set_attrib(number_node, "prio", "1");
				first = FALSE;
			}

			tmp = g_strdup_printf("%d", id++);
			xmlnode_set_attrib(number_node, "id", tmp);
			g_free(tmp);

			xmlnode_insert_data(number_node, number->number, -1);
			xmlnode_insert_child(telephony_node, number_node);
		}
		xmlnode_insert_child(node, telephony_node);
	}

	tmp_node = xmlnode_new("services");
	xmlnode_insert_child(node, tmp_node);

	tmp_node = xmlnode_new("setup");
	xmlnode_insert_child(node, tmp_node);

	tmp_node = xmlnode_new("mod_time");
	tmp = g_strdup_printf("%u", (unsigned)time(NULL));
	xmlnode_insert_data(tmp_node, tmp, -1);
	xmlnode_insert_child(node, tmp_node);
	g_free(tmp);

	tmp_node = xmlnode_new("uniqueid");
	xmlnode_insert_data(tmp_node, priv->unique_id, -1);
	xmlnode_insert_child(node, tmp_node);

	return node;
}

/**
 * \brief Convert phonebooks to xml node
 * \return xml node
 */
xmlnode *phonebook_to_xmlnode(void) {
	xmlnode *node;
	xmlnode *child;
	xmlnode *book;
	GSList *list;

	/* Create general phonebooks node */
	node = xmlnode_new("phonebooks");

	/* Currently we only support one phonebook, TODO */
	book = xmlnode_new("phonebook");
	xmlnode_set_attrib(book, "name", g_settings_get_string(fritzfon_settings, "book"));
	xmlnode_insert_child(node, book);

	/* Loop through persons list and add only non-deleted entries */
	for (list = contacts; list != NULL; list = list->next) {
		struct contact *contact = list->data;

		/* Convert each contact and add it to current phone book */
		child = contact_to_xmlnode(contact);
		xmlnode_insert_child(book, child);
	}

	return node;
}

gboolean fritzfon_remove_contact(struct contact *contact)
{
	xmlnode *node;
	gchar uri[1024];
	struct profile *profile = profile_get_active();
	gchar *file;
	gchar *data;
	gint len;

	if (!router_login(profile)) {
		return FALSE;
	}

	contacts = g_slist_remove(contacts, contact);

	node = phonebook_to_xmlnode();

	data = xmlnode_to_formatted_str(node, &len);
	g_debug("len: %d", len);
	if (len > 0) {
		log_save_data("test.xml", data, len);
	}

	if (1 == 0) {
	snprintf(uri, sizeof(uri), "http://%s/cgi-bin/firmwarecfg", router_get_host(profile));

	SoupMultipart *multipart = soup_multipart_new(SOUP_FORM_MIME_TYPE_MULTIPART);
	soup_multipart_append_form_string(multipart, "sid", profile->router_info->session_id);
	soup_multipart_append_form_string(multipart, "PhonebookId", g_settings_get_string(fritzfon_settings, "book"));
	soup_multipart_append_form_string(multipart, "PhonebookImportFile", file);
	SoupMessage *msg = soup_form_request_new_from_multipart(uri, multipart);

	soup_session_send_message(soup_session_sync, msg);
	if (msg->status_code != 200) {
		g_warning("Could not send phonebook");
		g_object_unref(msg);
		g_free(uri);
		return FALSE;
	}
	}

	return FALSE;
}

gboolean fritzfon_save_contact(struct contact *contact)
{
	return FALSE;
}

struct address_book fritzfon_book = {
	fritzfon_get_contacts,
	fritzfon_reload_contacts,
	fritzfon_remove_contact,
	fritzfon_save_contact
};

void impl_activate(PeasActivatable *plugin)
{
	RouterManagerFritzFonPlugin *fritzfon_plugin = ROUTERMANAGER_FRITZFON_PLUGIN(plugin);

	fritzfon_settings = g_settings_new("org.tabos.roger.plugins.fritzfon");

	table = g_hash_table_new(g_str_hash, g_str_equal);

	fritzfon_get_books();
	fritzfon_read_book();

	fritzfon_plugin->priv->signal_id = g_signal_connect(G_OBJECT(app_object), "contact-process", G_CALLBACK(fritzfon_contact_process_cb), NULL);

	routermanager_address_book_register(&fritzfon_book);
}

void impl_deactivate(PeasActivatable *plugin)
{
	RouterManagerFritzFonPlugin *fritzfon_plugin = ROUTERMANAGER_FRITZFON_PLUGIN(plugin);

	if (g_signal_handler_is_connected(G_OBJECT(app_object), fritzfon_plugin->priv->signal_id)) {
		g_signal_handler_disconnect(G_OBJECT(app_object), fritzfon_plugin->priv->signal_id);
	}

	g_object_unref(fritzfon_settings);

	if (table) {
		g_hash_table_destroy(table);
	}
}

GtkWidget *impl_create_configure_widget(PeasGtkConfigurable *config)
{
	GtkWidget *combo_box;
	GtkWidget *label;
	GtkWidget *box;
	GtkWidget *group;
	GSList *list;

	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	combo_box = gtk_combo_box_text_new();
	label = gtk_label_new("");

	gtk_label_set_markup(GTK_LABEL(label), _("Book:"));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, TRUE, 10);

	for (list = fritzfon_books; list != NULL; list= list->next) {
		struct fritzfon_book *book = list->data;

		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_box), book->id, book->name);
	}

	gtk_box_pack_start(GTK_BOX(box), combo_box, FALSE, TRUE, 5);
	g_settings_bind(fritzfon_settings, "book", combo_box, "active-id", G_SETTINGS_BIND_DEFAULT);
	g_signal_connect(combo_box, "changed", G_CALLBACK(fritzfon_combobox_changed_cb), combo_box);

	group = pref_group_create(box, _("Contact book"), TRUE, FALSE);

	return group;
}
