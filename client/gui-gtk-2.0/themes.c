/********************************************************************** 
 Freeciv - Copyright (C) 2005 The Freeciv Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include "mem.h"
#include "support.h"

#include "themes_common.h"
#include "themes_g.h"

/*****************************************************************************
  Loads a gtk theme directory/theme_name
*****************************************************************************/
void gui_load_theme(const char *directory, const char *theme_name)
{
  char buf[strlen(directory) + strlen(theme_name) + 32];
  gchar *default_files[] = {buf, NULL};
  
  /* Gtk theme is a directory containing gtk-2.0/gtkrc file */
  my_snprintf(buf, sizeof(buf), "%s/%s/gtk-2.0/gtkrc", directory,
	      theme_name);

  gtk_rc_set_default_files(default_files);

  gtk_rc_reparse_all_for_settings(gtk_settings_get_default(), TRUE);
}

/*****************************************************************************
  Clears a theme (sets default system theme)
*****************************************************************************/
void gui_clear_theme(void)
{
  gchar *default_files[] = {NULL};
  
  gtk_rc_set_default_files(default_files);
  gtk_rc_reparse_all_for_settings(gtk_settings_get_default(), TRUE);
}

/*****************************************************************************
  Each gui has its own themes directories.
  For gtk2 these are:
  - /usr/share/themes
  - ~/.themes
  Returns an array containing these strings and sets array size in count.
  The caller is responsible for freeing the array and the paths.
*****************************************************************************/
char **get_gui_specific_themes_directories(int *count)
{
  gchar *standard_dir;
  char *home_dir;
  char **directories = fc_malloc(sizeof(char *) * 2);

  *count = 0;

  standard_dir = gtk_rc_get_theme_dir();
  directories[*count] = mystrdup(standard_dir);
  (*count)++;
  g_free(standard_dir);

  home_dir = user_home_dir();
  if (home_dir) {
    char buf[strlen(home_dir) + 16];
    
    my_snprintf(buf, sizeof(buf), "%s/.themes/", home_dir);
    directories[*count] = mystrdup(buf);
    (*count)++;
  }

  return directories;
}

/*****************************************************************************
  Return an array of names of usable themes in the given directory.
  Array size is stored in count.
  Useable theme for gtk+ is a directory which contains file gtk-2.0/gtkrc.
  The caller is responsible for freeing the array and the names
*****************************************************************************/
char **get_useable_themes_in_directory(const char *directory, int *count)
{
  DIR *dir;
  struct dirent *entry;
  
  char **theme_names = fc_malloc(sizeof(char *) * 2);
  /* Allocated memory size */
  int t_size = 2;


  *count = 0;

  dir = opendir(directory);
  if (!dir) {
    /* This isn't directory or we can't list it */
    return theme_names;
  }

  while ((entry = readdir(dir))) {
    char buf[strlen(directory) + strlen(entry->d_name) + 32];
    struct stat stat_result;

    my_snprintf(buf, sizeof(buf),
		"%s/%s/gtk-2.0/gtkrc", directory, entry->d_name);

    if (stat(buf, &stat_result) != 0) {
      /* File doesn't exist */
      continue;
    }
    
    if (!S_ISREG(stat_result.st_mode)) {
      /* Not a regular file */
      continue;
    }
    
    /* Otherwise it's ok */
    
    /* Increase array size if needed */
    if (*count == t_size) {
      theme_names = fc_realloc(theme_names, t_size * 2 * sizeof(char *));
      t_size *= 2;
    }

    theme_names[*count] = mystrdup(entry->d_name);
    (*count)++;
  }

  closedir(dir);

  return theme_names;
}
