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
#ifndef FC__DATAIO_H
#define FC__DATAIO_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "bitvector.h"
#include "support.h"            /* bool type */

struct worklist;
struct requirement;

struct data_in {
  const void *src;
  size_t src_size, current;
};

struct data_out {
  void *dest;
  size_t dest_size, used, current;
  bool too_short;		/* set to 1 if try to read past end */
};

/* Used for dio_<put|get>_type() methods.
 * NB: we only support integer handling currently. */
enum data_type {
  DIOT_UINT8,
  DIOT_UINT16,
  DIOT_UINT32,
  DIOT_SINT8,
  DIOT_SINT16,
  DIOT_SINT32,

  DIOT_LAST
};

/* network string conversion */
typedef char *(*DIO_PUT_CONV_FUN) (const char *src, size_t *length);
void dio_set_put_conv_callback(DIO_PUT_CONV_FUN fun);

typedef bool(*DIO_GET_CONV_FUN) (char *dst, size_t ndst,
				 const char *src, size_t nsrc);
void dio_set_get_conv_callback(DIO_GET_CONV_FUN fun);

/* General functions */
void dio_output_init(struct data_out *dout, void *destination,
		     size_t dest_size);
void dio_output_rewind(struct data_out *dout);
size_t dio_output_used(struct data_out *dout);

void dio_input_init(struct data_in *dout, const void *src, size_t src_size);
void dio_input_rewind(struct data_in *din);
size_t dio_input_remaining(struct data_in *din);
bool dio_input_skip(struct data_in *din, size_t size);

size_t data_type_size(enum data_type type);

/* gets */
bool dio_get_type(struct data_in *din, enum data_type type, int *dest)
    fc__attribute((nonnull (3)));

bool dio_get_uint8(struct data_in *din, int *dest)
    fc__attribute((nonnull (2)));
bool dio_get_uint16(struct data_in *din, int *dest)
    fc__attribute((nonnull (2)));
bool dio_get_uint32(struct data_in *din, int *dest)
    fc__attribute((nonnull (2)));

bool dio_get_sint8(struct data_in *din, int *dest)
    fc__attribute((nonnull (2)));
bool dio_get_sint16(struct data_in *din, int *dest)
    fc__attribute((nonnull (2)));
bool dio_get_sint32(struct data_in *din, int *dest)
    fc__attribute((nonnull (2)));

bool dio_get_bool8(struct data_in *din, bool *dest)
    fc__attribute((nonnull (2)));
bool dio_get_bool32(struct data_in *din, bool *dest)
    fc__attribute((nonnull (2)));
bool dio_get_ufloat(struct data_in *din, float *dest, int float_factor)
    fc__attribute((nonnull (2)));
bool dio_get_sfloat(struct data_in *din, float *dest, int float_factor)
    fc__attribute((nonnull (2)));
bool dio_get_memory(struct data_in *din, void *dest, size_t dest_size)
    fc__attribute((nonnull (2)));
bool dio_get_string(struct data_in *din, char *dest, size_t max_dest_size)
    fc__attribute((nonnull (2)));
bool dio_get_tech_list(struct data_in *din, int *dest)
    fc__attribute((nonnull (2)));
bool dio_get_unit_list(struct data_in *din, int *dest)
    fc__attribute((nonnull (2)));
bool dio_get_building_list(struct data_in *din, int *dest)
    fc__attribute((nonnull (2)));
bool dio_get_worklist(struct data_in *din, struct worklist *pwl)
    fc__attribute((nonnull (2)));
bool dio_get_requirement(struct data_in *din, struct requirement *preq)
    fc__attribute((nonnull (2)));

bool dio_get_uint8_vec8(struct data_in *din, int **values, int stop_value)
    fc__attribute((nonnull (2)));
bool dio_get_uint16_vec8(struct data_in *din, int **values, int stop_value)
    fc__attribute((nonnull (2)));

/* Should be a function but we need some macro magic. */
#define DIO_BV_GET(pdin, bv) \
  dio_get_memory((pdin), (bv).vec, sizeof((bv).vec))

/* puts */
void dio_put_type(struct data_out *dout, enum data_type type, int value);

void dio_put_uint8(struct data_out *dout, int value);
void dio_put_uint16(struct data_out *dout, int value);
void dio_put_uint32(struct data_out *dout, int value);

void dio_put_sint8(struct data_out *dout, int value);
void dio_put_sint16(struct data_out *dout, int value);
void dio_put_sint32(struct data_out *dout, int value);

void dio_put_bool8(struct data_out *dout, bool value);
void dio_put_bool32(struct data_out *dout, bool value);
void dio_put_ufloat(struct data_out *dout, float value, int float_factor);
void dio_put_sfloat(struct data_out *dout, float value, int float_factor);

void dio_put_memory(struct data_out *dout, const void *value, size_t size);
void dio_put_string(struct data_out *dout, const char *value);
void dio_put_city_map(struct data_out *dout, const char *value);
void dio_put_tech_list(struct data_out *dout, const int *value);
void dio_put_unit_list(struct data_out *dout, const int *value);
void dio_put_building_list(struct data_out *dout, const int *value);
void dio_put_worklist(struct data_out *dout, const struct worklist *pwl);
void dio_put_requirement(struct data_out *dout, const struct requirement *preq);

void dio_put_uint8_vec8(struct data_out *dout, int *values, int stop_value);
void dio_put_uint16_vec8(struct data_out *dout, int *values, int stop_value);

/* Should be a function but we need some macro magic. */
#define DIO_BV_PUT(pdout, bv) \
  dio_put_memory((pdout), (bv).vec, sizeof((bv).vec))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* FC__DATAIO_H */
