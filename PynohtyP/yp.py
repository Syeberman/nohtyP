"""
yp.py - Python wrapper for nohtyP
    http://nohtyp.wordpress.com
    Copyright © 2001-2012 Python Software Foundation; All Rights Reserved
    License: http://docs.python.org/py3k/license.html
"""

from ctypes import *

ypdll = cdll.nohtyP

# From the ctypes documentation...
c_IN = 1
c_OUT = 2
c_INOUT = 3

class yp_param_ret:
    """Describes one parameter or return value from a function."""
    # type = the c_type
    direction = c_IN # ignored on return values
    # default = default value to use
    def __init__( self, name ):
        self.name = name

# Some standard C param/return types
class void( yp_param_ret ):
    type = None
    def __init__( self, name ): raise NotImplementedError( "cannot instantiate void" )
class int( yp_param_ret ):
    type = c_int
    default = 0

def yp_func_errcheck( result, func, args ):
    if hasattr( result, "errcheck" ): result.errcheck( )
    for arg in args: 
        if hasattr( arg, "errcheck" ): arg.errcheck( )
    return args

def yp_func( retval, name, args ):
    """Defines a function in globals() that wraps the given C yp_* function."""
    proto = CFUNCTYPE( retval.type, *(x.type for x in args) )
    pflags = tuple( (x.direction, x.name, x.default) for x in args )
    func = proto( (name, ypdll), pflags )
    func.errcheck = yp_func_errcheck
    globals( )[name] = func

# ypAPI void yp_initialize( void );
yp_func( void, "yp_initialize", () )

# typedef struct _ypObject ypObject;
# XXX other idea: _as_parameter_
class c_ypObject_p( c_void_p ):
    # TODO @classmethod def from_param( cls, val ): ...
    def errcheck( self ): ypObject_p_errcheck( self ) 
class ypObject_p( yp_param_ret ):
    type = c_ypObject_p
    # default set below

def ypObject_p_value( name ):
    globals( )[name] = ypObject_p.type.in_dll( ypdll, name )

# ypAPI ypObject *yp_None;
ypObject_p_value( "yp_None" )
ypObject_p.default = yp_None

# ypAPI ypObject *yp_incref( ypObject *x );
yp_func( ypObject_p, "yp_incref", (ypObject_p( "x" ), ) )

# void yp_increfN( int n, ... );
# void yp_increfV( int n, va_list args );

# void yp_decref( ypObject *x );
yp_func( None, "yp_decref", (ypObject_p( "x" ), ) )

# void yp_decrefN( int n, ... );
# void yp_decrefV( int n, va_list args );

# int yp_isexceptionC( ypObject *x );
yp_func( int, "yp_isexceptionC", (ypObject_p( "x" ), ) )


# void yp_freeze( ypObject **x );

# void yp_deepfreeze( ypObject **x );

# ypObject *yp_unfrozen_copy( ypObject *x );

# ypObject *yp_unfrozen_deepcopy( ypObject *x );

# ypObject *yp_frozen_copy( ypObject *x );

# ypObject *yp_frozen_deepcopy( ypObject *x );

# ypObject *yp_copy( ypObject *x );

# ypObject *yp_deepcopy( ypObject *x );

# void yp_invalidate( ypObject **x );

# void yp_deepinvalidate( ypObject **x );


# ypObject *yp_True;
ypObject_p_value( "yp_True" )
# ypObject *yp_False;
ypObject_p_value( "yp_False" )

# ypObject *yp_bool( ypObject *x );

# ypObject *yp_not( ypObject *x );

# ypObject *yp_or( ypObject *x, ypObject *y );

# ypObject *yp_orN( int n, ... );
# ypObject *yp_orV( int n, va_list args );

# ypObject *yp_anyN( int n, ... );
# ypObject *yp_anyV( int n, va_list args );

# ypObject *yp_any( ypObject *iterable );

# ypObject *yp_and( ypObject *x, ypObject *y );

# ypObject *yp_andN( int n, ... );
# ypObject *yp_andV( int n, va_list args );

# ypObject *yp_allN( int n, ... );
# ypObject *yp_allV( int n, va_list args );

# ypObject *yp_all( ypObject *iterable );

# ypObject *yp_lt( ypObject *x, ypObject *y );
# ypObject *yp_le( ypObject *x, ypObject *y );
# ypObject *yp_eq( ypObject *x, ypObject *y );
# ypObject *yp_ne( ypObject *x, ypObject *y );
# ypObject *yp_ge( ypObject *x, ypObject *y );
# ypObject *yp_gt( ypObject *x, ypObject *y );

#typedef float               yp_float32_t;
class yp_float32_t( yp_param_ret ):
    type = c_float
#typedef double              yp_float64_t;
class yp_float64_t( yp_param_ret ):
    type = c_double
#if SIZE_MAX == 0xFFFFFFFFu
#typedef yp_int32_t          yp_ssize_t;
#else
#typedef yp_int64_t          yp_ssize_t;
#endif
class yp_ssize_t( yp_param_ret ):
    type = c_ssize_t
#typedef yp_ssize_t          yp_hash_t;
class yp_hash_t( yp_param_ret ):
    type = yp_ssize_t.type

# typedef yp_int64_t      yp_int_t;
class yp_int_t( yp_param_ret ):
    type = c_int64
# typedef yp_float64_t    yp_float_t;
class yp_float_t( yp_param_ret ):
    type = yp_float64_t.type

# typedef ypObject *(*yp_generator_func_t)( ypObject *self, ypObject *value );


# ypObject *yp_intC( yp_int_t value );
yp_func( ypObject_p, "yp_intC", (yp_int_t( "value" ), ) )
# ypObject *yp_intstoreC( yp_int_t value );

# ypObject *yp_int_strC( const char *string, int base );
# ypObject *yp_intstore_strC( const char *string, int base );

# ypObject *yp_floatC( yp_float_t value );
# ypObject *yp_floatstoreC( yp_float_t value );

# ypObject *yp_float_strC( const char *string );
# ypObject *yp_floatstore_strC( const char *string );

# ypObject *yp_iter( ypObject *x );

# ypObject *yp_generatorCN( yp_generator_func_t func, yp_ssize_t lenhint, int n, ... );
# ypObject *yp_generatorCV( yp_generator_func_t func, yp_ssize_t lenhint, int n, va_list args );

# ypObject *yp_generator_fromstructCN( yp_generator_func_t func, yp_ssize_t lenhint,
#         void *state, yp_ssize_t size, int n, ... );
# ypObject *yp_generator_fromstructCV( yp_generator_func_t func, yp_ssize_t lenhint,
#         void *state, yp_ssize_t size, int n, va_list args );

# ypObject *yp_rangeC3( yp_int_t start, yp_int_t stop, yp_int_t step );
# ypObject *yp_rangeC( yp_int_t stop );

# ypObject *yp_bytesC( const yp_uint8_t *source, yp_ssize_t len );
# ypObject *yp_bytearrayC( const yp_uint8_t *source, yp_ssize_t len );

# XXX The str/characterarray types will be added in a future version

# ypObject *yp_chrC( yp_int_t i );

# ypObject *yp_tupleN( int n, ... );
# ypObject *yp_tupleV( int n, va_list args );
# ypObject *yp_listN( int n, ... );
# ypObject *yp_listV( int n, va_list args );

# ypObject *yp_tuple_repeatCN( yp_ssize_t factor, int n, ... );
# ypObject *yp_tuple_repeatCV( yp_ssize_t factor, int n, va_list args );
# ypObject *yp_list_repeatCN( yp_ssize_t factor, int n, ... );
# ypObject *yp_list_repeatCV( yp_ssize_t factor, int n, va_list args );

# ypObject *yp_tuple( ypObject *iterable );
# ypObject *yp_list( ypObject *iterable );

# typedef ypObject *(*yp_sort_key_func_t)( ypObject *x );
# ypObject *yp_sorted3( ypObject *iterable, yp_sort_key_func_t key, ypObject *reverse );

# ypObject *yp_sorted( ypObject *iterable );

# ypObject *yp_frozensetN( int n, ... );
# ypObject *yp_frozensetV( int n, va_list args );
# ypObject *yp_setN( int n, ... );
# ypObject *yp_setV( int n, va_list args );

# ypObject *yp_frozenset( ypObject *iterable );
# ypObject *yp_set( ypObject *iterable );

# ypObject *yp_frozendictK( int n, ... );
# ypObject *yp_frozendictKV( int n, va_list args );
# ypObject *yp_dictK( int n, ... );
# ypObject *yp_dictKV( int n, va_list args );

# ypObject *yp_frozendict_fromkeysN( ypObject *value, int n, ... );
# ypObject *yp_frozendict_fromkeysV( ypObject *value, int n, va_list args );
# ypObject *yp_dict_fromkeysN( ypObject *value, int n, ... );
# ypObject *yp_dict_fromkeysV( ypObject *value, int n, va_list args );

# ypObject *yp_frozendict( ypObject *x );
# ypObject *yp_dict( ypObject *x );

# XXX The file type will be added in a future version

# yp_hash_t yp_hashC( ypObject *x, ypObject **exc );

# yp_hash_t yp_currenthashC( ypObject *x, ypObject **exc );

# ypObject *yp_send( ypObject **iterator, ypObject *value );

# ypObject *yp_next( ypObject **iterator );

# ypObject *yp_next2( ypObject **iterator, ypObject *defval );

# ypObject *yp_throw( ypObject **iterator, ypObject *exc );

# yp_ssize_t yp_iter_lenhintC( ypObject *iterator, ypObject **exc );

# ypObject *yp_iter_stateX( ypObject *iterator, void **state, yp_ssize_t *size );

# void yp_close( ypObject **iterator );

# typedef ypObject *(*yp_filter_function_t)( ypObject *x );
# ypObject *yp_filter( yp_filter_function_t function, ypObject *iterable );

# ypObject *yp_filterfalse( yp_filter_function_t function, ypObject *iterable );

# ypObject *yp_max_keyN( yp_sort_key_func_t key, int n, ... );
# ypObject *yp_max_keyV( yp_sort_key_func_t key, int n, va_list args );
# ypObject *yp_min_keyN( yp_sort_key_func_t key, int n, ... );
# ypObject *yp_min_keyV( yp_sort_key_func_t key, int n, va_list args );

# ypObject *yp_maxN( int n, ... );
# ypObject *yp_maxV( int n, va_list args );
# ypObject *yp_minN( int n, ... );
# ypObject *yp_minV( int n, va_list args );

# ypObject *yp_max_key( ypObject *iterable, yp_sort_key_func_t key );
# ypObject *yp_min_key( ypObject *iterable, yp_sort_key_func_t key );

# ypObject *yp_max( ypObject *iterable );
# ypObject *yp_min( ypObject *iterable );

# ypObject *yp_reversed( ypObject *seq );

# ypObject *yp_zipN( int n, ... );
# ypObject *yp_zipV( int n, va_list args );

# ypObject *yp_contains( ypObject *container, ypObject *x );
# ypObject *yp_in( ypObject *x, ypObject *container );

# ypObject *yp_not_in( ypObject *x, ypObject *container );

# yp_ssize_t yp_lenC( ypObject *container, ypObject **exc );

# void yp_push( ypObject **container, ypObject *x );

# void yp_clear( ypObject **container );

# ypObject *yp_pop( ypObject **container );


# ypObject *yp_concat( ypObject *sequence, ypObject *x );

# ypObject *yp_repeatC( ypObject *sequence, yp_ssize_t factor );

# ypObject *yp_getindexC( ypObject *sequence, yp_ssize_t i );

# ypObject *yp_getsliceC4( ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

# ypObject *yp_getitem( ypObject *sequence, ypObject *key );

# yp_ssize_t yp_findC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
#         ypObject **exc );

# yp_ssize_t yp_findC( ypObject *sequence, ypObject *x, ypObject **exc );

# yp_ssize_t yp_indexC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
#         ypObject **exc );
# yp_ssize_t yp_indexC( ypObject *sequence, ypObject *x, ypObject **exc );

# yp_ssize_t yp_countC( ypObject *sequence, ypObject *x, ypObject **exc );

# void yp_setindexC( ypObject **sequence, yp_ssize_t i, ypObject *x );

# void yp_setsliceC5( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k, ypObject *x );

# void yp_setitem( ypObject **sequence, ypObject *key, ypObject *x );

# void yp_delindexC( ypObject **sequence, yp_ssize_t i );

# void yp_delsliceC4( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

# void yp_delitem( ypObject **sequence, ypObject *key );

# void yp_append( ypObject **sequence, ypObject *x );
# void yp_push( ypObject **sequence, ypObject *x );

# void yp_extend( ypObject **sequence, ypObject *t );

# void yp_irepeatC( ypObject **sequence, yp_ssize_t factor );

# void yp_insertC( ypObject **sequence, yp_ssize_t i, ypObject *x );

# ypObject *yp_popindexC( ypObject **sequence, yp_ssize_t i );

# ypObject *yp_pop( ypObject **sequence );

# void yp_remove( ypObject **sequence, ypObject *x );

# void yp_reverse( ypObject **sequence );

# void yp_sort3( ypObject **sequence, yp_sort_key_func_t key, ypObject *reverse );

# void yp_sort( ypObject **sequence );

#define yp_SLICE_DEFAULT yp_SSIZE_T_MIN
#define yp_SLICE_USELEN  yp_SSIZE_T_MAX

# ypObject *yp_isdisjoint( ypObject *set, ypObject *x );

# ypObject *yp_issubset( ypObject *set, ypObject *x );

# ypObject *yp_lt( ypObject *set, ypObject *x );

# ypObject *yp_issuperset( ypObject *set, ypObject *x );

# ypObject *yp_gt( ypObject *set, ypObject *x );

# ypObject *yp_unionN( ypObject *set, int n, ... );
# ypObject *yp_unionV( ypObject *set, int n, va_list args );

# ypObject *yp_intersectionN( ypObject *set, int n, ... );
# ypObject *yp_intersectionV( ypObject *set, int n, va_list args );

# ypObject *yp_differenceN( ypObject *set, int n, ... );
# ypObject *yp_differenceV( ypObject *set, int n, va_list args );

# ypObject *yp_symmetric_difference( ypObject *set, ypObject *x );

# void yp_updateN( ypObject **set, int n, ... );
# void yp_updateV( ypObject **set, int n, va_list args );

# void yp_intersection_updateN( ypObject **set, int n, ... );
# void yp_intersection_updateV( ypObject **set, int n, va_list args );

# void yp_difference_updateN( ypObject **set, int n, ... );
# void yp_difference_updateV( ypObject **set, int n, va_list args );

# void yp_symmetric_difference_update( ypObject **set, ypObject *x );

# void yp_push( ypObject **set, ypObject *x );
# void yp_set_add( ypObject **set, ypObject *x );

# ypObject *yp_pushuniqueE( ypObject **set, ypObject *x );

# void yp_remove( ypObject **set, ypObject *x );

# void yp_discard( ypObject **set, ypObject *x );

# ypObject *yp_pop( ypObject **set );


# ypObject *yp_getitem( ypObject *mapping, ypObject *key );

# void yp_setitem( ypObject **mapping, ypObject *key, ypObject *x );

# void yp_delitem( ypObject **mapping, ypObject *key );

# ypObject *yp_getdefault( ypObject *mapping, ypObject *key, ypObject *defval );

# ypObject *yp_iter_items( ypObject *mapping );

# ypObject *yp_iter_keys( ypObject *mapping );

# ypObject *yp_popvalue3( ypObject **mapping, ypObject *key, ypObject *defval );

# void yp_popitem( ypObject **mapping, ypObject **key, ypObject **value );

# ypObject *yp_setdefault( ypObject *mapping, ypObject *key, ypObject *defval );

# void yp_updateK( ypObject **mapping, int n, ... );
# void yp_updateKV( ypObject **mapping, int n, va_list args );

# void yp_updateN( ypObject **mapping, int n, ... );
# void yp_updateV( ypObject **mapping, int n, va_list args );

# ypObject *yp_iter_values( ypObject *mapping );


# XXX bytes- and str-specific methods will be added in a future version

# ypObject *yp_add( ypObject *x, ypObject *y );
# ypObject *yp_sub( ypObject *x, ypObject *y );
# ypObject *yp_mul( ypObject *x, ypObject *y );
# ypObject *yp_truediv( ypObject *x, ypObject *y );
# ypObject *yp_floordiv( ypObject *x, ypObject *y );
# ypObject *yp_mod( ypObject *x, ypObject *y );
# void yp_divmod( ypObject *x, ypObject *y, ypObject **div, ypObject **mod );
# ypObject *yp_pow( ypObject *x, ypObject *y );
# ypObject *yp_pow3( ypObject *x, ypObject *y, ypObject *z );
# ypObject *yp_lshift( ypObject *x, ypObject *y );
# ypObject *yp_rshift( ypObject *x, ypObject *y );
# ypObject *yp_amp( ypObject *x, ypObject *y );
# ypObject *yp_xor( ypObject *x, ypObject *y );
# ypObject *yp_bar( ypObject *x, ypObject *y );
# ypObject *yp_neg( ypObject *x );
# ypObject *yp_pos( ypObject *x );
# ypObject *yp_abs( ypObject *x );
# ypObject *yp_invert( ypObject *x );

# void yp_iadd( ypObject **x, ypObject *y );
# void yp_isub( ypObject **x, ypObject *y );
# void yp_imul( ypObject **x, ypObject *y );
# void yp_itruediv( ypObject **x, ypObject *y );
# void yp_ifloordiv( ypObject **x, ypObject *y );
# void yp_imod( ypObject **x, ypObject *y );
# void yp_ipow( ypObject **x, ypObject *y );
# void yp_ipow3( ypObject **x, ypObject *y, ypObject *z );
# void yp_ilshift( ypObject **x, ypObject *y );
# void yp_irshift( ypObject **x, ypObject *y );
# void yp_iamp( ypObject **x, ypObject *y );
# void yp_ixor( ypObject **x, ypObject *y );
# void yp_ibar( ypObject **x, ypObject *y );
# void yp_ineg( ypObject **x );
# void yp_ipos( ypObject **x );
# void yp_iabs( ypObject **x );
# void yp_iinvert( ypObject **x );

# void yp_iaddC( ypObject **x, yp_int_t y );
# void yp_isubC( ypObject **x, yp_int_t y );
# void yp_imulC( ypObject **x, yp_int_t y );
# void yp_itruedivC( ypObject **x, yp_int_t y );
# void yp_ifloordivC( ypObject **x, yp_int_t y );
# void yp_imodC( ypObject **x, yp_int_t y );
# void yp_ipowC( ypObject **x, yp_int_t y );
# void yp_ipowC3( ypObject **x, yp_int_t y, yp_int_t z );
# void yp_ilshiftC( ypObject **x, yp_int_t y );
# void yp_irshiftC( ypObject **x, yp_int_t y );
# void yp_iampC( ypObject **x, yp_int_t y );
# void yp_ixorC( ypObject **x, yp_int_t y );
# void yp_ibarC( ypObject **x, yp_int_t y );

# void yp_iaddFC( ypObject **x, yp_float_t y );
# void yp_isubFC( ypObject **x, yp_float_t y );
# void yp_imulFC( ypObject **x, yp_float_t y );
# void yp_itruedivFC( ypObject **x, yp_float_t y );
# void yp_ifloordivFC( ypObject **x, yp_float_t y );
# void yp_imodFC( ypObject **x, yp_float_t y );
# void yp_ipowFC( ypObject **x, yp_float_t y );
# void yp_ipowFC3( ypObject **x, yp_float_t y, yp_float_t z );
# void yp_ilshiftFC( ypObject **x, yp_float_t y );
# void yp_irshiftFC( ypObject **x, yp_float_t y );
# void yp_iampFC( ypObject **x, yp_float_t y );
# void yp_ixorFC( ypObject **x, yp_float_t y );
# void yp_ibarFC( ypObject **x, yp_float_t y );

# yp_int_t yp_addL( yp_int_t x, yp_int_t y, ypObject **exc );
# yp_int_t yp_subL( yp_int_t x, yp_int_t y, ypObject **exc );
# yp_int_t yp_mulL( yp_int_t x, yp_int_t y, ypObject **exc );
# yp_int_t yp_truedivL( yp_int_t x, yp_int_t y, ypObject **exc );
# yp_int_t yp_floordivL( yp_int_t x, yp_int_t y, ypObject **exc );
# yp_int_t yp_modL( yp_int_t x, yp_int_t y, ypObject **exc );
# void yp_divmodL( yp_int_t x, yp_int_t y, yp_int_t *div, yp_int_t *mod, ypObject **exc );
# yp_int_t yp_powL( yp_int_t x, yp_int_t y, ypObject **exc );
# yp_int_t yp_powL3( yp_int_t x, yp_int_t y, yp_int_t z, ypObject **exc );
# yp_int_t yp_lshiftL( yp_int_t x, yp_int_t y, ypObject **exc );
# yp_int_t yp_rshiftL( yp_int_t x, yp_int_t y, ypObject **exc );
# yp_int_t yp_ampL( yp_int_t x, yp_int_t y, ypObject **exc );
# yp_int_t yp_xorL( yp_int_t x, yp_int_t y, ypObject **exc );
# yp_int_t yp_barL( yp_int_t x, yp_int_t y, ypObject **exc );
# yp_int_t yp_negL( yp_int_t x, ypObject **exc );
# yp_int_t yp_posL( yp_int_t x, ypObject **exc );
# yp_int_t yp_absL( yp_int_t x, ypObject **exc );
# yp_int_t yp_invertL( yp_int_t x, ypObject **exc );

# yp_float_t yp_addFL( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_subFL( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_mulFL( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_truedivFL( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_floordivFL( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_modFL( yp_float_t x, yp_float_t y, ypObject **exc );
# void yp_divmodFL( yp_float_t x, yp_float_t y, yp_float_t *div, yp_float_t *mod, ypObject **exc );
# yp_float_t yp_powFL( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_powFL3( yp_float_t x, yp_float_t y, yp_float_t z, ypObject **exc );
# yp_float_t yp_lshiftFL( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_rshiftFL( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_ampFL( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_xorFL( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_barFL( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_negFL( yp_float_t x, ypObject **exc );
# yp_float_t yp_posFL( yp_float_t x, ypObject **exc );
# yp_float_t yp_absFL( yp_float_t x, ypObject **exc );
# yp_float_t yp_invertFL( yp_float_t x, ypObject **exc );

# yp_int_t yp_asintC( ypObject *x, ypObject **exc );
# yp_int8_t yp_asint8C( ypObject *x, ypObject **exc );
# yp_uint8_t yp_asuint8C( ypObject *x, ypObject **exc );
# yp_int16_t yp_asint16C( ypObject *x, ypObject **exc );
# yp_uint16_t yp_asuint16C( ypObject *x, ypObject **exc );
# yp_int32_t yp_asint32C( ypObject *x, ypObject **exc );
# yp_uint32_t yp_asuint32C( ypObject *x, ypObject **exc );
# yp_int64_t yp_asint64C( ypObject *x, ypObject **exc );
# yp_uint64_t yp_asuint64C( ypObject *x, ypObject **exc );
# yp_float_t yp_asfloatC( ypObject *x, ypObject **exc );
# yp_float32_t yp_asfloat32C( ypObject *x, ypObject **exc );
# yp_float64_t yp_asfloat64C( ypObject *x, ypObject **exc );
# yp_float_t yp_asfloatL( yp_int_t x, ypObject **exc );
# yp_int_t yp_asintFL( yp_float_t x, ypObject **exc );

# ypObject *yp_roundC( ypObject *x, int ndigits );

# ypObject *yp_sumN( int n, ... );
# ypObject *yp_sumV( int n, va_list args );

# ypObject *yp_sum( ypObject *iterable );


# ypAPI ypObject *yp_BaseException;
# ypAPI ypObject *yp_Exception;
# ypAPI ypObject *yp_StopIteration;
# ypAPI ypObject *yp_GeneratorExit;
# ypAPI ypObject *yp_ArithmeticError;
# ypAPI ypObject *yp_LookupError;
# ypAPI ypObject *yp_AssertionError;
# ypAPI ypObject *yp_AttributeError;
# ypAPI ypObject *yp_EOFError;
# ypAPI ypObject *yp_FloatingPointError;
# ypAPI ypObject *yp_OSError;
# ypAPI ypObject *yp_ImportError;
# ypAPI ypObject *yp_IndexError;
# ypAPI ypObject *yp_KeyError;
# ypAPI ypObject *yp_KeyboardInterrupt;
# ypAPI ypObject *yp_MemoryError;
# ypAPI ypObject *yp_NameError;
# ypAPI ypObject *yp_OverflowError;
# ypAPI ypObject *yp_RuntimeError;
# ypAPI ypObject *yp_NotImplementedError;
# ypAPI ypObject *yp_ReferenceError;
# ypAPI ypObject *yp_SystemError;
# ypAPI ypObject *yp_SystemExit;
# ypAPI ypObject *yp_TypeError;
# ypAPI ypObject *yp_UnboundLocalError;
# ypAPI ypObject *yp_UnicodeError;
# ypAPI ypObject *yp_UnicodeEncodeError;
# ypAPI ypObject *yp_UnicodeDecodeError;
# ypAPI ypObject *yp_UnicodeTranslateError;
# ypAPI ypObject *yp_ValueError;
# ypAPI ypObject *yp_ZeroDivisionError;
# ypAPI ypObject *yp_BufferError;
def ypObject_p_errcheck( x ):
    """Raises the appropriate Python exception if x is a nohtyP exception"""
    if yp_isexceptionC( x ): raise Exception( ) # TODO raise appropriate exceptions


# Raised when the object does not support the given method; subexception of yp_AttributeError
# ypAPI ypObject *yp_MethodError;
# Indicates a limitation in the implementation of nohtyP; subexception of yp_SystemError
# ypAPI ypObject *yp_SystemLimitationError;
# Raised when an invalidated object is passed to a function; subexception of yp_TypeError
# ypAPI ypObject *yp_InvalidatedError;

# int yp_isexceptionC2( ypObject *x, ypObject *exc );

# int yp_isexceptionCN( ypObject *x, int n, ... );


