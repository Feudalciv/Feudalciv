/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#ifndef FC__GUI_STUFF_H
#define FC__GUI_STUFF_H

#include <gtk/gtk.h>

#include "shared.h"

GtkWidget *gtk_stockbutton_new(const gchar *stock, const gchar *label_text);
void gtk_expose_now(GtkWidget *w);
void gtk_set_relative_position(GtkWidget *ref, GtkWidget *w, int px, int py);

void intl_slist(int n, char **s, bool *done);

/* the standard GTK+ 2.0 API is braindamaged. this is slightly better! */

typedef struct
{
  GtkTreeModel *model;
  gboolean end;
  GtkTreeIter it;
} ITree;

#define TREE_ITER_PTR(x)	(&(x).it)

void itree_begin(GtkTreeModel *model, ITree *it);
gboolean itree_end(ITree *it);
void itree_next(ITree *it);
void itree_get(ITree *it, ...);
void itree_set(ITree *it, ...);

void tstore_append(GtkTreeStore *store, ITree *it, ITree *parent);
void tstore_remove(ITree *it);

gboolean itree_is_selected(GtkTreeSelection *selection, ITree *it);
void itree_select(GtkTreeSelection *selection, ITree *it);
void itree_unselect(GtkTreeSelection *selection, ITree *it);

gint gtk_tree_selection_get_row(GtkTreeSelection *selection);
void gtk_tree_view_focus(GtkTreeView *view);
void setup_dialog(GtkWidget *shell, GtkWidget *parent);

#endif  /* FC__GUI_STUFF_H */
