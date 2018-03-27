/*! @file
  @brief
  mruby/c String object

  <pre>
  Copyright (C) 2015-2018 Kyushu Institute of Technology.
  Copyright (C) 2015-2018 Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.

  </pre>
*/

#include "vm_config.h"
#include <stdlib.h>
#include <string.h>

#include "value.h"
#include "alloc.h"
#include "static.h"
#include "class.h"
#include "c_string.h"
#include "console.h"


#if MRBC_USE_STRING
//================================================================
/*! constructor

  @param  vm	pointer to VM.
  @param  src	source string or NULL
  @param  len	source length
  @return 	string object
*/
mrb_value mrbc_string_new(struct VM *vm, const void *src, int len)
{
  mrb_value value = {.tt = MRB_TT_STRING};

  /*
    Allocate handle and string buffer.
  */
  mrb_string *h;
  h = (mrb_string *)mrbc_alloc(vm, sizeof(mrb_string));
  if( !h ) return value;		// ENOMEM

  uint8_t *str = mrbc_alloc(vm, len+1);
  if( !str ) {				// ENOMEM
    mrbc_raw_free( h );
    return value;
  }

  h->ref_count = 1;
  h->tt = MRB_TT_STRING;	// TODO: for DEBUG
  h->size = len;
  h->data = str;

  /*
    Copy a source string.
  */
  if( src == NULL ) {
    str[0] = '\0';
  } else {
    memcpy( str, src, len );
    str[len] = '\0';
  }

  value.string = h;
  return value;
}


//================================================================
/*! constructor by c string

  @param  vm	pointer to VM.
  @param  src	source string or NULL
  @return 	string object
*/
mrb_value mrbc_string_new_cstr(struct VM *vm, const char *src)
{
  return mrbc_string_new(vm, src, (src ? strlen(src) : 0));
}


//================================================================
/*! constructor by allocated buffer

  @param  vm	pointer to VM.
  @param  buf	pointer to allocated buffer
  @param  len	buffer length
  @return 	string object
*/
mrb_value mrbc_string_new_alloc(struct VM *vm, void *buf, int len)
{
  mrb_value value = {.tt = MRB_TT_STRING};

  /*
    Allocate handle
  */
  mrb_string *h;
  h = (mrb_string *)mrbc_alloc(vm, sizeof(mrb_string));
  if( !h ) return value;		// ENOMEM

  h->ref_count = 1;
  h->tt = MRB_TT_STRING;	// TODO: for DEBUG
  h->size = len;
  h->data = buf;

  value.string = h;
  return value;
}


//================================================================
/*! destructor

  @param  str	pointer to target value
*/
void mrbc_string_delete(mrb_value *str)
{
  mrbc_raw_free(str->string->data);
  mrbc_raw_free(str->string);
}



//================================================================
/*! clear vm_id
*/
void mrbc_string_clear_vm_id(mrb_value *str)
{
  mrbc_set_vm_id( str->string, 0 );
  mrbc_set_vm_id( str->string->data, 0 );
}


//================================================================
/*! add string (s1 + s2)

  @param  vm	pointer to VM.
  @param  s1	pointer to target value 1
  @param  s2	pointer to target value 2
  @return	new string as s1 + s2
*/
mrb_value mrbc_string_add(struct VM *vm, mrb_value *s1, mrb_value *s2)
{
  mrb_string *h1 = s1->string;
  mrb_string *h2 = s2->string;

  mrb_value value = mrbc_string_new(vm, NULL, h1->size + h2->size);
  if( value.string == NULL ) return value;		// ENOMEM

  memcpy( value.string->data,            h1->data, h1->size );
  memcpy( value.string->data + h1->size, h2->data, h2->size + 1 );

  return value;
}


//================================================================
/*! append string (s1 += s2)

  @param  vm	pointer to VM.
  @param  s1	pointer to target value 1
  @param  s2	pointer to target value 2
  @param	mrb_error_code
*/
int mrbc_string_append(struct VM *vm, mrb_value *s1, mrb_value *s2)
{
  int len1 = s1->string->size;
  int len2 = (s2->tt == MRB_TT_STRING) ? s2->string->size : 1;

  uint8_t *str = mrbc_realloc(vm, s1->string->data, len1+len2+1);
  if( !str ) return E_NOMEMORY_ERROR;

  if( s2->tt == MRB_TT_STRING ) {
    memcpy(str + len1, s2->string->data, len2 + 1);
  } else if( s2->tt == MRB_TT_FIXNUM ) {
    str[len1] = s2->i;
    str[len1+1] = '\0';
  }

  s1->string->size = len1 + len2;
  s1->string->data = str;

  return 0;
}


//================================================================
/*! (method) +
*/
static void c_string_add(mrb_vm *vm, mrb_value v[], int argc)
{
  if( GET_TT_ARG(1) != MRB_TT_STRING ) {
    console_print( "Not support STRING + Other\n" );
    return;
  }

  mrb_value value = mrbc_string_add(vm, &v[0], &v[1]);
  mrbc_release(v);
  SET_RETURN(value);
}



//================================================================
/*! (method) ===
*/
static void c_string_eql(mrb_vm *vm, mrb_value v[], int argc)
{
  int result = 0;
  if( GET_TT_ARG(1) != MRB_TT_STRING ) goto DONE;

  mrb_string *h1 = GET_ARG(0).string;
  mrb_string *h2 = GET_ARG(1).string;

  if( h1->size != h2->size ) goto DONE;	// false
  result = !memcmp(h1->data, h2->data, h1->size);

 DONE:
  mrbc_release(v);
  if( result ) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}



//================================================================
/*! (method) size, length
*/
static void c_string_size(mrb_vm *vm, mrb_value v[], int argc)
{
  int32_t size = mrbc_string_size(&v[0]);

  mrbc_release(v);
  SET_INT_RETURN( size );
}



//================================================================
/*! (method) to_i
*/
static void c_string_to_i(mrb_vm *vm, mrb_value v[], int argc)
{
  int base = 10;
  if( argc ) {
    base = GET_INT_ARG(1);
    if( base < 2 || base > 36 ) {
      return;	// raise ? ArgumentError
    }
  }

  int32_t i = mrbc_atoi( mrbc_string_cstr(v), base );

  mrbc_release(v);
  SET_INT_RETURN( i );
}


#if MRBC_USE_FLOAT
//================================================================
/*! (method) to_f
*/
static void c_string_to_f(mrb_vm *vm, mrb_value v[], int argc)
{
  double d = atof(mrbc_string_cstr(v));

  mrbc_release(v);
  SET_FLOAT_RETURN( d );
}
#endif


//================================================================
/*! (method) <<
*/
static void c_string_append(mrb_vm *vm, mrb_value v[], int argc)
{
  if( !mrbc_string_append( vm, &v[0], &v[1] ) ) {
    // raise ? ENOMEM
  }
}


//================================================================
/*! (method) []
*/
static void c_string_slice(mrb_vm *vm, mrb_value v[], int argc)
{
  mrb_value *v1 = &GET_ARG(1);
  mrb_value *v2 = &GET_ARG(2);

  /*
    in case of slice(nth) -> String | nil
  */
  if( argc == 1 && v1->tt == MRB_TT_FIXNUM ) {
    int len = v->string->size;
    int idx = v1->i;
    int ch = -1;
    if( idx >= 0 ) {
      if( idx < len ) {
        ch = *(v->string->data + idx);
      }
    } else {
      idx += len;
      if( idx >= 0 ) {
        ch = *(v->string->data + idx);
      }
    }
    if( ch < 0 ) goto RETURN_NIL;

    mrb_value value = mrbc_string_new(vm, NULL, 1);
    if( !value.string ) goto RETURN_NIL;		// ENOMEM

    value.string->data[0] = ch;
    value.string->data[1] = '\0';
    mrbc_release(v);
    SET_RETURN(value);
    return;		// normal return
  }

  /*
    in case of slice(nth, len) -> String | nil
  */
  if( argc == 2 && v1->tt == MRB_TT_FIXNUM && v2->tt == MRB_TT_FIXNUM ) {
    int len = v->string->size;
    int idx = v1->i;
    if( idx < 0 ) idx += len;
    if( idx < 0 ) goto RETURN_NIL;

    int rlen = (v2->i < (len - idx)) ? v2->i : (len - idx);
						// min( v2->i, (len-idx) )
    if( rlen < 0 ) goto RETURN_NIL;

    mrb_value value = mrbc_string_new(vm, v->string->data + idx, rlen);
    if( !value.string ) goto RETURN_NIL;		// ENOMEM

    mrbc_release(v);
    SET_RETURN(value);
    return;		// normal return
  }

  /*
    other case
  */
  console_print( "Not support such case in String#[].\n" );
  return;


 RETURN_NIL:
  mrbc_release(v);
  SET_NIL_RETURN();
}


//================================================================
/*! (method) []=
*/
static void c_string_insert(mrb_vm *vm, mrb_value v[], int argc)
{
  int nth;
  int len;
  mrb_value *val;

  /*
    in case of self[nth] = val
  */
  if( argc == 2 &&
      GET_TT_ARG(1) == MRB_TT_FIXNUM &&
      GET_TT_ARG(2) == MRB_TT_STRING ) {
    nth = GET_INT_ARG(1);
    len = 1;
    val = &GET_ARG(2);
  }
  /*
    in case of self[nth, len] = val
  */
  else if( argc == 3 &&
	   GET_TT_ARG(1) == MRB_TT_FIXNUM &&
	   GET_TT_ARG(2) == MRB_TT_FIXNUM &&
	   GET_TT_ARG(3) == MRB_TT_STRING ) {
    nth = GET_INT_ARG(1);
    len = GET_INT_ARG(2);
    val = &GET_ARG(3);
  }
  /*
    other cases
  */
  else {
    console_print( "Not support\n" );
    return;
  }

  int len1 = v->string->size;
  int len2 = val->string->size;
  if( nth < 0 ) nth = len1 + nth;               // adjust to positive number.
  if( len > len1 - nth ) len = len1 - nth;
  if( nth < 0 || nth > len1 || len < 0) {
    console_print( "IndexError\n" );  // raise?
    return;
  }

  uint8_t *str = mrbc_realloc(vm, mrbc_string_cstr(v), len1 + len2 - len + 1);
  if( !str ) return;

  memmove( str + nth + len2, str + nth + len, len1 - nth - len + 1 );
  memcpy( str + nth, mrbc_string_cstr(val), len2 );
  v->string->size = len1 + len2 - len;

  v->string->data = str;
}


//================================================================
/*! (method) ord
*/
static void c_string_ord(mrb_vm *vm, mrb_value v[], int argc)
{
  int i = mrbc_string_cstr(v)[0];

  mrbc_release(v);
  SET_INT_RETURN( i );
}



//================================================================
/*! (method) sprintf
*/
static void c_sprintf(mrb_vm *vm, mrb_value v[], int argc)
{
  static const int BUF_INC_STEP = 32;	// bytes.

  mrb_value *format = &GET_ARG(1);
  if( format->tt != MRB_TT_STRING ) {
    console_printf( "TypeError\n" );	// raise?
    return;
  }

  int buflen = BUF_INC_STEP;
  char *buf = mrbc_alloc(vm, buflen);
  if( !buf ) { return; }	// ENOMEM raise?

  MrbcPrintf pf;
  mrbc_printf_init( &pf, buf, buflen, mrbc_string_cstr(format) );

  int i = 2;
  int ret;
  while( 1 ) {
    MrbcPrintf pf_bak = pf;
    ret = mrbc_printf_main( &pf );
    if( ret == 0 ) break;	// normal break loop.
    if( ret < 0 ) goto INCREASE_BUFFER;

    if( i > argc ) {console_print("ArgumentError\n"); break;}	// raise?

    // maybe ret == 1
    switch(pf.fmt.type) {
    case 'c':
      if( GET_ARG(i).tt == MRB_TT_FIXNUM ) {
	ret = mrbc_printf_char( &pf, GET_ARG(i).i );
      }
      break;

    case 's':
      if( GET_ARG(i).tt == MRB_TT_STRING ) {
	ret = mrbc_printf_str( &pf, mrbc_string_cstr( &GET_ARG(i) ), ' ');
      }
      break;

    case 'd':
    case 'i':
    case 'u':
      if( GET_ARG(i).tt == MRB_TT_FIXNUM ) {
	ret = mrbc_printf_int( &pf, GET_ARG(i).i, 10);
      } else
	if( GET_ARG(i).tt == MRB_TT_FLOAT ) {
	  ret = mrbc_printf_int( &pf, (int32_t)GET_ARG(i).d, 10);
	} else
	  if( GET_ARG(i).tt == MRB_TT_STRING ) {
	    int32_t ival = atol(mrbc_string_cstr(&GET_ARG(i)));
	    ret = mrbc_printf_int( &pf, ival, 10 );
	  }
      break;

    case 'b':
    case 'B':
      if( GET_ARG(i).tt == MRB_TT_FIXNUM ) {
	ret = mrbc_printf_int( &pf, GET_ARG(i).i, 2);
      }
      break;

    case 'x':
    case 'X':
      if( GET_ARG(i).tt == MRB_TT_FIXNUM ) {
	ret = mrbc_printf_int( &pf, GET_ARG(i).i, 16);
      }
      break;

#if MRBC_USE_FLOAT
    case 'f':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
      if( GET_ARG(i).tt == MRB_TT_FLOAT ) {
	ret = mrbc_printf_float( &pf, GET_ARG(i).d );
      } else
	if( GET_ARG(i).tt == MRB_TT_FIXNUM ) {
	  ret = mrbc_printf_float( &pf, (double)GET_ARG(i).i );
	}
      break;
#endif

    default:
      break;
    }
    if( ret >= 0 ) {
      i++;
      continue;		// normal next loop.
    }

    // maybe buffer full. (ret == -1)
    if( pf.fmt.width > BUF_INC_STEP ) buflen += pf.fmt.width;
    pf = pf_bak;

  INCREASE_BUFFER:
    buflen += BUF_INC_STEP;
    buf = mrbc_realloc(vm, pf.buf, buflen);
    if( !buf ) { return; }	// ENOMEM raise? TODO: leak memory.
    mrbc_printf_replace_buffer(&pf, buf, buflen);
  }
  mrbc_printf_end( &pf );

  buflen = mrbc_printf_len( &pf ) + 1;
  mrbc_realloc(vm, pf.buf, buflen);

  mrb_value value = mrbc_string_new_alloc( vm, pf.buf, buflen );

  mrbc_release(v);
  SET_RETURN(value);
}



//================================================================
/*! initialize
*/
void mrbc_init_class_string(mrb_vm *vm)
{
  mrbc_class_string = mrbc_define_class(vm, "String", mrbc_class_object);

  mrbc_define_method(vm, mrbc_class_string, "+",	c_string_add);
  mrbc_define_method(vm, mrbc_class_string, "===",	c_string_eql);
  mrbc_define_method(vm, mrbc_class_string, "size",	c_string_size);
  mrbc_define_method(vm, mrbc_class_string, "length",	c_string_size);
  mrbc_define_method(vm, mrbc_class_string, "to_i",	c_string_to_i);
  mrbc_define_method(vm, mrbc_class_string, "to_s",	c_ineffect);
  mrbc_define_method(vm, mrbc_class_string, "<<",	c_string_append);
  mrbc_define_method(vm, mrbc_class_string, "[]",	c_string_slice);
  mrbc_define_method(vm, mrbc_class_string, "[]=",	c_string_insert);
  mrbc_define_method(vm, mrbc_class_string, "ord",	c_string_ord);
  mrbc_define_method(vm, mrbc_class_object, "sprintf",	c_sprintf);
#if MRBC_USE_FLOAT
  mrbc_define_method(vm, mrbc_class_string, "to_f",	c_string_to_f);
#endif
}


#endif // MRBC_USE_STRING
