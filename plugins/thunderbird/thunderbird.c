/*
 * Roger Router Copyright (c) 2012-2021 Jan-Michael Brummer
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

#include <fcntl.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <handy.h>
#include <rm/rm.h>
#include <string.h>
#include <stdlib.h>

void pref_notebook_add_page (GtkWidget *notebook,
                             GtkWidget *page,
                             char      *title);
GtkWidget *pref_group_create (GtkWidget *box,
                              char      *title_str,
                              gboolean   hexpand,
                              gboolean   vexpand);

static GList *contacts = NULL;
static GSettings *thunderbird_settings = NULL;
static GHashTable *table = NULL;

#define MORK_COLUMN_META        "<(a=c)>"
#define DEFAULT_SCOPE           0x80

#define MAX_VAL                         0x7FFFFFFF

enum {
  PARSE_VALUES,
  PARSE_ROWS,
  PARSE_COLUMNS
};

static char *mork_data = NULL;
static gint mork_pos = 0;
static gint mork_now_parsing = PARSE_VALUES;
static gint mork_next_add_value_id = MAX_VAL;
static GHashTable *mork_values = NULL;
static GHashTable *mork_columns = NULL;
static GHashTable *current_cells = NULL;

static GHashTable *table_scope_map = NULL;
static gint num_possible = 0;
static gint num_persons = 0;
static off_t mork_size = 0;
static gint default_table_id = 1;

/**
 * \brief Get selected thunderbird addressbook
 * \return thunderbird addressbook
 */
static const char *
thunderbird_get_selected_book (void)
{
  return g_settings_get_string (thunderbird_settings, "filename");
}

/**
 * \brief Set selected thunderbird addressbook
 * \param uri thunderbird addressbook
 */
void
thunderbird_set_selected_book (char *uri)
{
  g_settings_set_string (thunderbird_settings, "filename", uri);
}

/**
 * \brief Destroy hashtable
 * \param data hashtable widget
 */
void
hash_destroy (void *data)
{
  g_hash_table_destroy (data);
}

/**
 * \brief Create map structure
 * \param FreeData free data function
 * \return new hash table
 */
static GHashTable *
create_map (GDestroyNotify notify)
{
  GHashTable *table;

  table = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, notify);

  return table;
}

/**
 * \brief Insert key and value into map structure, check for double entries
 * \param table pointer to hash table
 * \param key key value
 * \param value value pointer
 */
static inline void
insert_map (GHashTable *table,
            long        key,
            void       *value)
{
  g_hash_table_insert (table, GINT_TO_POINTER (key), value);
}

/**
 * \brief Find map entry by key
 * \param table pointer to hash table
 * \param key key id
 * \return value of key or NULL on error
 */
static inline void *
find_map_entry (GHashTable *table,
                int         key)
{
  return g_hash_table_lookup (table, GINT_TO_POINTER (key));
}

/**
 * \brief Find thunderbird directory
 * \return string to directory
 */
static char *
find_thunderbird_dir (void)
{
  char *buffer;
  char file[256];
  char *path;
  char *relative;
  GString *result;
  gboolean version3 = FALSE;
  gboolean is_relative = TRUE;

  result = g_string_new (NULL);
  snprintf (file, sizeof (file), "%s/.mozilla-thunderbird/profiles.ini", g_get_home_dir ());

  buffer = (char *)rm_file_load (file, NULL);
  if (buffer == NULL) {
    snprintf (file, sizeof (file), "%s/.thunderbird/profiles.ini", g_get_home_dir ());
    buffer = (char *)rm_file_load (file, NULL);
    version3 = TRUE;
  }

  if (buffer != NULL) {
    relative = strstr (buffer, "IsRelative=");
    if (relative != NULL) {
      is_relative = relative[11] == '1';
    }

    path = strstr (buffer, "Path");
    if (path != NULL) {
      path += 5;

      if (is_relative == TRUE) {
        result = g_string_append (result, g_get_home_dir ());
        if (version3 == FALSE) {
          result = g_string_append (result, "/.mozilla-thunderbird/");
        } else {
          result = g_string_append (result, "/.thunderbird/");
        }
      }

      while (path != NULL && *path != '\n') {
        result = g_string_append_c (result, (char)*path++);
      }

      while (result->str[strlen (result->str) - 1] == '\n') {
        result->str[strlen (result->str) - 1] = '\0';
      }
      result->str[strlen (result->str)] = '\0';
    }
    g_free (buffer);
  }

  return g_string_free (result, FALSE);
}

/**
 * \brief Get next char of buffer
 * \return next char
 */
static inline char
next_char (void)
{
  char cur = 0;

  if (mork_pos < mork_size) {
    cur = mork_data[mork_pos];
    mork_pos++;
  }

  return cur;
}

/**
 * \brief Check if char is whitespace
 * \param character char to check
 * \return 1 or 0
 */
static gboolean
is_whitespace (char character)
{
  switch (character) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
    case '\f':
      return TRUE;
    default:
      return FALSE;
  }
}

/**
 * \brief Parse comment section
 * \return 1 on success, else error
 */
static inline gboolean
parse_comment (void)
{
  char cur = next_char ();

  if (cur != '/') {
    return FALSE;
  }

  while (cur != '\r' && cur != '\n' && cur) {
    cur = next_char ();
  }

  return TRUE;
}

/**
 * \brief Parse cell section
 * \return 1 on success, else error
 */
static gboolean
parse_cell (void)
{
  gboolean result = TRUE;
  gboolean is_column = TRUE;
  gboolean is_value_oid = FALSE;
  GString *column = g_string_new_len (NULL, 4);
  GString *text = g_string_new_len (NULL, 32);
  int corners = 0;
  char cur = next_char ();

  while (result && cur != ')' && cur) {
    switch (cur) {
      case '=':
        /* From column to value */
        if (is_column) {
          is_column = FALSE;
        } else {
          text = g_string_append_c (text, cur);
        }
        break;
      case '$': {
        char hex_chr[3];
        int x;

        hex_chr[0] = next_char ();
        hex_chr[1] = next_char ();
        hex_chr[2] = '\0';
        x = strtoul (hex_chr, 0, 16);
        g_string_append_printf (text, "%c", x);
        break;
      }
      case '\\': {
        char next_chr = next_char ();
        if (next_chr != '\r' && next_chr != '\n') {
          text = g_string_append_c (text, next_chr);
        } else {
          /* Ignored */
          next_char ();
        }
        break;
      }
      case '^':
        corners++;
        if (corners == 1) {
        } else if (corners == 2) {
          is_column = FALSE;
          is_value_oid = TRUE;
        } else {
          text = g_string_append_c (text, cur);
        }
        break;
      default:
        if (is_column) {
          column = g_string_append_c (column, cur);
        } else {
          text = g_string_append_c (text, cur);
        }
        break;
    }

    cur = next_char ();
  }

  int column_id = strtoul (column->str, 0, 16);

  if (mork_now_parsing != PARSE_ROWS) {
    /* Dicts */
    if (text && strlen (text->str)) {
      /*g_debug("Text: %s, %lx, %d", text->str, column_id, mork_now_parsing == PARSE_COLUMNS); */
      if (mork_now_parsing == PARSE_COLUMNS) {
        insert_map (mork_columns, column_id, g_strdup (text->str));
      } else {
        insert_map (mork_values, column_id, g_strdup (text->str));
      }
    }
  } else {
    if (text && strlen (text->str)) {
      gint value_id = strtoul (text->str, 0, 16);

      /*g_debug("is_value_oid: %d", is_value_oid); */
      /* Rows */
      if (is_value_oid) {
        insert_map (current_cells, column_id, GINT_TO_POINTER (value_id));
      } else {
        mork_next_add_value_id--;
        insert_map (mork_values, mork_next_add_value_id, g_strdup (text->str));
        insert_map (current_cells, column_id, GINT_TO_POINTER (mork_next_add_value_id));
        /*g_debug("text: %s", text->str); */
      }
    }
  }

  g_string_free (column, TRUE);
  g_string_free (text, TRUE);

  return result;
}

/**
 * \brief Parse dictionary section
 * \return 1 on success, else error
 */
static gboolean
parse_dict (void)
{
  char cur = next_char ();
  gboolean result = TRUE;

  mork_now_parsing = PARSE_VALUES;

  while (result && cur != '>' && cur) {
    if (!is_whitespace (cur)) {
      switch (cur) {
        case '<':
          if (!strncmp (mork_data + mork_pos - 1, MORK_COLUMN_META, strlen (MORK_COLUMN_META))) {
            mork_now_parsing = PARSE_COLUMNS;
            mork_pos += strlen (MORK_COLUMN_META) - 1;
          }
          break;
        case '/':
          result = parse_comment ();
          break;
        case '(':
          result = parse_cell ();
          break;
        default:
          g_warning ("[%s]: error '%c'", __FUNCTION__, cur);
          result = FALSE;
          break;
      }
    }
    cur = next_char ();
  }

  return result;
}

/**
 * \brief Parse scope id
 * \param pnText while text
 * \param pid pointer to save id
 * \param scope pointer to save scope
 */
static void
parse_scope_id (GString *text,
                gint    *id,
                gint    *scope)
{
  char *pos;

  if ((pos = strchr (text->str, ':')) != NULL) {
    gint size = pos - text->str;
    char *id_str = NULL;
    char *sc_str = NULL;
    gint pos = size;

    id_str = g_malloc (size + 1);
    strncpy (id_str, text->str, pos);
    id_str[size] = '\0';

    size = strlen (text->str) - pos;
    sc_str = g_malloc (size);
    strncpy (sc_str, text->str + pos + 1, size);
    sc_str[size] = '\0';

    if (size > 1 && sc_str[0] == '^') {
      /*char *tmp = g_malloc(strlen(sc_str));
       *  strncpy(tmp, sc_str + 1, strlen(sc_str));
       *  g_free(sc_str);
       *  sc_str = tmp;*/
      memmove (sc_str, sc_str + 1, size - 1);
    }

    *id = strtoul (id_str, 0, 16);
    g_free (id_str);
    *scope = strtoul (sc_str, 0, 16);
    g_free (sc_str);
  } else {
    *id = strtoul (text->str, 0, 16);
    *scope = 0;
  }
}

/**
 * \brief Parse meta section
 * \param character current char
 * \return 1 on success, else error
 */
static char
parse_meta (char character)
{
  char cur = next_char ();

  while (cur != character && cur) {
    cur = next_char ();
  }

  return 1;
}

/**
 * \brief Set current row
 * \param table_scope table scope id
 * \param table_id table id
 * \param row_scope row scope id
 * \param row_id row id
 */
static inline void
set_current_row (int table_scope,
                 int table_id,
                 int row_scope,
                 int row_id)
{
  GHashTable *map;
  GHashTable *tmp;

  if (!row_scope) {
    row_scope = DEFAULT_SCOPE;
  }

  if (!table_scope) {
    table_scope = DEFAULT_SCOPE;
  }

  if (table_id) {
    default_table_id = table_id;
  }

  if (!table_id) {
    table_id = default_table_id;
  }

  /*g_debug("Set to %lx/%lx/%lx/%lx", table_scope, table_id, row_scope, row_id); */

  /* First: Get table scope map */
  tmp = find_map_entry (table_scope_map, abs (table_scope));
  if (tmp == NULL) {
    insert_map (table_scope_map, abs (table_scope), create_map (hash_destroy));
    tmp = find_map_entry (table_scope_map, abs (table_scope));
    if (tmp == NULL) {
      g_warning ("Could not create table scope map!!");
      return;
    }
  }
  map = tmp;

  /* Second: Get table id map */
  tmp = find_map_entry (map, abs (table_id));
  if (tmp == NULL) {
    insert_map (map, abs (table_id), create_map (hash_destroy));
    tmp = find_map_entry (map, abs (table_id));
    if (tmp == NULL) {
      g_warning ("Could not create table id map!!");
      return;
    }
  }
  map = tmp;

  /* Third: Get row scope map */
  tmp = find_map_entry (map, abs (row_scope));
  if (tmp == NULL) {
    insert_map (map, abs (row_scope), create_map (hash_destroy));
    tmp = find_map_entry (map, abs (row_scope));
    if (tmp == NULL) {
      g_warning ("Could not create row scope map!!");
      return;
    }
  }
  map = tmp;

  /* Fourth: Get row id map */
  tmp = find_map_entry (map, abs (row_id));
  if (tmp == NULL) {
    insert_map (map, abs (row_id), create_map (NULL));
    tmp = find_map_entry (map, abs (row_id));
    if (tmp == NULL) {
      g_warning ("Could not create row id map!!");
      return;
    }
  }
  map = tmp;

  current_cells = map;
}

/**
 * \brief Parse row
 * \param table_id table id
 * \param table_scope table scope id
 * \return 1 on success, else error
 */
static char
parse_row (int table_id,
           int table_scope)
{
  char result = 1;
  char cur = next_char ();
  GString *text = g_string_new (NULL);
  int id, scope;

  mork_now_parsing = PARSE_ROWS;

  while (cur != '(' && cur != ']' && cur != '[' && cur) {
    if (!is_whitespace (cur)) {
      text = g_string_append_c (text, cur);
    }
    cur = next_char ();
  }

  parse_scope_id (text, &id, &scope);
  set_current_row (table_scope, table_id, scope, id);

  while (result && cur != ']' && cur) {
    if (!is_whitespace (cur)) {
      switch (cur) {
        case '(':
          result = parse_cell ();
          break;
        case '[':
          result = parse_meta (']');
          break;
        default:
          result = 0;
          break;
      }
    }
    cur = next_char ();
  }

  g_string_free (text, TRUE);

  return result;
}

/**
 * \brief Parse table section
 * \return 1 on success, else error
 */
static gboolean
parse_table (void)
{
  gboolean result = TRUE;
  GString *text_id = g_string_new (NULL);
  gint id = 0, scope = 0;
  char cur = next_char ();

  while (cur != '{' && cur != '[' && cur != '}' && cur) {
    if (!is_whitespace (cur)) {
      text_id = g_string_append_c (text_id, cur);
    }
    cur = next_char ();
  }

  parse_scope_id (text_id, &id, &scope);

  while (result && cur != '}' && cur) {
    if (!is_whitespace (cur)) {
      switch (cur) {
        case '{':
          result = parse_meta ('}');
          break;
        case '[':
          result = parse_row (id, scope);
          break;
        case '-':
        case '+':
          break;
        default: {
          GString *just_id = g_string_new (NULL);

          while (!is_whitespace (cur) && cur) {
            just_id = g_string_append_c (just_id, cur);
            cur = next_char ();

            if (cur == '}') {
              g_string_free (just_id, TRUE);
              g_string_free (text_id, TRUE);
              return result;
            }
          }

          int just_id_num = 0, just_scope_num = 0;
          parse_scope_id (just_id, &just_id_num, &just_scope_num);
          set_current_row (scope, id, just_scope_num, just_id_num);
          g_string_free (just_id, TRUE);
          break;
        }
      }
    }
    cur = next_char ();
  }

  g_string_free (text_id, TRUE);

  return result;
}

/**
 * \brief Parse group section
 * \return 1 on success, else error
 */
static char
parse_group (void)
{
  return parse_meta ('@');
}

/**
 * \brief Parse mork code
 * \return 1 on success, else error
 */
static gboolean
parse_mork (void)
{
  gboolean result = TRUE;
  char cur = 0;

  cur = next_char ();

  while (result && cur) {
    if (!is_whitespace (cur)) {
      switch (cur) {
        case '/':
          /* Comment */
          result = parse_comment ();
          break;
        case '<':
          /* Dict */
          result = parse_dict ();
          break;
        case '{':
          /* Table */
          result = parse_table ();
          break;
        case '@':
          /* Group */
          result = parse_group ();
          break;
        case '[':
          /* Row */
          result = parse_row (0, 0);
          break;
        default:
          g_warning ("Error: %c", cur);
          result = FALSE;
          break;
      }
    }
    cur = next_char ();
  }

  g_free (mork_data);
  mork_data = NULL;

  return result;
}

/**
 * \brief Get column entry by key
 * \param key key id
 * \return column entry
 */
static inline char *
get_column (int key)
{
  return g_hash_table_lookup (mork_columns, GINT_TO_POINTER (key));
}

/**
 * \brief Get value entry by key
 * \param key key id
 * \return value entry
 */
static inline char *
get_value (int key)
{
  return g_hash_table_lookup (mork_values, GINT_TO_POINTER (key));
}

/**
 * \brief Parse person data
 * \param map pointer to map structure holding person informations
 * \param pId id
 */
static void
parse_person (GHashTable *map,
              gpointer    pId)
{
  /*const char *check = NULL; */
  /*GdkPixbuf *image = NULL; */
  const char *home_street = NULL;
  const char *home_city = NULL;
  const char *home_zip = NULL;
  const char *business_street = NULL;
  const char *business_city = NULL;
  const char *business_zip = NULL;
  GHashTableIter iter5;
  gpointer key5, value5;
  RmContact *contact = NULL;
  RmPhoneNumber *number;
  const char *thunderbird_dir = thunderbird_get_selected_book ();

  if (thunderbird_dir) {
    thunderbird_dir = g_path_get_dirname (thunderbird_dir);
  }

  num_possible++;

#ifdef THUNDERBIRD_DEBUG
  g_debug ("***** possible: %d", num_possible);
#endif

  contact = g_slice_new0 (RmContact);

  g_hash_table_iter_init (&iter5, map);
  while (g_hash_table_iter_next (&iter5, &key5, &value5)) {
    if (GPOINTER_TO_INT (key5) == 0) {
      continue;
    }
    const char *column = get_column (GPOINTER_TO_INT (key5));
    const char *value = get_value (GPOINTER_TO_INT (value5));
#ifdef THUNDERBIRD_DEBUG
    g_debug ("'%s' = '%s'", column, value);
#endif

    if (!strcmp (column, "HomePhone")) {
      number = g_slice_new (RmPhoneNumber);
      number->number = rm_number_full (value, FALSE);
      number->type = RM_PHONE_NUMBER_TYPE_HOME;
      contact->numbers = g_list_prepend (contact->numbers, number);
    } else if (!strcmp (column, "WorkPhone")) {
      number = g_slice_new (RmPhoneNumber);
      number->number = rm_number_full (value, FALSE);
      number->type = RM_PHONE_NUMBER_TYPE_WORK;
      contact->numbers = g_list_prepend (contact->numbers, number);
    } else if (!strcmp (column, "FaxNumber")) {
      number = g_slice_new (RmPhoneNumber);
      number->number = rm_number_full (value, FALSE);
      number->type = RM_PHONE_NUMBER_TYPE_FAX_HOME;
      contact->numbers = g_list_prepend (contact->numbers, number);
    } else if (!strcmp (column, "CellularNumber")) {
      number = g_slice_new (RmPhoneNumber);
      number->number = rm_number_full (value, FALSE);
      number->type = RM_PHONE_NUMBER_TYPE_MOBILE;
      contact->numbers = g_list_prepend (contact->numbers, number);
    } else if (!strcmp (column, "DisplayName")) {
      contact->name = g_strdup (value);
    } else if (!strcmp (column, "HomeAddress")) {
      home_street = value;
    } else if (!strcmp (column, "HomeCity")) {
      home_city = value;
    } else if (!strcmp (column, "HomeZipCode")) {
      home_zip = value;
    } else if (!strcmp (column, "WorkAddress")) {
      business_street = value;
    } else if (!strcmp (column, "WorkCity")) {
      business_city = value;
    } else if (!strcmp (column, "WorkZipCode")) {
      business_zip = value;
    } else if (!strcmp (column, "PhotoName")) {
      char *file_name = g_build_filename (thunderbird_dir, "Photos", value, NULL);

      contact->image = gdk_pixbuf_new_from_file (file_name, NULL);
      g_free (file_name);
    }
  }

  /* Do not add entries without name */
  if (RM_EMPTY_STRING (contact->name)) {
    return;
  }

  if (home_city || home_zip || home_street) {
    RmContactAddress *address = g_slice_new0 (RmContactAddress);

    address->city = g_strdup (home_city ? home_city : "");
    address->zip = g_strdup (home_zip ? home_zip : "");
    address->street = g_strdup (home_street ? home_street : "");
    address->type = 0;

    contact->addresses = g_list_prepend (contact->addresses, address);
  }

  if (business_city || business_zip || business_street) {
    RmContactAddress *address = g_slice_new0 (RmContactAddress);

    address->city = g_strdup (business_city ? business_city : "");
    address->zip = g_strdup (business_zip ? business_zip : "");
    address->street = g_strdup (business_street ? business_street : "");
    address->type = 1;

    contact->addresses = g_list_prepend (contact->addresses, address);
  }

  contacts = g_list_insert_sorted (contacts, contact, rm_contact_name_compare);
  num_persons++;
}

/**
 * \brief Parse tables for important informations
 */
static void
parse_tables (void)
{
  GHashTable *tables = table_scope_map;

  if (tables != NULL) {
    GHashTableIter iter1;
    gpointer key1, value1;

    g_hash_table_iter_init (&iter1, tables);
    while (g_hash_table_iter_next (&iter1, &key1, &value1)) {
      if (GPOINTER_TO_INT (key1) == 0) {
        /*continue; */
      }

      GHashTable *rows = value1;
      if (rows != NULL) {
        GHashTableIter iter2;
        gpointer key2, value2;

        g_hash_table_iter_init (&iter2, rows);
        while (g_hash_table_iter_next (&iter2, &key2, &value2)) {
          if (GPOINTER_TO_INT (key2) == 0) {
            /*continue; */
          }

          GHashTable *rows2 = value2;
          if (rows2 != NULL) {
            GHashTableIter iter3;
            gpointer key3, value3;

            g_hash_table_iter_init (&iter3, rows2);
            while (g_hash_table_iter_next (&iter3, &key3, &value3)) {
              if (GPOINTER_TO_INT (key3) == 0) {
                /*continue; */
              }

              GHashTable *rows3 = value3;
              if (rows3 != NULL) {
                GHashTableIter iter4;
                gpointer key4, value4;

                g_hash_table_iter_init (&iter4, rows3);
                while (g_hash_table_iter_next (&iter4, &key4, &value4)) {
                  if (GPOINTER_TO_INT (key4) == 0) {
                    /*continue; */
                  }

                  GHashTable *rows4 = value4;
                  if (rows4 != NULL) {
                    parse_person (rows4, key4);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

/**
 * \brief Open thunderbird address book
 * \param book address book file name
 */
static void
thunderbird_open_book (char *book)
{
  int file;
  off_t size;

  file = open (book, O_RDONLY);
  if (file == -1) {
    return;
  }

  size = lseek (file, 0, SEEK_END);
  if (size == -1) {
    close (file);
    return;
  }

  lseek (file, 0, SEEK_SET);

  mork_data = g_malloc (size);
  if (mork_data != NULL) {
    mork_size = size;
    if (read (file, mork_data, size) == size) {
#ifdef THUNDERBIRD_DEBUG
      g_debug ("Parsing mork");
#endif
      parse_mork ();
#ifdef THUNDERBIRD_DEBUG
      g_debug ("Parsing tables");
#endif
      parse_tables ();
#ifdef THUNDERBIRD_DEBUG
      g_debug ("Done");
#endif
    }
  }

  close (file);
}

/**
 * \brief Read thunderbird book
 * \return error code
 */
static int
thunderbird_read_book (void)
{
  const char *book;
  char file[256];

  num_persons = 0;
  num_possible = 0;

  mork_data = NULL;
  mork_pos = 0;
  mork_now_parsing = PARSE_VALUES;
  mork_next_add_value_id = MAX_VAL;
  mork_values = NULL;
  mork_columns = NULL;
  current_cells = NULL;
  table_scope_map = NULL;
  default_table_id = 1;

  mork_values = create_map (free);
  mork_columns = create_map (free);
  table_scope_map = create_map (hash_destroy);

  book = thunderbird_get_selected_book ();
  memset (file, 0, sizeof (file));
  strncpy (file, book, sizeof (file) - 1);

#ifdef THUNDERBIRD_DEBUG
  g_debug ("Thunderbird book (%s)", file);
#endif
  thunderbird_open_book (file);

  g_hash_table_destroy (table_scope_map);
  g_hash_table_destroy (mork_columns);
  g_hash_table_destroy (mork_values);

#ifdef THUNDERBIRD_DEBUG
  g_debug ("%d entries!", num_possible);
  g_debug ("%d persons imported!", num_persons);
#endif

  return 0;
}

GList *
thunderbird_get_contacts (void)
{
  GList *list = contacts;

  return list;
}

gboolean
thunderbird_reload_contacts (void)
{
  contacts = NULL;
  thunderbird_read_book ();

  return TRUE;
}

char *
thunderbird_get_active_book_name (void)
{
  return g_strdup ("Thunderbird");
}


char **
thunderbird_get_sub_books (void)
{
  char **ret = NULL;
  const char *name = thunderbird_get_selected_book ();

  if (name) {
    ret = rm_strv_add (ret, name);
  }

  return ret;
}

gboolean
thunderbird_set_sub_book (char *name)
{
  return TRUE;
}

RmAddressBook thunderbird_book = {
  "Thunderbird",
  thunderbird_get_active_book_name,
  thunderbird_get_contacts,
  NULL,      /*thunderbird_remove_contact, */
  NULL,      /*thunderbird_save_contact, */
  thunderbird_get_sub_books,
  thunderbird_set_sub_book
};

static void
thunderbird_settings_init (void)
{
  if (thunderbird_settings)
    return;

  thunderbird_settings = rm_settings_new ("org.tabos.roger.plugins.thunderbird");
}

gboolean
thunderbird_plugin_init (RmPlugin *plugin)
{
  thunderbird_settings_init ();
  table = g_hash_table_new (g_str_hash, g_str_equal);

  thunderbird_read_book ();

  rm_addressbook_register (&thunderbird_book);

  return TRUE;
}

gboolean
thunderbird_plugin_shutdown (RmPlugin *plugin)
{
  rm_addressbook_unregister (&thunderbird_book);
  g_clear_object (&thunderbird_settings);

  if (table) {
    g_hash_table_destroy (table);
  }

  return TRUE;
}

/*static void thunderbird_filename_button_clicked_cb(GtkButton *button, gpointer user_data)
 *  {
 *       GtkFileChooserNative *dialog = gtk_file_chooser_native_new(_("Select mab file"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);
 *       GtkFileFilter *filter;
 *       const char *book;
 *       char *dir;
 *       char file[256];
 *
 *       filter = gtk_file_filter_new();
 *       gtk_file_filter_add_pattern(filter, "*.mab");
 *
 *       gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);
 *
 *       book = thunderbird_get_selected_book();
 *       if (book != NULL && strlen(book) > 0) {
 *               gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), book);
 *       } else {
 *               dir = find_thunderbird_dir();
 *               snprintf(file, sizeof(file), "%s/abook.mab", dir);
 *
 *               gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), file);
 *               g_free(dir);
 *       }
 *
 *       if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
 *               char *folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
 *
 *               gtk_entry_set_text(GTK_ENTRY(user_data), folder);
 *               contacts = NULL;
 *               thunderbird_set_selected_book(folder);
 *
 *               thunderbird_read_book();
 *
 *               g_free(folder);
 *       }
 *
 *       g_object_unref(dialog);
 *  }*/

void
thunderbird_file_chooser_button_file_set_cb (GtkWidget *button,
                                             gpointer   user_data)
{
  char *file = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));

  thunderbird_set_selected_book (file);
}

gpointer
thunderbird_plugin_configure (RmPlugin *plugin)
{
  GList *list = NULL;
  GtkWidget *row;
  GtkFileFilter *filter;
  const char *book;

  thunderbird_settings_init ();

  row = hdy_action_row_new ();
  hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row), _("Thunderbird file"));

  GtkWidget *thunderbird_chooser = gtk_file_chooser_button_new (_("Select Thunderbird Addressbook"), GTK_FILE_CHOOSER_ACTION_OPEN);
  gtk_widget_set_valign (thunderbird_chooser, GTK_ALIGN_CENTER);
  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.mab");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (thunderbird_chooser), filter);

  book = thunderbird_get_selected_book ();
  if (book != NULL && strlen (book) > 0) {
    gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (thunderbird_chooser), book);
  } else {
    g_autofree char *file = g_strdup_printf ("file:///%s/abook.mab", find_thunderbird_dir ());

    gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (thunderbird_chooser), file);
  }
  g_signal_connect (thunderbird_chooser, "file-set", G_CALLBACK (thunderbird_file_chooser_button_file_set_cb), NULL);

  gtk_container_add (GTK_CONTAINER (row), thunderbird_chooser);

  list = g_list_append (list, row);

  return list;
}

RM_PLUGIN_CONFIG (thunderbird)
