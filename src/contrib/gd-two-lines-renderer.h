/*
 * Copyright (c) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef _GD_TWO_LINES_RENDERER_H
#define _GD_TWO_LINES_RENDERER_H

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GD_TYPE_TWO_LINES_RENDERER gd_two_lines_renderer_get_type ()
G_DECLARE_DERIVABLE_TYPE (GdTwoLinesRenderer, gd_two_lines_renderer, GD, TWO_LINES_RENDERER, GtkCellRendererText)

struct _GdTwoLinesRendererClass {
  GtkCellRendererTextClass parent_class;
};

GtkCellRenderer *gd_two_lines_renderer_new (void);

G_END_DECLS

#endif /* _GD_TWO_LINES_RENDERER_H */
