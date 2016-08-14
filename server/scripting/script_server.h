/*****************************************************************************
 Freeciv - Copyright (C) 2005 - The Freeciv Project
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*****************************************************************************/

#ifndef FC__SCRIPT_SERVER_H
#define FC__SCRIPT_SERVER_H

/* utility */
#include "support.h"

/* common/scriptcore */
#include "luascript_types.h"

struct trigger_signal;

/* A trigger_signal_list is a list of trigger_signals. */
#define SPECLIST_TAG trigger_signal
#define SPECLIST_TYPE struct trigger_signal
#include "speclist.h"
#define trigger_signal_list_iterate(trigger_signal_list, ptrigger) \
  TYPED_LIST_ITERATE(struct trigger_signal, trigger_signal_list, ptrigger)
#define trigger_signal_list_iterate_end LIST_ITERATE_END

struct section_file;
struct connection;

/* Callback invocation function. */
bool script_server_callback_invoke(const char *callback_name, int nargs,
                                   enum api_types *parg_types, va_list args);

void script_server_remove_exported_object(void *object);

/* Script functions. */
bool script_server_init(void);
void script_server_free(void);
bool script_server_do_string(struct connection *caller, const char *str);
bool script_server_do_file(struct connection *caller, const char *filename);

/* Script state i/o. */
void script_server_state_load(struct section_file *file);
void script_server_state_save(struct section_file *file);

/* Signals. */
void script_server_signal_emit(const char *signal_name, int nargs, ...);
void script_server_trigger_emit(const char *signal_name, int nargs, void * args[]);
void script_server_trigger_signal_create(const char *signal_name,
                             int nargs, enum api_types args[]);
void script_server_trigger_signals_destroy();

/* Functions */
bool script_server_call(const char *func_name, int nargs, ...);

#endif /* FC__SCRIPT_SERVER_H */

