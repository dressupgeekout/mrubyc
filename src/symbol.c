/*! @file
  @brief
  mruby/c Symbol class

  <pre>
  Copyright (C) 2015-2020 Kyushu Institute of Technology.
  Copyright (C) 2015-2020 Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.

  </pre>
*/

/***** Feature test switches ************************************************/
/***** System headers *******************************************************/
#include "vm_config.h"
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

/***** Local headers ********************************************************/
#include "value.h"
#include "vm.h"
#include "alloc.h"
#include "class.h"
#include "symbol.h"
#include "c_object.h"
#include "c_string.h"
#include "c_array.h"
#include "console.h"

/***** Constant values ******************************************************/
#if !defined(MRBC_SYMBOL_SEARCH_LINER) && !defined(MRBC_SYMBOL_SEARCH_BTREE)
#define MRBC_SYMBOL_SEARCH_BTREE
#endif

#if MAX_SYMBOLS_COUNT <= UCHAR_MAX
#define MRBC_SYMBOL_TABLE_INDEX_TYPE	uint8_t
#else
#define MRBC_SYMBOL_TABLE_INDEX_TYPE	uint16_t
#endif


/***** Macros ***************************************************************/
/***** Typedefs *************************************************************/
/***** Function prototypes **************************************************/
/***** Local variables ******************************************************/
struct SYM_INDEX {
  uint16_t hash;	//!< hash value, returned by calc_hash().
#ifdef MRBC_SYMBOL_SEARCH_BTREE
  MRBC_SYMBOL_TABLE_INDEX_TYPE left;
  MRBC_SYMBOL_TABLE_INDEX_TYPE right;
#endif
  const char *cstr;	//!< point to the symbol string.
};

static struct SYM_INDEX sym_index[MAX_SYMBOLS_COUNT];
static int sym_index_pos;	// point to the last(free) sym_index array.


/***** Global variables *****************************************************/
/***** Signal catching functions ********************************************/
/***** Local functions ******************************************************/

//================================================================
/*! Calculate hash value.

  @param  str		Target string.
  @return uint16_t	Hash value.
*/
static inline uint16_t calc_hash(const char *str)
{
  uint16_t h = 0;

  while( *str != '\0' ) {
    h = h * 17 + *str++;
  }
  return h;
}


//================================================================
/*! search index table

  @param  hash	hash value.
  @param  str	string ptr.
  @return	symbol id. or -1 if not registered.
*/
static int search_index( uint16_t hash, const char *str )
{
#ifdef MRBC_SYMBOL_SEARCH_LINER
  int i;
  for( i = 0; i < sym_index_pos; i++ ) {
    if( sym_index[i].hash == hash && strcmp(str, sym_index[i].cstr) == 0 ) {
      return i;
    }
  }
  return -1;
#endif

#ifdef MRBC_SYMBOL_SEARCH_BTREE
  int i = 0;
  do {
    if( sym_index[i].hash == hash && strcmp(str, sym_index[i].cstr) == 0 ) {
      return i;
    }
    if( hash < sym_index[i].hash ) {
      i = sym_index[i].left;
    } else {
      i = sym_index[i].right;
    }
  } while( i != 0 );
  return -1;
#endif
}


//================================================================
/*! add to index table

  @param  hash	return value.
  @param  str	string ptr.
  @return	symbol id. or -1 if error.
*/
static int add_index( uint16_t hash, const char *str )
{
  // check overflow.
  if( sym_index_pos >= MAX_SYMBOLS_COUNT ) {
    console_printf( "Overflow MAX_SYMBOLS_COUNT for '%s'\n", str );
    return -1;
  }

  int sym_id = sym_index_pos++;

  // append table.
  sym_index[sym_id].hash = hash;
  sym_index[sym_id].cstr = str;

#ifdef MRBC_SYMBOL_SEARCH_BTREE
  int i = 0;

  while( 1 ) {
    if( hash < sym_index[i].hash ) {
      // left side
      if( sym_index[i].left == 0 ) {	// left is empty?
        sym_index[i].left = sym_id;
        break;
      }
      i = sym_index[i].left;
    } else {
      // right side
      if( sym_index[i].right == 0 ) {	// right is empty?
        sym_index[i].right = sym_id;
        break;
      }
      i = sym_index[i].right;
    }
  }
#endif

  return sym_id;
}


/***** Global functions *****************************************************/

//================================================================
/*! cleanup
*/
void mrbc_symbol_cleanup(void)
{
  memset(sym_index, 0, sizeof(sym_index));
  sym_index_pos = 0;
}



//================================================================
/*! Convert string to symbol value.

  @param  str		Target string.
  @return mrbc_sym	Symbol value.
*/
mrbc_sym mrbc_str_to_symid(const char *str)
{
  uint16_t h = calc_hash(str);
  mrbc_sym sym_id = search_index(h, str);
  if( sym_id >= 0 ) return sym_id;

  return add_index( h, str );
}


//================================================================
/*! Convert symbol value to string.

  @param  sym_id	Symbol value.
  @return const char*	String.
  @retval NULL		Invalid sym_id was given.
*/
const char * mrbc_symid_to_str(mrbc_sym sym_id)
{
  if( sym_id < 0 ) return NULL;
  if( sym_id >= sym_index_pos ) return NULL;

  return sym_index[sym_id].cstr;
}



//================================================================
/*! Search only.

  @param  str	C string.
  @return	symbol id. or -1 if not registered.
*/
mrbc_sym mrbc_search_symid( const char *str )
{
  uint16_t h = calc_hash(str);
  return search_index(h, str);
}


//================================================================
/*! constructor

  @param  vm	pointer to VM.
  @param  str	String
  @return 	symbol object
*/
mrbc_value mrbc_symbol_new(struct VM *vm, const char *str)
{
  uint16_t h = calc_hash(str);
  mrbc_sym sym_id = search_index(h, str);

  if( sym_id >= 0 ) goto DONE;	// already exist.

  // create symbol object dynamically.
  int size = strlen(str) + 1;
  char *buf = mrbc_raw_alloc_no_free(size);
  if( buf == NULL ) return mrbc_nil_value();	// ENOMEM raise?

  memcpy(buf, str, size);
  sym_id = add_index( h, buf );

 DONE:
  return mrbc_symbol_value( sym_id );
}


/***** mruby/c methods ******************************************************/

//================================================================
/*! (method) all_symbols
*/
static void c_all_symbols(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_value ret = mrbc_array_new(vm, sym_index_pos);

  int i;
  for( i = 0; i < sym_index_pos; i++ ) {
    mrbc_value sym1 = {.tt = MRBC_TT_SYMBOL};
    sym1.i = i;
    mrbc_array_push(&ret, &sym1);
  }
  SET_RETURN(ret);
}


#if MRBC_USE_STRING
//================================================================
/*! (method) inspect
*/
static void c_inspect(struct VM *vm, mrbc_value v[], int argc)
{
  const char *s = mrbc_symid_to_str( mrbc_symbol(v[0]) );
  v[0] = mrbc_string_new_cstr(vm, ":");
  mrbc_string_append_cstr(&v[0], s);
}


//================================================================
/*! (method) to_s
*/
static void c_to_s(struct VM *vm, mrbc_value v[], int argc)
{
  v[0] = mrbc_string_new_cstr(vm, mrbc_symid_to_str( mrbc_symbol(v[0]) ));
}
#endif



//================================================================
/*! initialize
*/
void mrbc_init_class_symbol(struct VM *vm)
{
  mrbc_class_symbol = mrbc_define_class(vm, "Symbol", mrbc_class_object);

  mrbc_define_method(vm, mrbc_class_symbol, "all_symbols", c_all_symbols);
#if MRBC_USE_STRING
  mrbc_define_method(vm, mrbc_class_symbol, "inspect", c_inspect);
  mrbc_define_method(vm, mrbc_class_symbol, "to_s", c_to_s);
  mrbc_define_method(vm, mrbc_class_symbol, "id2name", c_to_s);
#endif
  mrbc_define_method(vm, mrbc_class_symbol, "to_sym", c_ineffect);
}



#if defined(MRBC_DEBUG)
//================================================================
/* statistics

   (e.g.)
   total = MAX_SYMBOLS_COUNT;
   mrbc_symbol_statistics( &used );
   console_printf("Symbol table: %d/%d %d%% used.\n",
                   used, total, 100 * used / total );
*/
void mrbc_symbol_statistics( int *total_used )
{
  *total_used = sym_index_pos;
}
#endif
