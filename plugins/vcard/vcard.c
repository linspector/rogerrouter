/**
 * Roger Router Copyright (c) 2012-2014 Jan-Michael Brummer
 *
 * This file is part of Roger Router.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "vcard.h"

#include <ctype.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <handy.h>
#include <rm/rm.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static GList *contacts = NULL;

static GSettings *vcard_settings = NULL;

static GList *vcard_list = NULL;
static GList *vcard = NULL;
static struct vcard_data *current_card_data = NULL;
static GString *current_string = NULL;
static gint state = STATE_NEW;
static gint current_position = 0;
static GString *first_name = NULL;
static GString *last_name = NULL;
static GString *company = NULL;
static GString *title = NULL;
static GFileMonitor *file_monitor = NULL;

gboolean vcard_reload_contacts (void);

/**
 * \brief Free header, options and entry line
 * \param psCard pointer to card structure
 */
static void
vcard_free_data (struct vcard_data *card_data)
{
  /* if header is present, free it and set to NULL */
  if (card_data->header != NULL) {
    g_free (card_data->header);
    card_data->header = NULL;
  }

  /* if options is present, free it and set to NULL */
  if (card_data->options != NULL) {
    g_free (card_data->options);
    card_data->options = NULL;
  }

  /* if entry is present, free it and set to NULL */
  if (card_data->entry != NULL) {
    g_free (card_data->entry);
    card_data->entry = NULL;
  }

  g_free (card_data);
}

/**
 * \brief Process first/last name structure
 * \param psCard pointer to card structure
 */
static void
process_first_last_name (struct vcard_data *card_data)
{
  gint len = 0;
  gint index = 0;

  if (card_data == NULL || card_data->entry == NULL) {
    return;
  }

  len = strlen (card_data->entry);

  /* Create last name string */
  last_name = g_string_new ("");
  while (index < len) {
    if (
      (card_data->entry[index] != 0x00) &&
      (card_data->entry[index] != ';') &&
      (card_data->entry[index] != 0x0A) &&
      (card_data->entry[index] != 0x0D)) {
      g_string_append_c (last_name, card_data->entry[index++]);
    } else {
      break;
    }
  }

  /* Skip ';' */
  index++;

  /* Create first name string */
  first_name = g_string_new ("");
  while (index < len) {
    if (
      (card_data->entry[index] != 0x00) &&
      (card_data->entry[index] != ';') &&
      (card_data->entry[index] != 0x0A) &&
      (card_data->entry[index] != 0x0D)) {
      g_string_append_c (first_name, card_data->entry[index++]);
    } else {
      break;
    }
  }
}

/**
 * \brief Process formatted name structure
 * \param card_data pointer to card structure
 */
static void
process_formatted_name (struct vcard_data *card_data,
                        RmContact         *contact)
{
  GString *str;
  gint len = 0;
  gint index = 0;

  g_assert (contact);
  g_assert (card_data);
  g_assert (card_data->entry);

  len = strlen (card_data->entry);

  /* Create formattedname string */
  str = g_string_new ("");
  for (index = 0; index < len; index++) {
    if (
      (card_data->entry[index] != 0x00) &&
      (card_data->entry[index] != ';') &&
      (card_data->entry[index] != 0x0A) &&
      (card_data->entry[index] != 0x0D)) {
      g_string_append_c (str, card_data->entry[index]);
    } else {
      break;
    }
  }

  contact->name = g_string_free (str, FALSE);
}

/**
 * \brief Process organization structure
 * \param psCard pointer to card structure
 */
static void
process_organization (struct vcard_data *card_data)
{
  gint len = 0;
  gint index = 0;

  if (card_data == NULL || card_data->entry == NULL) {
    return;
  }

  len = strlen (card_data->entry);

  /* Create company string */
  company = g_string_new ("");
  while (index < len) {
    if (
      (card_data->entry[index] != 0x00) &&
      (card_data->entry[index] != ';') &&
      (card_data->entry[index] != 0x0A) &&
      (card_data->entry[index] != 0x0D)) {
      g_string_append_c (company, card_data->entry[index++]);
    } else {
      break;
    }
  }
}

/**
 * \brief Process title structure
 * \param psCard pointer to card structure
 */
static void
process_title (struct vcard_data *card_data)
{
  gint len = 0;
  gint index = 0;

  if (card_data == NULL || card_data->entry == NULL) {
    return;
  }

  len = strlen (card_data->entry);

  /* Create title string */
  title = g_string_new ("");
  while (index < len) {
    if (
      (card_data->entry[index] != 0x00) &&
      (card_data->entry[index] != ';') &&
      (card_data->entry[index] != 0x0A) &&
      (card_data->entry[index] != 0x0D)) {
      g_string_append_c (title, card_data->entry[index++]);
    } else {
      break;
    }
  }
}

/**
 * \brief Process uid structure
 * \param card_data pointer to card structure
 * \param contact contact structure
 */
static void
process_uid (struct vcard_data *card_data,
             RmContact         *contact)
{
  gint len = 0;
  gint index = 0;
  GString *uid;

  g_assert (contact);

  if (card_data == NULL || card_data->entry == NULL) {
    return;
  }

  len = strlen (card_data->entry);

  /* Create uid string */
  uid = g_string_new ("");
  while (index < len) {
    if (
      (card_data->entry[index] != 0x00) &&
      (card_data->entry[index] != ';') &&
      (card_data->entry[index] != 0x0A) &&
      (card_data->entry[index] != 0x0D)) {
      g_string_append_c (uid, card_data->entry[index++]);
    } else {
      break;
    }
  }

  contact->priv = g_string_free (uid, FALSE);
}

/**
 * \brief Process address structure
 * \param card_data pointer to card structure
 */
static void
process_address (struct vcard_data *card_data,
                 RmContact         *contact)
{
  RmContactAddress *address;
  GString *tmp_str;
  char *tmp = NULL;

  g_assert (contact);

  if (card_data == NULL || card_data->entry == NULL) {
    return;
  }

  if (card_data->options == NULL) {
    g_debug ("No options for address, skipping..");
    return;
  }

  address = g_slice_new0 (RmContactAddress);

  tmp = card_data->entry;

  /* Create address string */
  if (rm_strcasestr (card_data->options, "HOME") != NULL) {
    address->type = 0;
  } else {
    address->type = 1;
  }

  /* skip pobox */
  while (*tmp != ';') {
    tmp++;
  }
  tmp++;

  /* skip extended address */
  while (*tmp != ';') {
    tmp++;
  }
  tmp++;

  /* read street */
  tmp_str = g_string_new ("");
  while (*tmp != ';') {
    g_string_append_c (tmp_str, *tmp);
    tmp++;
  }
  address->street = tmp_str->str;
  g_string_free (tmp_str, FALSE);
  tmp++;

  /* read locality */
  tmp_str = g_string_new ("");
  while (*tmp != ';') {
    g_string_append_c (tmp_str, *tmp);
    tmp++;
  }
  address->city = tmp_str->str;
  g_string_free (tmp_str, FALSE);
  tmp++;

  /* skip region */
  while (*tmp != ';') {
    tmp++;
  }
  tmp++;

  /* read zip code */
  tmp_str = g_string_new ("");
  while (*tmp != ';') {
    g_string_append_c (tmp_str, *tmp);
    tmp++;
  }
  address->zip = tmp_str->str;
  g_string_free (tmp_str, FALSE);
  tmp++;

  contact->addresses = g_list_prepend (contact->addresses, address);

  /* read country */
  /*private_country = g_string_new("");
   *  while (*tmp != 0x00 && *tmp != 0x0A && *tmp != 0x0D) {
   *       g_string_append_c(private_country, *tmp);
   *       tmp++;
   *  }*/
}

/**
 * \brief Process telephone structure
 * \param card_data pointer to card structure
 */
static void
process_telephone (struct vcard_data *card_data,
                   RmContact         *contact)
{
  char *tmp = card_data->entry;
  RmPhoneNumber *number;

  g_assert (contact);

  if (card_data->options == NULL) {
    g_warning ("No option field in telephone entry");
    return;
  }

  number = g_slice_new (RmPhoneNumber);

  if (rm_strcasestr (card_data->options, "FAX") != NULL) {
    /*if (rm_strcasestr(card_data->options, "WORK") != NULL) {
     *       if (business_fax == NULL) {
     *               business_fax = g_string_new("");
     *               while (*tmp != 0x00 && *tmp != 0x0A && *tmp != 0x0D) {
     *                       g_string_append_c(business_fax, *tmp);
     *                       tmp++;
     *               }
     *       }
     *  } else*/{
      number->type = RM_PHONE_NUMBER_TYPE_FAX_HOME;
    }
  } else {
    /* Check for cell phone number, and create string if needed */
    if (rm_strcasestr (card_data->options, "CELL") != NULL) {
      number->type = RM_PHONE_NUMBER_TYPE_MOBILE;
    }

    /* Check for home phone number, and create string if needed */
    if (rm_strcasestr (card_data->options, "HOME") != NULL) {
      number->type = RM_PHONE_NUMBER_TYPE_HOME;
    }

    /* Check for work phone number, and create string if needed */
    if (rm_strcasestr (card_data->options, "WORK") != NULL) {
      number->type = RM_PHONE_NUMBER_TYPE_WORK;
    }
  }

  GString *number_str = g_string_new ("");
  while (*tmp != 0x00 && *tmp != 0x0A && *tmp != 0x0D) {
    g_string_append_c (number_str, *tmp);
    tmp++;
  }

  number->number = rm_number_full (number_str->str, FALSE);
  g_string_free (number_str, TRUE);

  contact->numbers = g_list_prepend (contact->numbers, number);
}

/**
 * \brief Process photo structure
 * \param card_data pointer to card structure
 * \param contact contact structure
 */
static void
process_photo (struct vcard_data *card_data,
               RmContact         *contact)
{
  g_autoptr (GdkPixbufLoader) loader = NULL;
  g_autofree guchar *image_ptr = NULL;
  gsize len;
  GError *error = NULL;
  goffset offset = 0;
  char *pos = NULL;

  if (!contact) {
    return;
  }

  if (card_data->options) {
    if (rm_strcasestr (card_data->options, "VALUE=URL") != NULL) {
      return;
    }
  }

  pos = g_strstr_len (card_data->entry, -1, "BASE64,");
  if (pos) {
    offset = pos - card_data->entry + 7;
  }

  image_ptr = g_base64_decode (card_data->entry + offset, &len);
  loader = gdk_pixbuf_loader_new ();
  if (gdk_pixbuf_loader_write (loader, image_ptr, len, &error)) {
    gdk_pixbuf_loader_close (loader, NULL);
    contact->image = gdk_pixbuf_copy (gdk_pixbuf_loader_get_pixbuf (loader));
  } else {
    g_debug ("Error!! (%s)", error->message);
  }
}

/**
 * \brief Create new uid
 */
GString *
vcard_create_uid (void)
{
  GString *id = g_string_new ("");
  gint index = 0;

  for (index = 0; index < 10; index++) {
    int random = g_random_int () % 62;
    random += 48;
    if (random > 57) {
      random += 7;
    }

    if (random > 90) {
      random += 6;
    }

    id = g_string_append_c (id, (char)random);
  }

  return id;
}

/**
 * \brief Parse end of vcard, check for valid entry and add person
 */
static void
process_card_end (RmContact *contact)
{
  if (!contact) {
    return;
  }
  if (!contact->priv) {
    struct vcard_data *card_data = g_malloc0 (sizeof (struct vcard_data));
    card_data->header = g_strdup ("UID");
    GString *uid = vcard_create_uid ();
    contact->priv = g_string_free (uid, FALSE);
    card_data->entry = g_strdup (contact->priv);

    vcard = g_list_append (vcard, card_data);
  }

  if (company != NULL) {
    contact->company = g_strdup (company->str);
  }

  /*
   *  if (title != NULL) {
   *       AddInfo(table, PERSON_TITLE, title->str);
   *  }
   *  }*/

  if (!contact->name && first_name != NULL && last_name != NULL) {
    contact->name = g_strdup_printf ("%s %s", first_name->str, last_name->str);
  } else if (!contact->name) {
    contact->name = g_strdup ("");
  }

  contacts = g_list_insert_sorted (contacts, contact, rm_contact_name_compare);

  /* Free firstname */
  if (first_name != NULL) {
    g_string_free (first_name, TRUE);
    first_name = NULL;
  }

  /* Free lastname */
  if (last_name != NULL) {
    g_string_free (last_name, TRUE);
    last_name = NULL;
  }

  /* Free company */
  if (company != NULL) {
    g_string_free (company, TRUE);
    company = NULL;
  }

  /* Free title */
  if (title != NULL) {
    g_string_free (title, TRUE);
    title = NULL;
  }
}

/**
 * \brief Process new data structure (header/options/entry)
 * \param card_data pointer to card data structure
 * \param contact contact data
 */
static void
process_data (struct vcard_data *card_data)
{
  static RmContact *contact;

  if (!card_data->header || !card_data->entry) {
    return;
  }

  if (strcasecmp (card_data->header, "BEGIN") == 0) {
    /* Begin of vcard */
    vcard = g_list_append (NULL, card_data);
    vcard_list = g_list_append (vcard_list, vcard);
    contact = g_slice_new0 (RmContact);

    return;
  } else {
    vcard = g_list_append (vcard, card_data);
  }

  if (strcasecmp (card_data->header, "FN") == 0) {
    /* Full name */
    process_formatted_name (card_data, contact);
  } else if (strcasecmp (card_data->header, "END") == 0) {
    /* End of vcard */
    process_card_end (contact);
  } else if (strcasecmp (card_data->header, "N") == 0) {
    /* First and Last name */
    process_first_last_name (card_data);
  } else if (strcasecmp (card_data->header, "TEL") == 0) {
    /* Telephone */
    process_telephone (card_data, contact);
  } else if (strcasecmp (card_data->header, "ORG") == 0) {
    /* Organization */
    process_organization (card_data);
  } else if (strcasecmp (card_data->header, "TITLE") == 0) {
    /* Title */
    process_title (card_data);
  } else if (strcasecmp (card_data->header, "ADR") == 0) {
    /* Address */
    process_address (card_data, contact);
  } else if (strcasecmp (card_data->header, "PHOTO") == 0) {
    /* Photo */
    process_photo (card_data, contact);
  } else if (strcasecmp (card_data->header, "UID") == 0) {
    /* UID */
    process_uid (card_data, contact);
  }
}

/**
 * \brief Parse one char and add it to internal card structure
 * \param chr current char
 */
void
parse_char (int chr)
{
  switch (state) {
    case STATE_NEW:
      current_card_data = g_malloc0 (sizeof (struct vcard_data));
      state = STATE_TAG;
    /* fall-through */
    case STATE_TAG:
      switch (chr) {
        case '\r':
          break;
        case '\n':
          if (current_string != NULL) {
            g_string_free (current_string, TRUE);
          }
          current_string = NULL;
          vcard_free_data (current_card_data);
          state = STATE_NEW;
          break;
        case ':':
          current_card_data->header = g_string_free (current_string, FALSE);
          current_string = NULL;
          state = STATE_ENTRY;
          break;
        case ';':
          current_card_data->header = g_string_free (current_string, FALSE);
          current_string = NULL;
          state = STATE_OPTIONS;
          break;
        default:
          if (current_string == NULL) {
            current_string = g_string_new ("");
          }
          g_string_append_c (current_string, chr);
          break;
      }
      break;
    case STATE_OPTIONS:
      switch (chr) {
        case '\r':
          break;
        case '\n':
          g_string_free (current_string, TRUE);
          current_string = NULL;
          vcard_free_data (current_card_data);
          state = STATE_NEW;
          break;
        case ':':
          current_card_data->options = g_string_free (current_string, FALSE);
          current_string = NULL;
          state = STATE_ENTRY;
          break;
        default:
          if (current_string == NULL) {
            current_string = g_string_new ("");
          }
          g_string_append_c (current_string, chr);
          break;
      }
      break;
    case STATE_ENTRY:
      switch (chr) {
        case '\r':
          break;
        case '\n':
          if (current_string != NULL) {
            current_card_data->entry = g_string_free (current_string, FALSE);
            process_data (current_card_data);
          }
          current_string = NULL;
          state = STATE_NEW;
          break;
        default:
          if (current_string == NULL) {
            current_string = g_string_new ("");
          }
          g_string_append_c (current_string, chr);
          break;
      }
      break;
  }
}

/**
 * \brief VCard file change callback
 * \param monitor file monitor
 * \param file file structure
 * \param other_file unused file structure
 * \param event_type file monitor event
 * \param user_data unused pointer
 */
static void
vcard_file_changed_cb (GFileMonitor      *monitor,
                       GFile             *file,
                       GFile             *other_file,
                       GFileMonitorEvent  event_type,
                       gpointer           user_data)
{
  if (event_type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
    return;
  }

  g_debug ("%s(): %d", __FUNCTION__, event_type);

  /* Reload contacts */
  vcard_reload_contacts ();

  /* Send signal to redraw journal and update contacts view */
  rm_object_emit_contacts_changed ();
}

/**
 * \brief Load card file information
 * \param file_name file name to read
 */
void
vcard_load_file (char *file_name)
{
  GFile *file;
  GFileInfo *file_info;
  goffset file_size;
  GFileInputStream *input_stream;
  GError *error = NULL;
  g_autofree char *data = NULL;
  gint chr;
  gboolean start_of_line = TRUE;
  gboolean fold = FALSE;
  gint index;

  if (!g_file_test (file_name, G_FILE_TEST_EXISTS)) {
    g_debug ("%s(): file does not exists, abort: %s", __FUNCTION__, file_name);
    return;
  }

  /* Open file */
  file = g_file_new_for_path (file_name);
  if (!file) {
    g_warning ("%s(): could not open file %s", __FUNCTION__, file_name);
    return;
  }

  file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
  file_size = g_file_info_get_size (file_info);

  data = g_malloc0 (file_size);
  input_stream = g_file_read (file, NULL, NULL);
  g_input_stream_read_all (G_INPUT_STREAM (input_stream), data, file_size, NULL, NULL, NULL);

  state = STATE_NEW;

  for (index = 0; index < file_size; index++) {
    chr = data[index];
    if (start_of_line == TRUE) {
      if (chr == '\r' || chr == '\n') {
        /* simple empty line */
        continue;
      }

      if (fold == FALSE && isspace (chr)) {
        /* Ok, we have a fold case, mark it and continue */
        fold = TRUE;
        continue;
      }

      start_of_line = FALSE;
      if (fold == TRUE) {
        fold = FALSE;
      } else {
        parse_char ('\n');
      }
    }

    if (chr == '\n') {
      start_of_line = TRUE;
    } else {
      parse_char (chr);
    }
  }

  /* Ensure we get a '\n' */
  parse_char ('\n');

  g_input_stream_close (G_INPUT_STREAM (input_stream), NULL, NULL);

  if (file_monitor) {
    g_file_monitor_cancel (G_FILE_MONITOR (file_monitor));
  }

  file_monitor = g_file_monitor_file (file, 0, NULL, &error);
  if (file_monitor) {
    g_signal_connect (file_monitor, "changed", G_CALLBACK (vcard_file_changed_cb), NULL);
  } else {
    g_warning ("%s(): could not connect file monitor. Error: %s", __FUNCTION__, error ? error->message : "?");
  }
}

/**
 * \brief Put char to vcard structure
 * \param data data string
 * \param chr put char
 */
static void
vcard_put_char (GString *data,
                gint     chr)
{
  if (current_position == 74 && chr != '\r') {
    g_string_append (data, "\n ");
    current_position = 1;
  } else if (chr == '\n') {
    current_position = 0;
  }

  g_string_append_c (data, chr);
  current_position++;
}

/**
 * \brief printf to data structure
 * \param data data string
 * \param format format string
 */
void
vcard_print (GString *data,
             char    *format,
             ...)
{
  va_list args;
  int len;
  int size = 100;
  g_autofree char *ptr = NULL;

  while (1) {
    va_start (args, format);

    ptr = g_realloc (ptr, size);
    len = vsnprintf (ptr, size, format, args);

    va_end (args);

    if (len > -1 && len < size) {
      int index;

      for (index = 0; index < len; index++) {
        vcard_put_char (data, ptr[index]);
      }

      break;
    }

    if (len > -1) {
      size = len + 1;
    } else {
      size *= 2;
    }
  }
}

/**
 * \brief Find vcard entry via uid
 * \param uid uid
 * \param vcard entry list pointer or NULL
 */
GList *
vcard_find_entry (const char *uid)
{
  GList *list1 = NULL;
  GList *list2 = NULL;
  GList *card = NULL;
  struct vcard_data *data;
  char *current_uid;

  for (list1 = vcard_list; list1 != NULL && list1->data != NULL; list1 = list1->next) {
    card = list1->data;

    for (list2 = card; list2 != NULL && list2->data != NULL; list2 = list2->next) {
      data = list2->data;

      if (data->header && strcmp (data->header, "UID") == 0) {
        current_uid = data->entry;
        if (current_uid != NULL && !strcmp (current_uid, uid)) {
          return card;
        }
      }
    }
  }

  return NULL;
}

/**
 * \brief Find card data via header and option
 * \param list vcard list structure
 * \param header header string
 * \param option optional option string
 * \return card data structure or NULL
 */
struct vcard_data *
find_card_data (GList *list,
                char  *header,
                char  *option)
{
  GList *tmp = NULL;
  struct vcard_data *data = NULL;

  for (tmp = list; tmp != NULL && tmp->data != NULL; tmp = tmp->next) {
    data = tmp->data;

    if (data->header && strcmp (data->header, header) == 0) {
      if (!option || (data->options != NULL && strstr (data->options, option))) {
        return data;
      }
    }
  }

  return NULL;
}

gboolean
vcard_modify_data (GList *list,
                   char  *header,
                   char  *entry)
{
  struct vcard_data *card_data;

  card_data = find_card_data (list, header, NULL);

  if (card_data == NULL) {
    g_warning ("Tried to modify an non existing vcard data, return");
    return FALSE;
  } else {
    g_free (card_data->entry);
  }

  if (entry) {
    card_data->entry = g_strdup (entry);
  } else {
    card_data->entry = g_strdup ("");
  }

  return TRUE;
}

GList *
vcard_remove_data (GList *list,
                   char  *header)
{
  GList *tmp = NULL;
  struct vcard_data *data = NULL;

again:
  for (tmp = list; tmp != NULL && tmp->data != NULL; tmp = tmp->next) {
    data = tmp->data;

    if (data->header && !strcmp (data->header, header)) {
      list = g_list_remove (list, data);
      goto again;
    }
  }

  return list;
}

/**
 * \brief Write card file information
 * \param file_name file name to read
 */
void
vcard_write_file (char *file_name)
{
  GString *data = NULL;
  GList *list = NULL;
  RmContact *contact = NULL;
  GList *entry = NULL;
  GList *list2 = NULL;
  GList *numbers;
  GList *addresses;

  data = g_string_new ("");

  current_position = 0;

  for (list = contacts; list != NULL && list->data != NULL; list = list->next) {
    contact = list->data;

    if (!contact->priv) {
      struct vcard_data *card_data = g_malloc0 (sizeof (struct vcard_data));
      card_data->header = g_strdup ("UID");
      GString *uid = vcard_create_uid ();
      contact->priv = g_string_free (uid, FALSE);
      uid = NULL;
      card_data->entry = g_strdup (contact->priv);
      vcard = g_list_append (NULL, g_steal_pointer (&card_data));
      vcard_list = g_list_append (vcard_list, vcard);
    }

    entry = vcard_find_entry (contact->priv);
    if (entry == NULL) {
      continue;
    }

    vcard_print (data, "BEGIN:VCARD\n");

    /* Set version to 4.0 and remove obsolete entries */
    vcard_print (data, "VERSION:4.0\n");

    entry = vcard_remove_data (entry, "BEGIN");
    entry = vcard_remove_data (entry, "END");
    entry = vcard_remove_data (entry, "VERSION");
    entry = vcard_remove_data (entry, "N");
    entry = vcard_remove_data (entry, "LABEL");
    entry = vcard_remove_data (entry, "AGENT");

    /* formatted name */
    vcard_modify_data (entry, "FN", contact->name);

    /* telephone */
    entry = vcard_remove_data (entry, "TEL");
    for (numbers = contact->numbers; numbers != NULL; numbers = numbers->next) {
      RmPhoneNumber *number = numbers->data;
      struct vcard_data *card_data;
      g_autofree char *options = NULL;

      switch (number->type) {
        case RM_PHONE_NUMBER_TYPE_HOME:
          options = g_strdup ("TYPE=HOME,VOICE");
          break;
        case RM_PHONE_NUMBER_TYPE_WORK:
          options = g_strdup ("TYPE=WORK,VOICE");
          break;
        case RM_PHONE_NUMBER_TYPE_MOBILE:
          options = g_strdup ("TYPE=CELL");
          break;
        case RM_PHONE_NUMBER_TYPE_FAX_HOME:
          options = g_strdup ("TYPE=HOME,FAX");
          break;
        default:
          continue;
      }

      card_data = g_malloc0 (sizeof (struct vcard_data));
      card_data->options = g_strdup (options);
      card_data->header = g_strdup ("TEL");
      card_data->entry = g_strdup (number->number);
      entry = g_list_append (entry, g_steal_pointer (&card_data));
    }

    /* address */
    entry = vcard_remove_data (entry, "ADR");
    for (addresses = contact->addresses; addresses != NULL; addresses = addresses->next) {
      RmContactAddress *address = addresses->data;
      struct vcard_data *card_data;
      g_autofree char *options = NULL;

      switch (address->type) {
        case 0:
          options = g_strdup ("TYPE=HOME");
          break;
        case 1:
          options = g_strdup ("TYPE=WORK");
          break;
        default:
          continue;
      }

      card_data = g_malloc0 (sizeof (struct vcard_data));
      card_data->options = g_strdup (options);
      card_data->header = g_strdup ("ADR");
      card_data->entry = g_strdup_printf (";;%s;%s;;%s;%s",
                                          address->street,
                                          address->city,
                                          address->zip,
                                          /*address->country*/ "");

      entry = g_list_append (entry, g_steal_pointer (&card_data));
    }

    /* Handle photos with care, in case the type is url skip it */
#if 0
    if (contact->image_uri != NULL) {
      /* Ok, new image set */
      char *data = NULL;
      gsize len = 0;
      struct vcard_data *card_data;

      card_data = find_card_data (entry, "PHOTO", NULL);
      if (card_data && card_data->options && strstr (card_data->options, "VALUE=URL")) {
        /* Sorry, we cannot handled URL photos yet */
      } else {
        if (!card_data) {
          card_data = g_malloc0 (sizeof (struct vcard_data));
          card_data->header = g_strdup ("PHOTO");
          entry = g_list_append (entry, card_data);
        } else {
          g_free (card_data->entry);
        }

        if (g_file_get_contents (contact->image_uri, &data, &len, NULL)) {
          char *base64 = g_base64_encode ((const guchar *)data, len);
          if (card_data->options != NULL) {
            g_free (card_data->options);
          }
          card_data->options = g_strdup ("ENCODING=b");
          card_data->entry = g_strdup (base64);
          g_free (base64);
        }
      }
    } else
#endif
    if (contact->image == NULL) {
      /* No image available, check if contact had an image */
      struct vcard_data *card_data = find_card_data (entry, "PHOTO", NULL);

      if (card_data && card_data->options && !strstr (card_data->options, "VALUE=URL")) {
        /* Only remove previous image if it is non URL based */
        entry = vcard_remove_data (entry, "PHOTO");
      }
    }

    /* Lets add the additional data */
    for (list2 = entry; list2 != NULL && list2->data != NULL; list2 = list2->next) {
      struct vcard_data *card_data = list2->data;

      if (card_data->options != NULL) {
        vcard_print (data, "%s;%s:%s\n", card_data->header, card_data->options, card_data->entry);
      } else {
        vcard_print (data, "%s:%s\n", card_data->header, card_data->entry);
      }
    }

    vcard_print (data, "END:VCARD\n\n");
  }

  rm_file_save (file_name, data->str, data->len);

  g_string_free (data, TRUE);
}

GList *
vcard_get_contacts (void)
{
  return contacts;
}

gboolean
vcard_reload_contacts (void)
{
  char *name;

  contacts = NULL;

  name = g_settings_get_string (vcard_settings, "filename");
  vcard_load_file (name);

  return TRUE;
}

gboolean
vcard_remove_contact (RmContact *contact)
{
  char *name;

  contacts = g_list_remove (contacts, contact);

  name = g_settings_get_string (vcard_settings, "filename");
  vcard_write_file (name);

  return TRUE;
}

gboolean
vcard_save_contact (RmContact *contact)
{
  char *name;

  if (!contact->priv) {
    contacts = g_list_insert_sorted (contacts, contact, rm_contact_name_compare);
  }

  name = g_settings_get_string (vcard_settings, "filename");
  vcard_write_file (name);

  return TRUE;
}

char *
vcard_get_active_book_name (void)
{
  return g_strdup ("VCard");
}

char **
vcard_get_sub_books (void)
{
  char **ret = NULL;
  char *name = g_settings_get_string (vcard_settings, "filename");

  if (name) {
    ret = rm_strv_add (ret, name);
  }

  return ret;
}

gboolean
vcard_set_sub_book (char *name)
{
  return TRUE;
}

RmAddressBook vcard_book = {
  "VCard",
  vcard_get_active_book_name,
  vcard_get_contacts,
  vcard_remove_contact,
  vcard_save_contact,
  vcard_get_sub_books,
  vcard_set_sub_book
};

gboolean
vcard_plugin_init (RmPlugin *plugin)
{
  char *name;

  if (!vcard_settings)
    vcard_settings = rm_settings_new ("org.tabos.roger.plugins.vcard");

  name = g_settings_get_string (vcard_settings, "filename");
  if (RM_EMPTY_STRING (name)) {
    name = g_build_filename (g_get_user_data_dir (), "roger", "ab.vcf", NULL);
    g_settings_set_string (vcard_settings, "filename", name);
  }

  vcard_load_file (name);

  rm_addressbook_register (&vcard_book);

  return TRUE;
}

gboolean
vcard_plugin_shutdown (RmPlugin *plugin)
{
  rm_addressbook_unregister (&vcard_book);
  if (current_card_data)
    vcard_free_data (current_card_data);
  g_clear_object (&vcard_settings);

  return TRUE;
}

void
filename_button_clicked_cb (GtkButton *button,
                            gpointer   user_data)
{
  GtkFileChooserNative *dialog = gtk_file_chooser_native_new (_("Select vcard file"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);
  GtkFileFilter *filter;

  filter = gtk_file_filter_new ();

  gtk_file_filter_add_mime_type (filter, "text/vcard");
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
    char *folder = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

    gtk_entry_set_text (GTK_ENTRY (user_data), folder);
    contacts = NULL;
    vcard_load_file (folder);

    g_free (folder);
  }

  g_object_unref (dialog);
}

void
vcard_file_chooser_button_file_set_cb (GtkWidget *button,
                                       gpointer   user_data)
{
  char *file = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));

  g_settings_set_string (vcard_settings, "filename", file);
}

gpointer
vcard_plugin_configure (RmPlugin *plugin)
{
  GList *list = NULL;
  GtkWidget *row;

  if (!vcard_settings)
    vcard_settings = rm_settings_new ("org.tabos.roger.plugins.vcard");

  row = hdy_action_row_new ();
  hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row), _("VCard file"));

  GtkFileFilter *filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.vcf");
  GtkWidget *vcard_button = gtk_file_chooser_button_new (_("Select VCard"), GTK_FILE_CHOOSER_ACTION_OPEN);
  gtk_widget_set_valign (vcard_button, GTK_ALIGN_CENTER);
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (vcard_button), filter);
  gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (vcard_button), g_settings_get_string (vcard_settings, "filename"));
  g_signal_connect (vcard_button, "file-set", G_CALLBACK (vcard_file_chooser_button_file_set_cb), NULL);

  gtk_container_add (GTK_CONTAINER (row), vcard_button);
  list = g_list_append (list, row);

  return list;
}

RM_PLUGIN_CONFIG (vcard)
