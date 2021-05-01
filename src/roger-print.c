/*
 * Roger Router
 * Copyright (c) 2012-2021 Jan-Michael Brummer
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

#include "config.h"

#include "roger-print.h"

#include "roger-journal.h"

#include <cairo-pdf.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>
#include <rm/rm.h>
#include <string.h>
#include <strings.h>
#include <tiff.h>
#include <tiffio.h>

#define FONT "cairo:monospace 10"

#ifndef GTK_PRINT_SETTINGS_OUTPUT_BASENAME
#define GTK_PRINT_SETTINGS_OUTPUT_BASENAME "output-basename"
#endif

#define MM_TO_POINTS(mm) ((mm) / 25.4 * 72.0)

/** Structure holds information about the page and start positions of columns */
typedef struct {
  /*< private >*/
  gdouble font_width;
  gdouble line_height;
  gdouble char_width;
  PangoLayout *layout;

  GList *journal;
  gint lines_per_page;
  gint num_lines;
  gint num_pages;

  gint logo_pos;
  gint date_time_pos;
  gint name_pos;
  gint number_pos;
  gint local_name_pos;
  gint local_number_pos;
  gint duration_pos;
} RogerPrintData;

static gint
roger_print_journal_get_font_width (GtkPrintContext      *context,
                                    PangoFontDescription *desc)
{
  PangoContext *pc;
  PangoFontMetrics *metrics;
  gint width;

  pc = gtk_print_context_create_pango_context (context);

  metrics = pango_context_get_metrics (pc, desc, pango_context_get_language (pc));
  width = pango_font_metrics_get_approximate_digit_width (metrics) / PANGO_SCALE;
  if (!width) {
    width = pango_font_metrics_get_approximate_char_width (metrics) / PANGO_SCALE;
    if (!width)
      width = pango_font_description_get_size (desc) / PANGO_SCALE;
  }

  pango_font_metrics_unref (metrics);
  g_object_unref (pc);

  return width;
}

static int
roger_print_journal_get_page_count (GtkPrintContext *context,
                                    RogerPrintData  *print_data)
{
  gdouble width, height;
  gint layout_h;
  gint layout_v;

  if (print_data == NULL)
    return -1;

  width = gtk_print_context_get_width (context);
  height = gtk_print_context_get_height (context);

  pango_layout_set_width (print_data->layout, width * PANGO_SCALE);

  pango_layout_set_text (print_data->layout, "Z", -1);
  pango_layout_get_size (print_data->layout, &layout_v, &layout_h);
  if (layout_h <= 0) {
    g_debug ("Invalid layout h (%d). Falling back to default height (%d)", layout_h, 100 * PANGO_SCALE);
    layout_h = 100 * PANGO_SCALE;
  }
  if (layout_v <= 0) {
    g_debug ("Invalid layout v (%d). Falling back to default width (%d)", layout_v, 100 * PANGO_SCALE);
    layout_v = 100 * PANGO_SCALE;
  }

  print_data->line_height = (gdouble)layout_h / PANGO_SCALE + 2;
  print_data->char_width = (gdouble)layout_v / PANGO_SCALE;
  print_data->lines_per_page = ceil ((height - print_data->line_height) / print_data->line_height);

  /* 3 for header, 1 for bar */
  print_data->lines_per_page -= 5;

  return (print_data->num_lines / print_data->lines_per_page) + 1;
}

static void
roger_print_journal_begin_print_cb (GtkPrintOperation *operation,
                                    GtkPrintContext   *context,
                                    gpointer           user_data)
{
  RogerPrintData *print_data = (RogerPrintData *)user_data;
  PangoFontDescription *desc = pango_font_description_from_string (FONT);

  print_data->num_lines = g_list_length (print_data->journal);

  print_data->layout = gtk_print_context_create_pango_layout (context);
  pango_layout_set_wrap (print_data->layout, PANGO_WRAP_WORD_CHAR);
  pango_layout_set_spacing (print_data->layout, 0);
  pango_layout_set_attributes (print_data->layout, NULL);
  pango_layout_set_font_description (print_data->layout, desc);

  print_data->num_pages = roger_print_journal_get_page_count (context, print_data);
  print_data->font_width = roger_print_journal_get_font_width (context, desc) + 1;
  if (print_data->font_width == 0)
    print_data->font_width = print_data->char_width;

  print_data->logo_pos = 4;
  print_data->date_time_pos = print_data->logo_pos + print_data->font_width * 4;
  print_data->name_pos = print_data->date_time_pos + print_data->font_width * 12;
  print_data->number_pos = print_data->name_pos + print_data->font_width * 19;
  print_data->local_name_pos = print_data->number_pos + print_data->font_width * 14;
  print_data->local_number_pos = print_data->local_name_pos + print_data->font_width * 11;
  print_data->duration_pos = print_data->local_number_pos + print_data->font_width * 14;

  if (print_data->num_pages >= 0)
    gtk_print_operation_set_n_pages (operation, print_data->num_pages);

  pango_font_description_free (desc);
}

static void
roger_print_journal_show_text (cairo_t     *cairo,
                               PangoLayout *layout,
                               char        *text,
                               gint         width)
{
  gint text_width, text_height;

  pango_layout_set_text (layout, text, -1);
  pango_layout_get_pixel_size (layout, &text_width, &text_height);

  if (text_width > width) {
    pango_layout_set_width (layout, width * PANGO_SCALE);
    pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
  }
  pango_cairo_show_layout (cairo, layout);

  if (text_width > width)
    pango_layout_set_width (layout, -1);
}

static char *
roger_print_journal_get_date_time (const char *format)
{
  const struct tm *time_m;
  static char date[1024];
  g_autofree char *locale_format = NULL;
  gsize len;

  if (!g_utf8_validate (format, -1, NULL)) {
    locale_format = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
    if (locale_format == NULL)
      return NULL;
  } else {
    locale_format = g_strdup (format);
  }

  time_t time_s = time (NULL);
  time_m = localtime (&time_s);

  len = strftime (date, 1024, locale_format, time_m);

  if (len == 0)
    return NULL;

  if (!g_utf8_validate (date, len, NULL))
    return g_locale_to_utf8 (date, len, NULL, NULL, NULL);

  return g_strdup (date);
}

static void
roger_print_journal_draw_page_cb (GtkPrintOperation *operation,
                                  GtkPrintContext   *context,
                                  gint               page_nr,
                                  gpointer           user_data)
{
  RogerPrintData *print_data = (RogerPrintData *)user_data;
  g_autofree char *title = NULL;
  g_autofree char *page = NULL;
  g_autofree char *date = NULL;
  g_autofree char *tmp = NULL;
  cairo_t *cairo;
  gint line, i = 0;
  gint line_height = 0;

  cairo = gtk_print_context_get_cairo_context (context);
  gdouble width = gtk_print_context_get_width (context);

  cairo_set_source_rgb (cairo, 0, 0, 0);
  cairo_move_to (cairo, 0, 0);
  pango_layout_set_width (print_data->layout, width * PANGO_SCALE);
  pango_layout_set_alignment (print_data->layout, PANGO_ALIGN_LEFT);
  pango_layout_set_ellipsize (print_data->layout, FALSE);
  pango_layout_set_justify (print_data->layout, FALSE);

  pango_layout_set_width (print_data->layout, (width - 8) * PANGO_SCALE);

  /* Title */
  title = g_strdup_printf ("<b>%s - %s</b>", PACKAGE_NAME, _("Journal"));
  pango_layout_set_markup (print_data->layout, title, -1);
  pango_layout_set_alignment (print_data->layout, PANGO_ALIGN_CENTER);
  cairo_move_to (cairo, 3, print_data->line_height * 0.5);
  pango_cairo_show_layout (cairo, print_data->layout);

  /* Page */
  page = g_strdup_printf (_("<small>Page %d of %d</small>"), page_nr + 1, print_data->num_pages);
  pango_layout_set_markup (print_data->layout, page, -1);
  pango_layout_set_alignment (print_data->layout, PANGO_ALIGN_LEFT);
  cairo_move_to (cairo, 4, print_data->line_height * 1.5);
  pango_cairo_show_layout (cairo, print_data->layout);

  /* Date */
  tmp = roger_print_journal_get_date_time ("%d.%m.%Y %H:%M:%S");
  date = g_strdup_printf ("<small>%s</small>", tmp);
  pango_layout_set_markup (print_data->layout, date, -1);
  pango_layout_set_alignment (print_data->layout, PANGO_ALIGN_RIGHT);
  cairo_move_to (cairo, 2, print_data->line_height * 1.5);
  pango_cairo_show_layout (cairo, print_data->layout);

  /* Reset */
  cairo_move_to (cairo, 0, 0);
  pango_layout_set_width (print_data->layout, width * PANGO_SCALE);
  pango_layout_set_ellipsize (print_data->layout, FALSE);
  pango_layout_set_justify (print_data->layout, FALSE);
  pango_layout_set_attributes (print_data->layout, NULL);
  pango_layout_set_alignment (print_data->layout, PANGO_ALIGN_LEFT);

  line = page_nr * print_data->lines_per_page;

  cairo_set_line_width (cairo, 0);

  /* Draw header */
  cairo_rectangle (cairo, 2, print_data->line_height * 3, width - 2, print_data->line_height);
  cairo_set_source_rgb (cairo, 0.9, 0.9, 0.9);
  cairo_fill (cairo);
  cairo_stroke (cairo);
  cairo_set_source_rgb (cairo, 0.0, 0.0, 0.0);

  cairo_move_to (cairo, print_data->logo_pos, print_data->line_height * 3 + 1);
  pango_layout_set_text (print_data->layout, _("Type"), -1);
  pango_cairo_show_layout (cairo, print_data->layout);

  cairo_move_to (cairo, print_data->date_time_pos, print_data->line_height * 3 + 1);
  pango_layout_set_text (print_data->layout, _("Date/Time"), -1);
  pango_cairo_show_layout (cairo, print_data->layout);

  cairo_move_to (cairo, print_data->name_pos, print_data->line_height * 3 + 1);
  pango_layout_set_text (print_data->layout, _("Name"), -1);
  pango_cairo_show_layout (cairo, print_data->layout);

  cairo_move_to (cairo, print_data->number_pos, print_data->line_height * 3 + 1);
  pango_layout_set_text (print_data->layout, _("Number"), -1);
  pango_cairo_show_layout (cairo, print_data->layout);

  cairo_move_to (cairo, print_data->local_name_pos, print_data->line_height * 3 + 1);
  pango_layout_set_text (print_data->layout, _("Local Name"), -1);
  pango_cairo_show_layout (cairo, print_data->layout);

  cairo_move_to (cairo, print_data->local_number_pos, print_data->line_height * 3 + 1);
  pango_layout_set_text (print_data->layout, _("Local Number"), -1);
  pango_cairo_show_layout (cairo, print_data->layout);

  cairo_move_to (cairo, print_data->duration_pos, print_data->line_height * 3 + 1);
  pango_layout_set_text (print_data->layout, _("Duration"), -1);
  pango_cairo_show_layout (cairo, print_data->layout);

  /* print caller rows */
  for (i = 1; i <= print_data->lines_per_page && line < print_data->num_lines; i++) {
    RmCallEntry *entry = g_list_nth_data (print_data->journal, line);
    g_autoptr (GdkPixbuf) dst_pix = NULL;

    cairo_rectangle (cairo, 2, (3 + i) * print_data->line_height, width - 4, print_data->line_height);
    if (!(i & 1)) {
      cairo_set_source_rgb (cairo, 0.9, 0.9, 0.9);
      cairo_fill (cairo);
      cairo_stroke (cairo);
      cairo_set_source_rgb (cairo, 0.0, 0.0, 0.0);
    }
    cairo_stroke (cairo);

    dst_pix = gdk_pixbuf_scale_simple (roger_journal_get_call_icon (entry->type), 8, 8, GDK_INTERP_BILINEAR);

    cairo_save (cairo);
    gdk_cairo_set_source_pixbuf (cairo, dst_pix, print_data->logo_pos, (3 + i) * print_data->line_height + 2);
    cairo_paint (cairo);
    cairo_restore (cairo);

    cairo_move_to (cairo, print_data->date_time_pos, (3 + i) * print_data->line_height + 1);
    pango_layout_set_text (print_data->layout, entry->date_time, -1);
    pango_cairo_show_layout (cairo, print_data->layout);

    cairo_move_to (cairo, print_data->name_pos, (3 + i) * print_data->line_height + 1);
    roger_print_journal_show_text (cairo, print_data->layout, entry->remote->name, print_data->number_pos - print_data->name_pos);

    cairo_move_to (cairo, print_data->number_pos, (3 + i) * print_data->line_height + 1);
    roger_print_journal_show_text (cairo, print_data->layout, entry->remote->number, print_data->local_name_pos - print_data->number_pos);

    cairo_move_to (cairo, print_data->local_name_pos, (3 + i) * print_data->line_height + 1);
    if (entry->local->name != NULL && strlen (entry->local->name) > 0)
      roger_print_journal_show_text (cairo, print_data->layout, entry->local->name, print_data->local_number_pos - print_data->local_name_pos);

    cairo_move_to (cairo, print_data->local_number_pos, (3 + i) * print_data->line_height + 1);
    if (entry->local->number != NULL && strlen (entry->local->number) > 0)
      roger_print_journal_show_text (cairo, print_data->layout, entry->local->number, print_data->duration_pos - print_data->local_number_pos);

    cairo_move_to (cairo, print_data->duration_pos, (3 + i) * print_data->line_height + 1);
    if (entry->duration != NULL && strlen (entry->duration) > 0)
      roger_print_journal_show_text (cairo, print_data->layout, entry->duration, width - print_data->duration_pos);

    cairo_rel_move_to (cairo, 2, line_height);
    line++;
  }
}

static void
roger_print_journal_end_print_cb (GtkPrintOperation *operation,
                                  GtkPrintContext   *context,
                                  gpointer           user_data)
{
  RogerPrintData *print_data = (RogerPrintData *)user_data;

  g_clear_pointer (&print_data->layout, g_object_unref);
  g_free (print_data);
}

void
roger_print_journal (GList *journal)
{
  g_autoptr (GtkPrintOperation) operation = NULL;
  g_autoptr (GtkPrintSettings) settings = NULL;
  g_autoptr (GError) error = NULL;
  RogerPrintData *print_data;

  operation = gtk_print_operation_new ();
  g_object_ref (journal);

  print_data = g_new0 (RogerPrintData, 1);
  print_data->journal = journal;

  settings = gtk_print_settings_new ();
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_OUTPUT_BASENAME, _("Roger Router-Journal"));
  gtk_print_operation_set_print_settings (operation, settings);
  gtk_print_operation_set_unit (operation, GTK_UNIT_POINTS);
  gtk_print_operation_set_show_progress (operation, TRUE);

  gtk_print_operation_set_embed_page_setup (operation, TRUE);

  g_signal_connect (G_OBJECT (operation), "begin-print", G_CALLBACK (roger_print_journal_begin_print_cb), print_data);
  g_signal_connect (G_OBJECT (operation), "draw-page", G_CALLBACK (roger_print_journal_draw_page_cb), print_data);
  g_signal_connect (G_OBJECT (operation), "end-print", G_CALLBACK (roger_print_journal_end_print_cb), print_data);

  gtk_print_operation_run (operation, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, NULL, &error);
  if (error != NULL) {
    g_autoptr (GtkWidget) dialog = NULL;

    dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", error->message);
    g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
    gtk_widget_show (dialog);
  }
}

static GdkPixbuf *
roger_print_load_tiff_page (TIFF *tiff_file)
{
  TIFFRGBAImage img;
  gint row, col;
  gint width, height;
  guint32 *raster;

  /* discover image height, width */
  TIFFGetField (tiff_file, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField (tiff_file, TIFFTAG_IMAGELENGTH, &height);

  /* allocate and fill raster from file. */
  raster = (guint32 *)_TIFFmalloc (width * height * sizeof (guint32));
  TIFFRGBAImageBegin (&img, tiff_file, FALSE, NULL);
  TIFFRGBAImageGet (&img, raster, width, height);
  TIFFRGBAImageEnd (&img);

  /* vertically flip the raster */
  for (row = 0; row <= height / 2; row++) {
    for (col = 0; col < width; col++) {
      guint32 tmp;

      tmp = raster[row * width + col];
      raster[row * width + col] = raster[(height - row - 1) * width + col];
      raster[(height - row - 1) * width + col] = tmp;
    }
  }

  return gdk_pixbuf_new_from_data ((const guchar *)raster, GDK_COLORSPACE_RGB, TRUE, 8, width, height, width * 4, NULL, NULL);
}

void
print_fax_report (RmFaxStatus *status,
                  char        *file,
                  const char  *report_dir)
{
  RmProfile *profile = rm_profile_get_active ();
  RmContact *contact = NULL;
  cairo_t *cairo;
  cairo_surface_t *out;
  time_t time_s = time (NULL);
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (GdkPixbuf) scaled_pixbuf = NULL;
  TIFF *tiff;
  struct tm *time_ptr = localtime (&time_s);
  g_autofree char *buffer = NULL;
  char *remote = status->remote_number;
  char *local = status->local_number;
  char *status_code = status->error_code == 0 ? _("SUCCESS") : _("FAILED");
  int pages = status->pages_transferred;

  if (!file || !g_file_test (file, G_FILE_TEST_EXISTS)) {
    g_warning ("%s: File is invalid\n", __FUNCTION__);
    return;
  }

  if (!report_dir) {
    g_warning ("%s: report_dir is not set\n", __FUNCTION__);
    return;
  }

  tiff = TIFFOpen (file, "r");

  pixbuf = roger_print_load_tiff_page (tiff);
  if (!pixbuf) {
    g_warning ("pixbuf is null (file '%s')\n", file);
    return;
  }

  scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf, MM_TO_POINTS (594) - 140, MM_TO_POINTS (841) - 200, GDK_INTERP_BILINEAR);
  g_clear_object (&pixbuf);

  buffer = g_strdup_printf ("%s/fax-report_%s_%s_%02d_%02d_%d_%02d_%02d_%02d.pdf",
                            report_dir, local, remote,
                            time_ptr->tm_mday, time_ptr->tm_mon + 1, time_ptr->tm_year + 1900,
                            time_ptr->tm_hour, time_ptr->tm_min, time_ptr->tm_sec);
  out = cairo_pdf_surface_create (buffer, MM_TO_POINTS (594), MM_TO_POINTS (841));
  if (!out) {
    g_warning ("%s: Could not create pdf surface - is report directory writeable?\n", __FUNCTION__);
    return;
  }

  cairo = cairo_create (out);
  gdk_cairo_set_source_pixbuf (cairo, scaled_pixbuf, 70, 200);
  g_clear_object (&scaled_pixbuf);
  cairo_paint (cairo);

  cairo_set_source_rgb (cairo, 0, 0, 0);
  cairo_select_font_face (cairo, "cairo:monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cairo, 20);

  cairo_move_to (cairo, 60, 60);
  buffer = g_strconcat (_("Fax Transfer Protocol"), " (", rm_router_get_name (profile), ")", NULL);
  cairo_show_text (cairo, buffer);
  g_clear_pointer (&buffer, g_free);

  cairo_select_font_face (cairo, "cairo:monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

  /* Date/Time */
  cairo_move_to (cairo, 60, 95);
  cairo_show_text (cairo, _("Date/Time:"));
  cairo_move_to (cairo, 280, 95);
  buffer = roger_print_journal_get_date_time ("%a %b %d %Y - %X");
  cairo_show_text (cairo, buffer);
  g_clear_pointer (&buffer, g_free);

  /* Status */
  cairo_move_to (cairo, 1000, 95);
  cairo_show_text (cairo, _("Transfer status:"));
  cairo_move_to (cairo, 1220, 95);
  cairo_show_text (cairo, status_code);

  /* FAX-ID */
  cairo_move_to (cairo, 60, 120);
  cairo_show_text (cairo, _("Receiver ID:"));
  cairo_move_to (cairo, 280, 120);
  buffer = g_strdup_printf ("%s", status->remote_ident);
  cairo_show_text (cairo, buffer);
  g_clear_pointer (&buffer, g_free);

  /* Pages */
  cairo_move_to (cairo, 1000, 120);
  cairo_show_text (cairo, _("Pages sent:"));
  cairo_move_to (cairo, 1220, 120);
  buffer = g_strdup_printf ("%d", pages);
  cairo_show_text (cairo, buffer);
  g_clear_pointer (&buffer, g_free);

  /* Remote name */
  cairo_move_to (cairo, 60, 145);
  cairo_show_text (cairo, _("Recipient name:"));

  /** Ask for contact information */
  contact = rm_contact_find_by_number (remote);
  cairo_move_to (cairo, 280, 145);
  cairo_show_text (cairo, contact ? contact->name : "");

  /* Remote number */
  cairo_move_to (cairo, 1000, 145);
  cairo_show_text (cairo, _("Recipient number:"));
  cairo_move_to (cairo, 1220, 145);
  buffer = rm_number_full (remote, FALSE);
  cairo_show_text (cairo, buffer);
  g_clear_pointer (&buffer, g_free);

  /* Local name */
  cairo_move_to (cairo, 60, 170);
  cairo_show_text (cairo, _("Sender name:"));
  buffer = g_strdup_printf ("%s", status->local_ident);
  cairo_move_to (cairo, 280, 170);
  cairo_show_text (cairo, buffer);
  g_clear_pointer (&buffer, g_free);

  /* Local number */
  cairo_move_to (cairo, 1000, 170);
  cairo_show_text (cairo, _("Sender number:"));
  cairo_move_to (cairo, 1220, 170);
  buffer = rm_number_full (local, FALSE);
  cairo_show_text (cairo, buffer);
  g_clear_pointer (&buffer, g_free);

  /* line */
  cairo_set_line_width (cairo, 0.5);
  cairo_move_to (cairo, 0, 200);
  cairo_rel_line_to (cairo, MM_TO_POINTS (594), 0);
  cairo_stroke (cairo);

  cairo_show_page (cairo);
  while (TIFFReadDirectory (tiff)) {
    pixbuf = roger_print_load_tiff_page (tiff);

    scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf, MM_TO_POINTS (594), MM_TO_POINTS (841), GDK_INTERP_BILINEAR);
    g_clear_object (&pixbuf);

    gdk_cairo_set_source_pixbuf (cairo, scaled_pixbuf, 0, 0);
    g_clear_object (&scaled_pixbuf);

    cairo_paint (cairo);
    cairo_show_page (cairo);
  }

  cairo_destroy (cairo);

  cairo_surface_flush (out);
  cairo_surface_destroy (out);
}
