/*
 * Roger Router Copyright (c) 2012-2022 Jan-Michael Brummer
 *
 * This file is part of Roger Router.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <adwaita.h>
#include <rm/rm.h>

G_BEGIN_DECLS

enum {
  JOURNAL_COL_TYPE = 0,
  JOURNAL_COL_DATETIME,
  JOURNAL_COL_NAME,
  JOURNAL_COL_COMPANY,
  JOURNAL_COL_NUMBER,
  JOURNAL_COL_CITY,
  JOURNAL_COL_EXTENSION,
  JOURNAL_COL_LINE,
  JOURNAL_COL_DURATION,
  JOURNAL_COL_CALL_PTR,
};

#define ROGER_TYPE_JOURNAL (roger_journal_get_type ())

G_DECLARE_FINAL_TYPE (RogerJournal, roger_journal, ROGER, JOURNAL, AdwApplicationWindow)

GtkWidget *roger_journal_new (void);

void journal_window (GApplication *app);
void journal_set_visible (RogerJournal *self,
                          gboolean      state);
GdkPixbuf *journal_get_call_icon (gint type);

/*void journal_update_filter_box (RogerJournal *self); */
void journal_quit (RogerJournal *self);
GSList *journal_get_list (void);

GtkWidget *journal_get_window (void);
gboolean roger_uses_headerbar (void);

void journal_clear (RogerJournal *self);
void journal_init_call_icon (void);
void journal_redraw (RogerJournal *self);

void journal_update_content (RogerJournal *self);

GdkPixbuf *roger_journal_get_call_icon (RmCallEntryTypes type);

void roger_journal_reload (RogerJournal *self);

G_END_DECLS
