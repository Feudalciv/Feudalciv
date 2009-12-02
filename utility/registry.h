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
#ifndef FC__REGISTRY_H
#define FC__REGISTRY_H

#include "ioz.h"
#include "support.h"            /* bool type and fc__attribute */

/* Opaque types. */
struct section_file;
struct section;
struct entry;

/* Create a 'struct section_list' and related functions: */
#define SPECLIST_TAG section
#include "speclist.h"
#define section_list_iterate(seclist, psection) \
       TYPED_LIST_ITERATE(struct section, seclist, psection)
#define section_list_iterate_end  LIST_ITERATE_END
#define section_list_iterate_rev(seclist, psection) \
       TYPED_LIST_ITERATE_REV(struct section, seclist, psection)
#define section_list_iterate_rev_end  LIST_ITERATE_REV_END

/* Create a 'struct entry_list' and related functions: */
#define SPECLIST_TAG entry
#include "speclist.h"
#define entry_list_iterate(entlist, pentry) \
       TYPED_LIST_ITERATE(struct entry, entlist, pentry)
#define entry_list_iterate_end  LIST_ITERATE_END

/* Global functions. */
const char *secfile_error(void);

/* Main functions. */
struct section_file *secfile_new(bool allow_duplicates);
struct section_file *secfile_load(const char *filename,
                                  bool allow_duplicates);
struct section_file *secfile_load_section(const char *filename,
                                          const char *section,
                                          bool allow_duplicates);
struct section_file *secfile_from_stream(fz_FILE *stream,
                                         bool allow_duplicates);
void secfile_destroy(struct section_file *secfile);

bool secfile_save(const struct section_file *secfile, const char *filename,
                  int compression_level, enum fz_method compression_method);
void secfile_check_unused(const struct section_file *secfile);
const char *secfile_name(const struct section_file *secfile);

/* Insertion functions. */
struct entry *secfile_insert_bool_full(struct section_file *secfile,
                                       bool value, const char *comment,
                                       bool allow_replace,
                                       const char *path, ...)
                                       fc__attribute((__format__(__printf__, 5, 6)));
#define secfile_insert_bool(secfile, value, path, ...)                      \
  secfile_insert_bool_full(secfile, value, NULL, FALSE,                     \
                           path, ## __VA_ARGS__)
#define secfile_insert_bool_comment(secfile, value, comment, path, ...)     \
  secfile_insert_bool_full(secfile, value, comment, FALSE,                  \
                           path, ## __VA_ARGS__)
#define secfile_replace_bool(secfile, value, path, ...)                     \
  secfile_insert_bool_full(secfile, value, NULL, TRUE,                      \
                           path, ## __VA_ARGS__)
#define secfile_replace_bool_comment(secfile, value, comment, path, ...)    \
  secfile_insert_bool_full(secfile, value, comment, TRUE,                   \
                           path, ## __VA_ARGS__)
size_t secfile_insert_bool_vec_full(struct section_file *secfile,
                                    const bool *values, size_t dim,
                                    const char *comment, bool allow_replace,
                                    const char *path, ...)
                                    fc__attribute((__format__(__printf__, 6, 7)));
#define secfile_insert_bool_vec(secfile, values, dim, path, ...)            \
  secfile_insert_bool_vec_full(secfile, values, dim, NULL, FALSE,           \
                               path, ## __VA_ARGS__)
#define secfile_insert_bool_vec_comment(secfile, values, dim,               \
                                        comment, path, ...)                 \
  secfile_insert_bool_vec_full(secfile, values, comment, FALSE,             \
                               path, ## __VA_ARGS__)
#define secfile_replace_bool_vec(secfile, values, dim, path, ...)           \
  secfile_insert_bool_vec_full(secfile, values, NULL, TRUE,                 \
                               path, ## __VA_ARGS__)
#define secfile_replace_bool_vec_comment(secfile, values, dim,              \
                                         comment, path, ...)                \
  secfile_insert_bool_vec_full(secfile, values, comment, TRUE,              \
                               path, ## __VA_ARGS__)

struct entry *secfile_insert_int_full(struct section_file *secfile,
                                      int value, const char *comment,
                                      bool allow_replace,
                                      const char *path, ...)
                                      fc__attribute((__format__ (__printf__, 5, 6)));
#define secfile_insert_int(secfile, value, path, ...)                       \
  secfile_insert_int_full(secfile, value, NULL, FALSE,                      \
                          path, ## __VA_ARGS__)
#define secfile_insert_int_comment(secfile, value, comment, path, ...)      \
  secfile_insert_int_full(secfile, value, comment, FALSE,                   \
                          path, ## __VA_ARGS__)
#define secfile_replace_int(secfile, value, path, ...)                      \
  secfile_insert_int_full(secfile, value, NULL, TRUE,                       \
                          path, ## __VA_ARGS__)
#define secfile_replace_int_comment(secfile, value, comment, path, ...)     \
  secfile_insert_int_full(secfile, value, comment, TRUE,                    \
                          path, ## __VA_ARGS__)
size_t secfile_insert_int_vec_full(struct section_file *secfile,
                                   const int *values, size_t dim,
                                   const char *comment, bool allow_replace,
                                   const char *path, ...)
                                   fc__attribute((__format__ (__printf__, 6, 7)));
#define secfile_insert_int_vec(secfile, values, dim, path, ...)             \
  secfile_insert_int_vec_full(secfile, values, dim, NULL, FALSE,            \
                              path, ## __VA_ARGS__)
#define secfile_insert_int_vec_comment(secfile, values, dim,                \
                                       comment, path, ...)                  \
  secfile_insert_int_vec_full(secfile, values, dim, comment, FALSE,         \
                              path, ## __VA_ARGS__)
#define secfile_replace_int_vec(secfile, values, dim, path, ...)            \
  secfile_insert_int_vec_full(secfile, values, dim,  NULL, TRUE,            \
                              path, ## __VA_ARGS__)
#define secfile_replace_int_vec_comment(secfile, values, dim,               \
                                        comment, path, ...)                 \
  secfile_insert_int_vec_full(secfile, values, dim, comment, TRUE,          \
                              path, ## __VA_ARGS__)

struct entry *secfile_insert_str_full(struct section_file *secfile,
                                      const char *string,
                                      const char *comment,
                                      bool allow_replace, bool no_escape,
                                      const char *path, ...)
                                      fc__attribute((__format__(__printf__, 6, 7)));
#define secfile_insert_str(secfile, string, path, ...)                      \
  secfile_insert_str_full(secfile, string, NULL, FALSE, FALSE,              \
                          path, ## __VA_ARGS__)
#define secfile_insert_str_noescape(secfile, string, path, ...)             \
  secfile_insert_str_full(secfile, string, NULL, FALSE, TRUE,               \
                          path, ## __VA_ARGS__)
#define secfile_insert_str_comment(secfile, string, comment, path, ...)     \
  secfile_insert_str_full(secfile, string, comment, FALSE, TRUE,            \
                          path, ## __VA_ARGS__)
#define secfile_insert_str_noescape_comment(secfile, string,                \
                                            comment, path, ...)             \
  secfile_insert_str_full(secfile, string, comment, FALSE, TRUE,            \
                          path, ## __VA_ARGS__)
#define secfile_replace_str(secfile, string, path, ...)                     \
  secfile_insert_str_full(secfile, string, NULL, TRUE, FALSE,               \
                          path, ## __VA_ARGS__)
#define secfile_replace_str_noescape(secfile, string, path, ...)            \
  secfile_insert_str_full(secfile, string, NULL, TRUE, TRUE,                \
                          path, ## __VA_ARGS__)
#define secfile_replace_str_comment(secfile, string, comment, path, ...)    \
  secfile_insert_str_full(secfile, string, comment, TRUE, TRUE,             \
                          path, ## __VA_ARGS__)
#define secfile_replace_str_noescape_comment(secfile, string,               \
                                             comment, path, ...)            \
  secfile_insert_str_full(secfile, string, comment, TRUE, TRUE,             \
                          path, ## __VA_ARGS__)
size_t secfile_insert_str_vec_full(struct section_file *secfile,
                                   const char *const *strings, size_t dim,
                                   const char *comment, bool allow_replace,
                                   bool no_escape, const char *path, ...)
                                   fc__attribute((__format__(__printf__, 7, 8)));
#define secfile_insert_str_vec(secfile, strings, dim, path, ...)            \
  secfile_insert_str_vec_full(secfile, strings, dim, NULL, FALSE, FALSE,    \
                              path, ## __VA_ARGS__)
#define secfile_insert_str_vec_noescape(secfile, strings, dim, path, ...)   \
  secfile_insert_str_vec_full(secfile, strings, dim, NULL, FALSE, TRUE,     \
                              path, ## __VA_ARGS__)
#define secfile_insert_str_vec_comment(secfile, strings, dim,               \
                                       comment, path, ...)                  \
  secfile_insert_str_vec_full(secfile, strings, dim, comment, FALSE, TRUE,  \
                              path, ## __VA_ARGS__)
#define secfile_insert_str_vec_noescape_comment(secfile, strings, dim,      \
                                                comment, path, ...)         \
  secfile_insert_str_vec_full(secfile, strings, dim, comment, FALSE, TRUE,  \
                              path, ## __VA_ARGS__)
#define secfile_replace_str_vec(secfile, strings, dim, path, ...)           \
  secfile_insert_str_vec_full(secfile, strings, dim, NULL, TRUE, FALSE,     \
                              path, ## __VA_ARGS__)
#define secfile_replace_str_vec_noescape(secfile, strings, dim, path, ...)  \
  secfile_insert_str_vec_full(secfile, strings, dim, NULL, TRUE, TRUE,      \
                              path, ## __VA_ARGS__)
#define secfile_replace_str_vec_comment(secfile, strings, dim,              \
                                        comment, path, ...)                 \
  secfile_insert_str_vec_full(secfile, strings, dim, comment, TRUE, TRUE,   \
                              path, ## __VA_ARGS__)
#define secfile_replace_str_vec_noescape_comment(secfile, strings, dim,     \
                                                 comment, path, ...)        \
  secfile_insert_str_vec_full(secfile, strings, dim, comment, TRUE, TRUE,   \
                              path, ## __VA_ARGS__)

/* Lookup functions. */
struct entry *secfile_entry_by_path(const struct section_file *secfile,
                                    const char *entry_path);
struct entry *secfile_entry_lookup(const struct section_file *secfile,
                                   const char *path, ...)
                                   fc__attribute((__format__ (__printf__, 2, 3)));

bool secfile_lookup_bool(const struct section_file *secfile, bool *bval,
                         const char *path, ...)
                         fc__attribute((__format__ (__printf__, 3, 4)));
bool secfile_lookup_bool_default(const struct section_file *secfile,
                                 bool def, const char *path, ...)
                                 fc__attribute((__format__ (__printf__, 3, 4)));
bool *secfile_lookup_bool_vec(const struct section_file *secfile,
                              size_t *dim, const char *path, ...)
                              fc__attribute((__format__ (__printf__, 3, 4)));

bool secfile_lookup_int(const struct section_file *secfile, int *ival,
                        const char *path, ...)
                        fc__attribute((__format__ (__printf__, 3, 4)));
int secfile_lookup_int_default(const struct section_file *secfile, int def,
                               const char *path, ...)
                               fc__attribute((__format__ (__printf__, 3, 4)));
int secfile_lookup_int_def_min_max(const struct section_file *secfile,
                                int defval, int minval, int maxval,
                                const char *path, ...)
                                fc__attribute((__format__ (__printf__, 5, 6)));
int *secfile_lookup_int_vec(const struct section_file *secfile,
                            size_t *dim, const char *path, ...)
                            fc__attribute((__format__ (__printf__, 3, 4)));

const char *secfile_lookup_str(const struct section_file *secfile,
                               const char *path, ...)
                               fc__attribute((__format__ (__printf__, 2, 3)));
const char *secfile_lookup_str_default(const struct section_file *secfile,
                                       const char *def,
                                       const char *path, ...)
                                       fc__attribute((__format__ (__printf__, 3, 4)));
const char **secfile_lookup_str_vec(const struct section_file *secfile,
                                    size_t *dim, const char *path, ...)
                                    fc__attribute((__format__ (__printf__, 3, 4)));

/* Sections functions. */
struct section *secfile_section_by_name(const struct section_file *secfile,
                                        const char *section_name);
struct section *secfile_section_lookup(const struct section_file *secfile,
                                       const char *path, ...)
                                       fc__attribute((__format__ (__printf__, 2, 3)));
const struct section_list *
secfile_sections(const struct section_file *secfile);
struct section_list *
secfile_sections_by_name_prefix(const struct section_file *secfile,
                                const char *prefix);
struct section *secfile_section_new(struct section_file *secfile,
                                    const char *section_name);


/* Independant section functions. */
void section_destroy(struct section *psection);
void section_clear_all(struct section *psection);

const char *section_name(const struct section *psection);
bool section_set_name(struct section *psection, const char *section_name);

/* Entry functions. */
const struct entry_list *section_entries(const struct section *psection);
struct entry *section_entry_by_name(const struct section *psection,
                                    const char *entry_name);
struct entry *section_entry_lookup(const struct section *psection,
                                   const char *path, ...)
                                   fc__attribute((__format__ (__printf__, 2, 3)));
struct entry *section_entry_int_new(struct section *psection,
                                    const char *entry_name,
                                    int value);
struct entry *section_entry_bool_new(struct section *psection,
                                     const char *entry_name,
                                     bool value);
struct entry *section_entry_str_new(struct section *psection,
                                    const char *entry_name,
                                    const char *value, bool escaped);

/* Independant entry functions. */
enum entry_type {
  ENTRY_BOOL,
  ENTRY_INT,
  ENTRY_STR
};

void entry_destroy(struct entry *pentry);

struct section *entry_section(const struct entry *pentry);
enum entry_type entry_type(const struct entry *pentry);
int entry_path(const struct entry *pentry, char *buf, size_t buf_len);

const char *entry_name(const struct entry *pentry);
bool entry_set_name(struct entry *pentry, const char *entry_name);

const char *entry_comment(const struct entry *pentry);
void entry_set_comment(struct entry *pentry, const char *comment);

bool entry_int_get(const struct entry *pentry, int *value);
bool entry_int_set(struct entry *pentry, int value);

bool entry_bool_get(const struct entry *pentry, bool *value);
bool entry_bool_set(struct entry *pentry, bool value);

bool entry_str_get(const struct entry *pentry, const char **value);
bool entry_str_set(struct entry *pentry, const char *value);
bool entry_str_escaped(const struct entry *pentry);
bool entry_str_set_escaped(struct entry *pentry, bool escaped);

#endif  /* FC__REGISTRY_H */
