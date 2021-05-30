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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ROGER_TYPE_SHELL (roger_shell_get_type ())

G_DECLARE_FINAL_TYPE (RogerShell, roger_shell, ROGER, SHELL, GtkApplication)

RogerShell *roger_shell_get_default (void);
GtkWidget *roger_shell_get_journal (RogerShell *self);
void roger_shell_phone (RogerShell *self,
                        char       *number);

G_END_DECLS
