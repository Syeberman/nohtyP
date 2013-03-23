"""
yp.py - Python wrapper for nohtyP
    http://nohtyp.wordpress.com    [v0.1.0 $Change$]
    Copyright © 2001-2013 Python Software Foundation; All Rights Reserved

    License: http://docs.python.org/3/license.html
"""

#import atexit
#atexit.register( input, "Press Enter to continue..." )

from ctypes import *
import sys

ypdll = cdll.nohtyP

# From the ctypes documentation...
c_IN = 1
c_OUT = 2
c_INOUT = 3

# Some standard C param/return types
c_void = None # only valid for return values

_yp_no_default = object( )
class yp_param:
    def __init__( self, type, name, default=_yp_no_default, direction=c_IN ):
        self.type = type
        if default is _yp_no_default:
            self.pflag = (direction, name)
        else:
            self.pflag = (direction, name, default)
    def preconvert( self, x ):
        if issubclass( self.type, (c_ypObject_p, c_ypObject_pp) ):
            return self.type.from_param( x )
        else:
            return x

def yp_func_errcheck( result, func, args ):
    getattr( result, "_yp_errcheck", int )( )
    for arg in args: getattr( arg, "_yp_errcheck", int )( )
    return args

def yp_func( retval, name, paramtuple, errcheck=True ):
    """Defines a function in globals() that wraps the given C yp_* function."""
    # Gather all the information that ctypes needs
    params = tuple( yp_param( *x ) for x in paramtuple )
    proto = CFUNCTYPE( retval, *(x.type for x in params) )
    c_func = proto( (name, ypdll), tuple( x.pflag for x in params ) )
    c_func._yp_name = "_"+name
    if errcheck: c_func.errcheck = yp_func_errcheck

    # Create a wrapper function to convert arguments and check for errors (because the way ctypes
    # does it doesn't work well for us...yp_func_errcheck needs the objects _after_ from_param)
    def c_func_wrapper( *args ):
        args = tuple( params[i].preconvert( x ) for (i, x) in enumerate( args ) )
        return c_func( *args ) # let c_func.errcheck check for errors
    c_func_wrapper.__name__ = c_func._yp_name
    globals( )[c_func._yp_name] = c_func_wrapper


# ypAPI void yp_initialize( void );
yp_func( c_void, "yp_initialize", (), errcheck=False )

# typedef struct _ypObject ypObject;
class c_ypObject_p( c_void_p ):
    @classmethod
    def from_param( cls, val ):
        if isinstance( val, c_ypObject_p ): return val
        return ypObject.frompython( val )
    def _yp_errcheck( self ):
        ypObject_p_errcheck( self )

def c_ypObject_p_value( name ):
    globals( )["_"+name] = c_ypObject_p.in_dll( ypdll, name )

class c_ypObject_pp( c_ypObject_p*1 ):
    def __init__( self, *args, **kwargs ):
        super( ).__init__( *args, **kwargs )
        _yp_incref( self[0] )
    @classmethod
    def from_param( cls, val ):
        if isinstance( val, c_ypObject_pp ): return val
        obj = cls( c_ypObject_p.from_param( val ) )
        return obj
    def _yp_errcheck( self ):
        ypObject_p_errcheck( self[0] )
    def __del__( self ):
        _yp_decref( self[0] )
        self[0] = _yp_None
        #super( ).__del__( self ) # no __del__ method?!


# ypAPI ypObject *yp_None;
c_ypObject_p_value( "yp_None" )

# ypAPI ypObject *yp_incref( ypObject *x );
yp_func( c_ypObject_p, "yp_incref", ((c_ypObject_p, "x"), ), errcheck=False )

# void yp_increfN( int n, ... );
# void yp_increfV( int n, va_list args );

# void yp_decref( ypObject *x );
yp_func( c_void, "yp_decref", ((c_ypObject_p, "x"), ), errcheck=False )

# void yp_decrefN( int n, ... );
# void yp_decrefV( int n, va_list args );

# Disable errcheck for this to avoid an infinite recursion, as it's used by c_ypObject_p's errcheck
# int yp_isexceptionC( ypObject *x );
yp_func( c_int, "yp_isexceptionC", ((c_ypObject_p, "x"), ), errcheck=False )

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
c_ypObject_p_value( "yp_True" )
# ypObject *yp_False;
c_ypObject_p_value( "yp_False" )

# ypObject *yp_bool( ypObject *x );
yp_func( c_ypObject_p, "yp_bool", ((c_ypObject_p, "x"), ) )

# ypObject *yp_not( ypObject *x );
yp_func( c_ypObject_p, "yp_not", ((c_ypObject_p, "x"), ) )

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
yp_func( c_ypObject_p, "yp_all", ((c_ypObject_p, "iterable"), ) )

# ypObject *yp_lt( ypObject *x, ypObject *y );
# ypObject *yp_le( ypObject *x, ypObject *y );
# ypObject *yp_eq( ypObject *x, ypObject *y );
# ypObject *yp_ne( ypObject *x, ypObject *y );
# ypObject *yp_ge( ypObject *x, ypObject *y );
# ypObject *yp_gt( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_lt", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
yp_func( c_ypObject_p, "yp_le", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
yp_func( c_ypObject_p, "yp_eq", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
yp_func( c_ypObject_p, "yp_ne", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
yp_func( c_ypObject_p, "yp_ge", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
yp_func( c_ypObject_p, "yp_gt", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )


#typedef float               yp_float32_t;
class c_yp_float32_t( c_float ): pass
#typedef double              yp_float64_t;
class c_yp_float64_t( c_double ): pass
#if SIZE_MAX == 0xFFFFFFFFu
#typedef yp_int32_t          yp_ssize_t;
#else
#typedef yp_int64_t          yp_ssize_t;
#endif
class c_yp_ssize_t( c_ssize_t ): pass
#typedef yp_ssize_t          yp_hash_t;
class c_yp_hash_t( c_yp_ssize_t ): pass

# typedef yp_int64_t      yp_int_t;
class c_yp_int_t( c_int64 ): pass
# typedef yp_float64_t    yp_float_t;
class c_yp_float_t( c_yp_float64_t ): pass

# typedef ypObject *(*yp_generator_func_t)( ypObject *self, ypObject *value );
# XXX The return value needs to be a c_void_p to prevent addresses-as-ints from being converted to
# a yp_int
c_yp_generator_func_t = CFUNCTYPE( c_void_p, c_ypObject_p, c_ypObject_p )

# ypObject *yp_intC( yp_int_t value );
yp_func( c_ypObject_p, "yp_intC", ((c_yp_int_t, "value"), ) )
# ypObject *yp_intstoreC( yp_int_t value );
yp_func( c_ypObject_p, "yp_intstoreC", ((c_yp_int_t, "value"), ) )

# ypObject *yp_int_strC( const char *string, int base );
# ypObject *yp_intstore_strC( const char *string, int base );

# ypObject *yp_floatC( yp_float_t value );
yp_func( c_ypObject_p, "yp_floatC", ((c_yp_float_t, "value"), ) )
# ypObject *yp_floatstoreC( yp_float_t value );
yp_func( c_ypObject_p, "yp_floatstoreC", ((c_yp_float_t, "value"), ) )

# ypObject *yp_float_strC( const char *string );
# ypObject *yp_floatstore_strC( const char *string );

# ypObject *yp_iter( ypObject *x );
yp_func( c_ypObject_p, "yp_iter", ((c_ypObject_p, "x"), ) )

# ypObject *yp_generatorCN( yp_generator_func_t func, yp_ssize_t lenhint, int n, ... );
# ypObject *yp_generatorCV( yp_generator_func_t func, yp_ssize_t lenhint, int n, va_list args );

# ypObject *yp_generator_fromstructCN( yp_generator_func_t func, yp_ssize_t lenhint,
#         void *state, yp_ssize_t size, int n, ... );
# ypObject *yp_generator_fromstructCV( yp_generator_func_t func, yp_ssize_t lenhint,
#         void *state, yp_ssize_t size, int n, va_list args );
yp_func( c_ypObject_p, "yp_generator_fromstructCN",
        ((c_yp_generator_func_t, "func"), (c_yp_ssize_t, "lenhint"),
            (c_void_p, "state"), (c_yp_ssize_t, "size"), (c_int, "n", 0)) )

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
yp_func( c_ypObject_p, "yp_frozensetN", ((c_int, "n"), ) ) # FIXME
# ypObject *yp_setN( int n, ... );
# ypObject *yp_setV( int n, va_list args );
yp_func( c_ypObject_p, "yp_setN", ((c_int, "n"), ) ) # FIXME

# ypObject *yp_frozenset( ypObject *iterable );
yp_func( c_ypObject_p, "yp_frozenset", ((c_ypObject_p, "x"), ) )
# ypObject *yp_set( ypObject *iterable );
yp_func( c_ypObject_p, "yp_set", ((c_ypObject_p, "x"), ) )

# ypObject *yp_frozendictK( int n, ... );
# ypObject *yp_frozendictKV( int n, va_list args );
# ypObject *yp_dictK( int n, ... );
# ypObject *yp_dictKV( int n, va_list args );

# ypObject *yp_frozendict_fromkeysN( ypObject *value, int n, ... );
# ypObject *yp_frozendict_fromkeysV( ypObject *value, int n, va_list args );
# ypObject *yp_dict_fromkeysN( ypObject *value, int n, ... );
# ypObject *yp_dict_fromkeysV( ypObject *value, int n, va_list args );

# ypObject *yp_frozendict( ypObject *x );
yp_func( c_ypObject_p, "yp_frozendict", ((c_ypObject_p, "x"), ) )
# ypObject *yp_dict( ypObject *x );
yp_func( c_ypObject_p, "yp_dict", ((c_ypObject_p, "x"), ) )

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
yp_func( c_ypObject_p, "yp_contains", ((c_ypObject_p, "container"), (c_ypObject_p, "x")) )
# ypObject *yp_in( ypObject *x, ypObject *container );
yp_func( c_ypObject_p, "yp_in", ((c_ypObject_p, "x"), (c_ypObject_p, "container")) )

# ypObject *yp_not_in( ypObject *x, ypObject *container );
yp_func( c_ypObject_p, "yp_not_in", ((c_ypObject_p, "x"), (c_ypObject_p, "container")) )

# yp_ssize_t yp_lenC( ypObject *container, ypObject **exc );

# void yp_push( ypObject **container, ypObject *x );

# void yp_clear( ypObject **container );

# ypObject *yp_pop( ypObject **container );


# ypObject *yp_concat( ypObject *sequence, ypObject *x );
yp_func( c_ypObject_p, "yp_concat", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x")) )

# ypObject *yp_repeatC( ypObject *sequence, yp_ssize_t factor );
yp_func( c_ypObject_p, "yp_repeatC", ((c_ypObject_p, "sequence"), (c_yp_ssize_t, "factor")) )

# ypObject *yp_getindexC( ypObject *sequence, yp_ssize_t i );
yp_func( c_ypObject_p, "yp_getindexC", ((c_ypObject_p, "sequence"), (c_yp_ssize_t, "i")) )

# ypObject *yp_getsliceC4( ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );
yp_func( c_ypObject_p, "yp_getsliceC4", ((c_ypObject_p, "sequence"),
    (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), (c_yp_ssize_t, "k")) )

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
yp_func( c_void, "yp_set_add", ((c_ypObject_pp, "set"), (c_ypObject_p, "x")) )

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


_ypExc2py = {}
_pyExc2yp = {}
def ypObject_p_exception( name, pyExc ):
    ypExc = c_ypObject_p.in_dll( ypdll, name )
    _ypExc2py[ypExc.value] = (name, pyExc)
    _pyExc2yp[pyExc] = ypExc
    globals( )["_"+name] = ypExc

ypObject_p_exception( "yp_BaseException", BaseException )
ypObject_p_exception( "yp_Exception", Exception )
ypObject_p_exception( "yp_StopIteration", StopIteration )
ypObject_p_exception( "yp_GeneratorExit", GeneratorExit )
ypObject_p_exception( "yp_ArithmeticError", ArithmeticError )
ypObject_p_exception( "yp_LookupError", LookupError )
ypObject_p_exception( "yp_AssertionError", AssertionError )
ypObject_p_exception( "yp_AttributeError", AttributeError )
ypObject_p_exception( "yp_EOFError", EOFError )
ypObject_p_exception( "yp_FloatingPointError", FloatingPointError )
ypObject_p_exception( "yp_OSError", OSError )
ypObject_p_exception( "yp_ImportError", ImportError )
ypObject_p_exception( "yp_IndexError", IndexError )
ypObject_p_exception( "yp_KeyError", KeyError )
ypObject_p_exception( "yp_KeyboardInterrupt", KeyboardInterrupt )
ypObject_p_exception( "yp_MemoryError", MemoryError )
ypObject_p_exception( "yp_NameError", NameError )
ypObject_p_exception( "yp_OverflowError", OverflowError )
ypObject_p_exception( "yp_RuntimeError", RuntimeError )
ypObject_p_exception( "yp_NotImplementedError", NotImplementedError )
ypObject_p_exception( "yp_ReferenceError", ReferenceError )
ypObject_p_exception( "yp_SystemError", SystemError )
ypObject_p_exception( "yp_SystemExit", SystemExit )
ypObject_p_exception( "yp_TypeError", TypeError )
ypObject_p_exception( "yp_UnboundLocalError", UnboundLocalError )
ypObject_p_exception( "yp_UnicodeError", UnicodeError )
ypObject_p_exception( "yp_UnicodeEncodeError", UnicodeEncodeError )
ypObject_p_exception( "yp_UnicodeDecodeError", UnicodeDecodeError )
ypObject_p_exception( "yp_UnicodeTranslateError", UnicodeTranslateError )
ypObject_p_exception( "yp_ValueError", ValueError )
ypObject_p_exception( "yp_ZeroDivisionError", ZeroDivisionError )
ypObject_p_exception( "yp_BufferError", BufferError )

# Raised when the object does not support the given method; subexception of yp_AttributeError
ypObject_p_exception( "yp_MethodError", AttributeError )
# Indicates a limitation in the implementation of nohtyP; subexception of yp_SystemError
ypObject_p_exception( "yp_SystemLimitationError", SystemError )
# Raised when an invalidated object is passed to a function; subexception of yp_TypeError
ypObject_p_exception( "yp_InvalidatedError", TypeError )

def ypObject_p_errcheck( x ):
    """Raises the appropriate Python exception if x is a nohtyP exception"""
    if _yp_isexceptionC( x ):
        name, pyExc = _ypExc2py[x.value]
        raise pyExc( name )

# int yp_isexceptionC2( ypObject *x, ypObject *exc );
yp_func( c_int, "yp_isexceptionC2", ((c_ypObject_p, "x"), (c_ypObject_p, "exc")) )

# int yp_isexceptionCN( ypObject *x, int n, ... );


class ypObject( c_ypObject_p ):
    def __init__( self ):
        raise NotImplementedError( "can't instantiate ypObject directly" )
    def __del__( self ):
        # FIXME It seems that _yp_decref and _yp_None gets set to None when Python is closing
        try: _yp_decref( self )
        except: pass
        try: self.value = _yp_None.value
        except: self.value = 0
    _pytype2yp = {}
    @classmethod
    def frompython( cls, pyobj ):
        """ypObject.frompython is a factory that returns the correct yp_* object based on the type
        of pyobj.  All other .frompython class methods always return that exact type.
        """
        if isinstance( pyobj, ypObject ): return pyobj
        if cls is ypObject: cls = cls._pytype2yp[type( pyobj )]
        return cls._frompython( pyobj )
    @classmethod
    def _frompython( cls, pyobj ):
        """Default implementation for cls.frompython, which simply calls the constructor."""
        # TODO if every class uses the default _frompython, then remove it, it's not needed
        return cls( pyobj )

    def __contains__( self, x ): return yp_bool( _yp_contains( self, x ) )
# TODO will this work if _yp_bool returns an exception?
    def __bool__( self ): return bool( yp_bool( self ) )

def pytype( pytype ):
    def _pytype( cls ):
        ypObject._pytype2yp[pytype] = cls
        return cls
    return _pytype

@pytype( bool )
class yp_bool( ypObject ):
    def __new__( cls, x=_yp_False, *, _value=None ):
        """_value is for internal use only."""
        if _value is not None:
            self = super( ).__new__( cls )
            self.value = _value
            return self
        elif isinstance( x, c_ypObject_p ):
            x = _yp_bool( x )
            if x.value == _yp_True.value: return yp_True
            if x.value == _yp_False.value: return yp_False
            raise TypeError( "unexpected return from _yp_bool %r" % x )
        else:
            return yp_True if x else yp_False
    def __init__( self, *args, **kwargs ):
        """For internal use only."""
        pass
    def __bool__( self ): return self.value == _yp_True.value
yp_True = yp_bool( _value=_yp_True.value )
yp_False = yp_bool( _value=_yp_False.value )

class yp_iter( ypObject ):
    def _pygenerator_func( self, yp_self, yp_value ):
        try:
            if _yp_isexceptionC( yp_value ):
                result = yp_value # yp_GeneratorExit, in particular
            else:
                result = ypObject.frompython( next( self.pyiter ) )
        except BaseException as e:
            result = _pyExc2yp[type( e )]
        # If we just try to return result here, ctypes gets confused (TODO bug?)
        return result.value

    _no_sentinel = object( )
    def __init__( self, object, sentinel=_no_sentinel ):
        if sentinel is not yp_iter._no_sentinel: object = iter( object, sentinel )
        if isinstance( object, ypObject ):
            self.value = _yp_iter( object ).value
            return

        try: lenhint = len( object )
        except: lenhint = 0 # TODO try Python's lenhint?
        self.pyiter = iter( object )
        self.pycallback = c_yp_generator_func_t( self._pygenerator_func )
        self.value = _yp_generator_fromstructCN( self.pycallback, lenhint, 0, 0, 0 ).value

def yp_iterable( iterable ):
    """Returns a ypObject that nohtyP can iterate over directly, which may be iterable itself or a
    yp_iter based on iterable."""
    if isinstance( iterable, ypObject ): return iterable
    return yp_iter( iterable )

@pytype( int )
class yp_int( ypObject ):
    def __init__( self, x=0, base=None ):
        if base is None:
            try: self.value = _yp_intC( x ).value # TODO can I get rid of .value?
            except TypeError: pass
            else: return
            base = 10
        self.value = _yp_int_strC( x, base ).value

@pytype( set )
class yp_set( ypObject ):
    def __init__( self, iterable=iter( () ) ): # TODO default to a yp_iter
        iterable = yp_iterable( iterable )
        self.value = _yp_set( iterable ).value

    def add( self, x ): _yp_set_add( self, x )



_yp_initialize( )

#so = yp_set( )
#so.add( 5 )
#assert 5 in so
#assert 50 not in so

#import pdb; pdb.set_trace()
#try: _yp_set_add( yp_int( 5 ), 5 )
#except TypeError: pass
#except: raise AssertionError( "should have raised TypeError" )
#else: raise AssertionError( "should have failed" )

#del so # FIXME how can we ensure so gets cleaned up at the end, before ctypes?

# FIXME integrate this somehow with unittest
#import os
#os.system( "ypExamples.exe" )

