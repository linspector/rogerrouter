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
#include <handy.h>

G_BEGIN_DECLS

#define ROGER_TYPE_VOICE_MAIL (roger_voice_mail_get_type ())

G_DECLARE_FINAL_TYPE (RogerVoiceMail, roger_voice_mail, ROGER, VOICE_MAIL, HdyWindow)

GtkWidget *roger_voice_mail_new (void);

void roger_voice_mail_play (RogerVoiceMail *self,
                            GBytes         *voice_mail);

G_END_DECLS
