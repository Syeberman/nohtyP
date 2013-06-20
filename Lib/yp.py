"""
yp.py - Python wrapper for nohtyP
    http://bitbucket.org/Syeberman/nohtyp   [v0.1.0 $Change$]
    Copyright © 2001-2013 Python Software Foundation; All Rights Reserved

    License: http://docs.python.org/3/license.html
"""

# TODO __all__, or underscores

from ctypes import *
import sys, weakref

ypdll = cdll.nohtyP

# From the ctypes documentation...
c_IN = 1
c_OUT = 2
c_INOUT = 3

# Some standard C param/return types
c_void = None # only valid for return values
c_char_pp = POINTER( c_char_p )

# Used to signal that a particular arg has not been supplied; set to default value of such params
_yp_arg_missing = object( )

# Ensures that at most one Python object exists per nohtyP object; make sure new ypObjects are
# given their own references
_yp_pyobj_cache = weakref.WeakValueDictionary( )
def _yp_transmute_and_cache( obj ):
    try: return _yp_pyobj_cache[obj.value] # try to use an existing object
    except KeyError:
        obj.__class__ = ypObject._ypcode2yp[obj._yp_typecode]
        _yp_pyobj_cache[obj.value] = obj
        return obj

class yp_param:
    def __init__( self, type, name, default=_yp_arg_missing, direction=c_IN ):
        self.type = type
        if default is _yp_arg_missing:
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
    if isinstance( result, c_ypObject_p ):
        result = _yp_transmute_and_cache( result )
        # Returned references are always new; no need to incref
    return result

def yp_func( retval, name, paramtuple, errcheck=True ):
    """Defines a function in globals() that wraps the given C yp_* function."""
    # Gather all the information that ctypes needs
    params = tuple( yp_param( *x ) for x in paramtuple )
    proto = CFUNCTYPE( retval, *(x.type for x in params) )
    # XXX ctypes won't allow variable-length arguments if we pass in pflags
    c_func = proto( (name, ypdll) )
    c_func._yp_name = "_"+name
    if errcheck: c_func.errcheck = yp_func_errcheck

    # Create a wrapper function to convert arguments and check for errors (because the way ctypes
    # does it doesn't work well for us...yp_func_errcheck needs the objects _after_ from_param)
    if len( paramtuple ) > 0 and paramtuple[-1] is c_multiN_ypObject_p:
        def c_func_wrapper( *args ):
            fixed, extra = args[:len( params )-1], args[len( params )-1:]
            converted = list( params[i].preconvert( x ) for (i, x) in enumerate( fixed ) )
            converted.append( len( extra ) )
            converted.extend( c_ypObject_p.from_param( x ) for x in extra )
            return c_func( *converted ) # let c_func.errcheck check for errors

    elif len( paramtuple ) > 0 and paramtuple[-1] is c_multiK_ypObject_p:
        def c_func_wrapper( *args ):
            fixed, extra = args[:len( params )-1], args[len( params )-1:]
            assert len( extra ) % 2 == 0
            converted = list( params[i].preconvert( x ) for (i, x) in enumerate( fixed ) )
            converted.append( len( extra ) // 2 )
            converted.extend( c_ypObject_p.from_param( x ) for x in extra )
            return c_func( *converted ) # let c_func.errcheck check for errors

    else:
        def c_func_wrapper( *args ):
            converted = tuple( params[i].preconvert( x ) for (i, x) in enumerate( args ) )
            return c_func( *converted ) # let c_func.errcheck check for errors
    c_func_wrapper.__name__ = c_func._yp_name
    globals( )[c_func._yp_name] = c_func_wrapper


# typedef struct _ypObject ypObject;
class c_ypObject_p( c_void_p ):
    @classmethod
    def from_param( cls, val ):
        if isinstance( val, c_ypObject_p ): return val
        return ypObject.frompython( val )
    def _yp_errcheck( self ):
        ypObject_p_errcheck( self )
    @property
    def _yp_typecode( self ):
        return string_at( self.value, 1 )[0]
class c_ypObject_p_no_errcheck( c_ypObject_p ):
    def _yp_errcheck( self ): pass

def c_ypObject_p_value( name ):
    value = c_ypObject_p.in_dll( ypdll, name )
    value = _yp_transmute_and_cache( value )
    # These values are all immortal; no need to incref
    globals( )[name] = value

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
        # FIXME Make __del__ work during shutdown
        try: _yp_decref( self[0] )
        except: pass
        try: self[0] = yp_None
        except: pass
        #super( ).__del__( self ) # no __del__ method?!
    def __getitem__( self, key ):
        item = super( ).__getitem__( key )
        item_cached = _yp_transmute_and_cache( item )
        # If our item was the one added to the cache, then we need to give it a new reference
        if item is item_cached: _yp_incref( item_cached )
        return item_cached

# ypAPI ypObject *yp_None;

# Special-case arguments
c_ypObject_pp_exc = (c_ypObject_pp, "exc", None)
c_multiN_ypObject_p = (c_int, "n", 0)
c_multiK_ypObject_p = (c_int, "n", 0)
assert c_multiN_ypObject_p is not c_multiK_ypObject_p

# XXX Initialize nohtyP
# ypAPI void yp_initialize( yp_initialize_kwparams *kwparams );
yp_func( c_void, "yp_initialize", ((c_void_p, "kwparams"), ), errcheck=False )
_yp_initialize( None )

# ypAPI ypObject *yp_incref( ypObject *x );
yp_func( c_void_p, "yp_incref", ((c_ypObject_p, "x"), ), errcheck=False )

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
yp_func( c_ypObject_p, "yp_unfrozen_copy", ((c_ypObject_p, "x"), ) )

# ypObject *yp_unfrozen_deepcopy( ypObject *x );
yp_func( c_ypObject_p, "yp_unfrozen_deepcopy", ((c_ypObject_p, "x"), ) )

# ypObject *yp_frozen_copy( ypObject *x );
yp_func( c_ypObject_p, "yp_frozen_copy", ((c_ypObject_p, "x"), ) )

# ypObject *yp_frozen_deepcopy( ypObject *x );
yp_func( c_ypObject_p, "yp_frozen_deepcopy", ((c_ypObject_p, "x"), ) )

# ypObject *yp_copy( ypObject *x );
yp_func( c_ypObject_p, "yp_copy", ((c_ypObject_p, "x"), ) )

# ypObject *yp_deepcopy( ypObject *x );
yp_func( c_ypObject_p, "yp_deepcopy", ((c_ypObject_p, "x"), ) )

# void yp_invalidate( ypObject **x );

# void yp_deepinvalidate( ypObject **x );


# ypObject *yp_True;
# ypObject *yp_False;

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
c_yp_float32_t = c_float
#typedef double              yp_float64_t;
c_yp_float64_t = c_double
#if SIZE_MAX == 0xFFFFFFFFu
#typedef yp_int32_t          yp_ssize_t;
#else
#typedef yp_int64_t          yp_ssize_t;
#endif
c_yp_ssize_t = c_ssize_t
c_yp_ssize_t_p = POINTER( c_yp_ssize_t )
#typedef yp_ssize_t          yp_hash_t;
c_yp_hash_t = c_yp_ssize_t

# typedef yp_int64_t      yp_int_t;
c_yp_int_t = c_int64
# typedef yp_float64_t    yp_float_t;
c_yp_float_t = c_yp_float64_t

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
            (c_void_p, "state"), (c_yp_ssize_t, "size"), c_multiN_ypObject_p) )

# ypObject *yp_rangeC3( yp_int_t start, yp_int_t stop, yp_int_t step );
# ypObject *yp_rangeC( yp_int_t stop );

# ypObject *yp_bytesC( const yp_uint8_t *source, yp_ssize_t len );
yp_func( c_ypObject_p, "yp_bytesC", ((c_char_p, "source"), (c_yp_ssize_t, "len")) )
# ypObject *yp_bytearrayC( const yp_uint8_t *source, yp_ssize_t len );
yp_func( c_ypObject_p, "yp_bytearrayC", ((c_char_p, "source"), (c_yp_ssize_t, "len")) )

# ypObject *yp_str_frombytesC( const yp_uint8_t *source, yp_ssize_t len,
#         ypObject *encoding, ypObject *errors );
yp_func( c_ypObject_p, "yp_str_frombytesC", ((c_char_p, "source"), (c_yp_ssize_t, "len"),
            (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")) )
# ypObject *yp_chrarray_frombytesC( const yp_uint8_t *source, yp_ssize_t len,
#         ypObject *encoding, ypObject *errors );
yp_func( c_ypObject_p, "yp_chrarray_frombytesC", ((c_char_p, "source"), (c_yp_ssize_t, "len"),
            (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")) )

# ypObject *yp_str3( ypObject *object, ypObject *encoding, ypObject *errors );
# ypObject *yp_chrarray3( ypObject *object, ypObject *encoding, ypObject *errors );

# ypObject *yp_str( ypObject *object );
# ypObject *yp_chrarray( ypObject *object );

# ypObject *yp_str0( void );
# ypObject *yp_chrarray0( void );

# ypObject *yp_chrC( yp_int_t i );

# ypObject *yp_tupleN( int n, ... );
yp_func( c_ypObject_p, "yp_tupleN", (c_multiN_ypObject_p, ) )

# ypObject *yp_tupleV( int n, va_list args );
# ypObject *yp_listN( int n, ... );
# ypObject *yp_listV( int n, va_list args );

# ypObject *yp_tuple_repeatCN( yp_ssize_t factor, int n, ... );
# ypObject *yp_tuple_repeatCV( yp_ssize_t factor, int n, va_list args );
# ypObject *yp_list_repeatCN( yp_ssize_t factor, int n, ... );
# ypObject *yp_list_repeatCV( yp_ssize_t factor, int n, va_list args );

# ypObject *yp_tuple( ypObject *iterable );
yp_func( c_ypObject_p, "yp_tuple", ((c_ypObject_p, "iterable"), ) )
# ypObject *yp_list( ypObject *iterable );
yp_func( c_ypObject_p, "yp_list", ((c_ypObject_p, "iterable"), ) )

# typedef ypObject *(*yp_sort_key_func_t)( ypObject *x );
# ypObject *yp_sorted3( ypObject *iterable, yp_sort_key_func_t key, ypObject *reverse );

# ypObject *yp_sorted( ypObject *iterable );

# ypObject *yp_frozensetN( int n, ... );
# ypObject *yp_frozensetV( int n, va_list args );
yp_func( c_ypObject_p, "yp_frozensetN", (c_multiN_ypObject_p, ) )
# ypObject *yp_setN( int n, ... );
# ypObject *yp_setV( int n, va_list args );
yp_func( c_ypObject_p, "yp_setN", (c_multiN_ypObject_p, ) )

# ypObject *yp_frozenset( ypObject *iterable );
yp_func( c_ypObject_p, "yp_frozenset", ((c_ypObject_p, "iterable"), ) )
# ypObject *yp_set( ypObject *iterable );
yp_func( c_ypObject_p, "yp_set", ((c_ypObject_p, "iterable"), ) )

# ypObject *yp_frozendictK( int n, ... );
# ypObject *yp_frozendictKV( int n, va_list args );
# ypObject *yp_dictK( int n, ... );
# ypObject *yp_dictKV( int n, va_list args );
yp_func( c_ypObject_p, "yp_dictK", (c_multiK_ypObject_p, ) )

# ypObject *yp_frozendict_fromkeysN( ypObject *value, int n, ... );
# ypObject *yp_frozendict_fromkeysV( ypObject *value, int n, va_list args );
# ypObject *yp_dict_fromkeysN( ypObject *value, int n, ... );
# ypObject *yp_dict_fromkeysV( ypObject *value, int n, va_list args );
yp_func( c_ypObject_p, "yp_dict_fromkeysN", ((c_ypObject_p, "value"), c_multiN_ypObject_p) )

# ypObject *yp_frozendict( ypObject *x );
yp_func( c_ypObject_p, "yp_frozendict", ((c_ypObject_p, "x"), ) )
# ypObject *yp_dict( ypObject *x );
yp_func( c_ypObject_p, "yp_dict", ((c_ypObject_p, "x"), ) )

# XXX The file type will be added in a future version


# yp_hash_t yp_hashC( ypObject *x, ypObject **exc );
yp_func( c_yp_hash_t, "yp_hashC", ((c_ypObject_p, "x"), c_ypObject_pp_exc) )

# yp_hash_t yp_currenthashC( ypObject *x, ypObject **exc );


# ypObject *yp_send( ypObject *iterator, ypObject *value );

# ypObject *yp_next( ypObject *iterator );
yp_func( c_ypObject_p, "yp_next", ((c_ypObject_p, "iterator"), ) )

# ypObject *yp_next2( ypObject *iterator, ypObject *defval );

# ypObject *yp_throw( ypObject *iterator, ypObject *exc );

# yp_ssize_t yp_iter_lenhintC( ypObject *iterator, ypObject **exc );
yp_func( c_yp_ssize_t, "yp_iter_lenhintC", ((c_ypObject_p, "iterator"), c_ypObject_pp_exc) )

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
yp_func( c_yp_ssize_t, "yp_lenC", ((c_ypObject_p, "container"), c_ypObject_pp_exc) )

# void yp_push( ypObject **container, ypObject *x );
yp_func( c_void, "yp_push", ((c_ypObject_pp, "container"), (c_ypObject_p, "x")) )

# void yp_clear( ypObject **container );
yp_func( c_void, "yp_clear", ((c_ypObject_pp, "container"), ) )

# ypObject *yp_pop( ypObject **container );
yp_func( c_ypObject_p, "yp_pop", ((c_ypObject_pp, "container"), ) )


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
yp_func( c_void, "yp_append", ((c_ypObject_pp, "sequence"), (c_ypObject_p, "x")) )

# void yp_extend( ypObject **sequence, ypObject *t );

# void yp_irepeatC( ypObject **sequence, yp_ssize_t factor );

# void yp_insertC( ypObject **sequence, yp_ssize_t i, ypObject *x );

# ypObject *yp_popindexC( ypObject **sequence, yp_ssize_t i );

# ypObject *yp_pop( ypObject **sequence );

# void yp_remove( ypObject **sequence, ypObject *x );
yp_func( c_void, "yp_remove", ((c_ypObject_pp, "sequence"), (c_ypObject_p, "x")) )

# void yp_reverse( ypObject **sequence );

# void yp_sort3( ypObject **sequence, yp_sort_key_func_t key, ypObject *reverse );

# void yp_sort( ypObject **sequence );

#define yp_SLICE_DEFAULT yp_SSIZE_T_MIN
#define yp_SLICE_USELEN  yp_SSIZE_T_MAX

# ypObject *yp_isdisjoint( ypObject *set, ypObject *x );
yp_func( c_ypObject_p, "yp_isdisjoint", ((c_ypObject_p, "set"), (c_ypObject_p, "x")) )

# ypObject *yp_issubset( ypObject *set, ypObject *x );
yp_func( c_ypObject_p, "yp_issubset", ((c_ypObject_p, "set"), (c_ypObject_p, "x")) )

# ypObject *yp_lt( ypObject *set, ypObject *x );

# ypObject *yp_issuperset( ypObject *set, ypObject *x );
yp_func( c_ypObject_p, "yp_issuperset", ((c_ypObject_p, "set"), (c_ypObject_p, "x")) )

# ypObject *yp_gt( ypObject *set, ypObject *x );

# ypObject *yp_unionN( ypObject *set, int n, ... );
# ypObject *yp_unionV( ypObject *set, int n, va_list args );
yp_func( c_ypObject_p, "yp_unionN", ((c_ypObject_p, "set"), c_multiN_ypObject_p) )

# ypObject *yp_intersectionN( ypObject *set, int n, ... );
# ypObject *yp_intersectionV( ypObject *set, int n, va_list args );
yp_func( c_ypObject_p, "yp_intersectionN", ((c_ypObject_p, "set"), c_multiN_ypObject_p) )

# ypObject *yp_differenceN( ypObject *set, int n, ... );
# ypObject *yp_differenceV( ypObject *set, int n, va_list args );
yp_func( c_ypObject_p, "yp_differenceN", ((c_ypObject_p, "set"), c_multiN_ypObject_p) )

# ypObject *yp_symmetric_difference( ypObject *set, ypObject *x );
yp_func( c_ypObject_p, "yp_symmetric_difference", ((c_ypObject_p, "set"), (c_ypObject_p, "x")) )

# void yp_updateN( ypObject **set, int n, ... );
# void yp_updateV( ypObject **set, int n, va_list args );
yp_func( c_void, "yp_updateN", ((c_ypObject_pp, "set"), c_multiN_ypObject_p) )

# void yp_intersection_updateN( ypObject **set, int n, ... );
# void yp_intersection_updateV( ypObject **set, int n, va_list args );
yp_func( c_void, "yp_intersection_updateN", ((c_ypObject_pp, "set"), c_multiN_ypObject_p) )

# void yp_difference_updateN( ypObject **set, int n, ... );
# void yp_difference_updateV( ypObject **set, int n, va_list args );
yp_func( c_void, "yp_difference_updateN", ((c_ypObject_pp, "set"), c_multiN_ypObject_p) )

# void yp_symmetric_difference_update( ypObject **set, ypObject *x );
yp_func( c_void, "yp_symmetric_difference_update", ((c_ypObject_pp, "set"), (c_ypObject_p, "x")) )

# void yp_push( ypObject **set, ypObject *x );
# void yp_set_add( ypObject **set, ypObject *x );
yp_func( c_void, "yp_set_add", ((c_ypObject_pp, "set"), (c_ypObject_p, "x")) )

# ypObject *yp_pushuniqueE( ypObject **set, ypObject *x );
yp_func( c_void, "yp_pushuniqueE", ((c_ypObject_pp, "set"), (c_ypObject_p, "x")) )

# void yp_remove( ypObject **set, ypObject *x );
# (declared above)

# void yp_discard( ypObject **set, ypObject *x );
yp_func( c_void, "yp_discard", ((c_ypObject_pp, "set"), (c_ypObject_p, "x")) )

# ypObject *yp_pop( ypObject **set );


# ypObject *yp_getitem( ypObject *mapping, ypObject *key );
yp_func( c_ypObject_p, "yp_getitem", ((c_ypObject_p, "mapping"), (c_ypObject_p, "key")) )

# void yp_setitem( ypObject **mapping, ypObject *key, ypObject *x );
yp_func( c_void, "yp_setitem",
        ((c_ypObject_pp, "mapping"), (c_ypObject_p, "key"), (c_ypObject_p, "x")) )

# void yp_delitem( ypObject **mapping, ypObject *key );
yp_func( c_void, "yp_delitem", ((c_ypObject_pp, "mapping"), (c_ypObject_p, "key")) )

# ypObject *yp_getdefault( ypObject *mapping, ypObject *key, ypObject *defval );
yp_func( c_ypObject_p, "yp_getdefault",
        ((c_ypObject_p, "mapping"), (c_ypObject_p, "key"), (c_ypObject_p, "defval")) )

# ypObject *yp_iter_items( ypObject *mapping );
yp_func( c_ypObject_p, "yp_iter_items", ((c_ypObject_p, "mapping"), ) )

# ypObject *yp_iter_keys( ypObject *mapping );
yp_func( c_ypObject_p, "yp_iter_keys", ((c_ypObject_p, "mapping"), ) )

# ypObject *yp_popvalue3( ypObject **mapping, ypObject *key, ypObject *defval );
yp_func( c_ypObject_p, "yp_popvalue3", ((c_ypObject_pp, "mapping"), (c_ypObject_p, "key"),
    (c_ypObject_p, "defval")) )

# void yp_popitem( ypObject **mapping, ypObject **key, ypObject **value );
yp_func( c_void, "yp_popitem", ((c_ypObject_pp, "mapping"), (c_ypObject_pp, "key"),
    (c_ypObject_pp, "value")) )

# ypObject *yp_setdefault( ypObject **mapping, ypObject *key, ypObject *defval );
yp_func( c_ypObject_p, "yp_setdefault",
        ((c_ypObject_pp, "mapping"), (c_ypObject_p, "key"), (c_ypObject_p, "defval")) )

# void yp_updateK( ypObject **mapping, int n, ... );
# void yp_updateKV( ypObject **mapping, int n, va_list args );
yp_func( c_void, "yp_updateK", ((c_ypObject_pp, "mapping"), c_multiK_ypObject_p) )

# void yp_updateN( ypObject **mapping, int n, ... );
# void yp_updateV( ypObject **mapping, int n, va_list args );

# ypObject *yp_iter_values( ypObject *mapping );
yp_func( c_ypObject_p, "yp_iter_values", ((c_ypObject_p, "mapping"), ) )


# ypObject *yp_s_ascii;     // "ascii"
# ypObject *yp_s_latin_1;   // "latin_1"
# ypObject *yp_s_utf_32;    // "utf_32"
# ypObject *yp_s_utf_32_be; // "utf_32_be"
# ypObject *yp_s_utf_32_le; // "utf_32_le"
# ypObject *yp_s_utf_16;    // "utf_16"
# ypObject *yp_s_utf_16_be; // "utf_16_be"
# ypObject *yp_s_utf_16_le; // "utf_16_le"
# ypObject *yp_s_utf_8;     // "utf_8"

# ypObject *yp_s_strict;    // "strict"
# ypObject *yp_s_ignore;    // "ignore"
# ypObject *yp_s_replace;   // "replace"

# XXX Additional bytes- and str-specific methods will be added in a future version


# ypObject *yp_add( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_add", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
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
yp_func( c_yp_int_t, "yp_asintC", ((c_ypObject_p, "x"), c_ypObject_pp_exc) )
# yp_int8_t yp_asint8C( ypObject *x, ypObject **exc );
# yp_uint8_t yp_asuint8C( ypObject *x, ypObject **exc );
# yp_int16_t yp_asint16C( ypObject *x, ypObject **exc );
# yp_uint16_t yp_asuint16C( ypObject *x, ypObject **exc );
# yp_int32_t yp_asint32C( ypObject *x, ypObject **exc );
# yp_uint32_t yp_asuint32C( ypObject *x, ypObject **exc );
# yp_int64_t yp_asint64C( ypObject *x, ypObject **exc );
# yp_uint64_t yp_asuint64C( ypObject *x, ypObject **exc );
# yp_float_t yp_asfloatC( ypObject *x, ypObject **exc );
yp_func( c_yp_float_t, "yp_asfloatC", ((c_ypObject_p, "x"), c_ypObject_pp_exc) )
# yp_float32_t yp_asfloat32C( ypObject *x, ypObject **exc );
# yp_float64_t yp_asfloat64C( ypObject *x, ypObject **exc );
# yp_float_t yp_asfloatL( yp_int_t x, ypObject **exc );
# yp_int_t yp_asintFL( yp_float_t x, ypObject **exc );

# ypObject *yp_roundC( ypObject *x, int ndigits );

# ypObject *yp_sumN( int n, ... );
# ypObject *yp_sumV( int n, va_list args );

# ypObject *yp_sum( ypObject *iterable );


# ypObject *yp_asbytesCX( ypObject *seq, const yp_uint8_t * *bytes, yp_ssize_t *len );

# ypObject *yp_asencodedCX( ypObject *seq, const yp_uint8_t * *encoded, yp_ssize_t *size,
#        ypObject * *encoding );
yp_func( c_ypObject_p, "yp_asencodedCX", ((c_ypObject_p, "seq"),
    (c_char_pp, "encoded"), (c_yp_ssize_t_p, "size"), (c_ypObject_pp, "encoding")) )

# ypObject *yp_itemarrayX( ypObject *seq, ypObject * const * *array, yp_ssize_t *len );


_ypExc2py = {}
_pyExc2yp = {}
def ypObject_p_exception( name, pyExc, *, one_to_one=True ):
    """Use one_to_one=False when pyExc should not be mapped back to the nohtyP exception."""
    ypExc = c_ypObject_p.in_dll( ypdll, name )
    _ypExc2py[ypExc.value] = (name, pyExc)
    if one_to_one: _pyExc2yp[pyExc] = ypExc
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
ypObject_p_exception( "yp_MethodError", AttributeError, one_to_one=False )
# Indicates a limitation in the implementation of nohtyP; subexception of yp_SystemError
ypObject_p_exception( "yp_SystemLimitationError", SystemError, one_to_one=False )
# Raised when an invalidated object is passed to a function; subexception of yp_TypeError
ypObject_p_exception( "yp_InvalidatedError", TypeError, one_to_one=False )

def ypObject_p_errcheck( x ):
    """Raises the appropriate Python exception if x is a nohtyP exception"""
    if _yp_isexceptionC( x ):
        name, pyExc = _ypExc2py[x.value]
        raise pyExc( name )

# int yp_isexceptionC2( ypObject *x, ypObject *exc );
yp_func( c_int, "yp_isexceptionC2", ((c_ypObject_p, "x"), (c_ypObject_p, "exc")) )

# int yp_isexceptionCN( ypObject *x, int n, ... );


class ypObject( c_ypObject_p ):
    def __new__( cls ):
        if cls is ypObject: raise NotImplementedError( "can't instantiate ypObject directly" )
        return super( ).__new__( cls )
    def __init__( self, *args, **kwargs ): pass
    def __del__( self ):
        # FIXME It seems that _yp_decref and yp_None gets set to None when Python is closing:
        # "Python guarantees that globals whose name begins with a single underscore are deleted 
        # from their module before other globals are deleted"
        try: _yp_decref( self )
        except: pass
        return # FIXME Causing a Segmentation Fault sometimes?!?!
        try: self.value = yp_None.value
        except: pass
    _pytype2yp = {}
    _ypcode2yp = {}
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

    def copy( self ): return _yp_copy( self )

    # TODO will this work if yp_bool returns an exception?
    def __bool__( self ): return bool( yp_bool( self ) )
    def __lt__( self, other ): return _yp_lt( self, other )
    def __le__( self, other ): return _yp_le( self, other )
    def __eq__( self, other ): return _yp_eq( self, other )
    def __ne__( self, other ): return _yp_ne( self, other )
    def __ge__( self, other ): return _yp_ge( self, other )
    def __gt__( self, other ): return _yp_gt( self, other )

    def __iter__( self ): return _yp_iter( self )

    def __hash__( self ): return _yp_hashC( self, yp_None )

    def __next__( self ): return _yp_next( self )

    def __contains__( self, x ): return _yp_contains( self, x )
    def __len__( self ): return _yp_lenC( self, yp_None )
    def push( self, x ): _yp_push( self, x )
    def clear( self ): _yp_clear( self )
    def pop( self ): return _yp_pop( self )

    def append( self, x ): _yp_append( self, x )

    def isdisjoint( self, other ):
        return _yp_isdisjoint( self, _yp_iterable( other ) )
    def issubset( self, other ):
        return _yp_issubset( self, _yp_iterable( other ) )
    def issuperset( self, other ):
        return _yp_issuperset( self, _yp_iterable( other ) )
    def union( self, *others ):
        return _yp_unionN( self, *(_yp_iterable( x ) for x in others) )
    def intersection( self, *others ):
        return _yp_intersectionN( self, *(_yp_iterable( x ) for x in others) )
    def difference( self, *others ):
        return _yp_differenceN( self, *(_yp_iterable( x ) for x in others) )
    def symmetric_difference( self, other ):
        return _yp_symmetric_difference( self, _yp_iterable( other ) )
    def update( self, *others ):
        _yp_updateN( self, *(_yp_iterable( x ) for x in others) )
    def intersection_update( self, *others ):
        _yp_intersection_updateN( self, *(_yp_iterable( x ) for x in others) )
    def difference_update( self, *others ):
        _yp_difference_updateN( self, *(_yp_iterable( x ) for x in others) )
    def symmetric_difference_update( self, other ):
        _yp_symmetric_difference_update( self, _yp_iterable( other ) )
    def remove( self, elem ): _yp_remove( self, elem )
    def discard( self, elem ): _yp_discard( self, elem )

    def __getitem__( self, key ): return _yp_getitem( self, key )
    def __setitem__( self, key, value ): _yp_setitem( self, key, value )
    def __delitem__( self, key ): _yp_delitem( self, key )
    def get( self, key, defval=None ): return _yp_getdefault( self, key, defval )
    def setdefault( self, key, defval=None ): return _yp_setdefault( self, key, defval )

    def __add__( self, other ): return _yp_add( self, other )

def pytype( pytypes, ypcode ):
    if not isinstance( pytypes, tuple ): pytypes = (pytypes, )
    def _pytype( cls ):
        for pytype in pytypes:
            ypObject._pytype2yp[pytype] = cls
        ypObject._ypcode2yp[ypcode] = cls
        return cls
    return _pytype

@pytype( type( None ), 6 )
class yp_NoneType( ypObject ):
    def __new__( cls ): raise NotImplementedError( "can't instantiate yp_NoneType directly" )
    @classmethod
    def _frompython( cls, pyobj ):
        assert pyobj is None
        return yp_None
c_ypObject_p_value( "yp_None" )

@pytype( bool, 8 )
class yp_bool( ypObject ):
    def __new__( cls, x=False ):
        if isinstance( x, c_ypObject_p ):
            return _yp_bool( x )
        else:
            return yp_True if x else yp_False
        pass
    def __bool__( self ): return self.value == yp_True.value
    def __lt__( self, other ): return bool( self ) <  other
    def __le__( self, other ): return bool( self ) <= other
    def __eq__( self, other ): return bool( self ) == other
    def __ne__( self, other ): return bool( self ) != other
    def __ge__( self, other ): return bool( self ) >= other
    def __gt__( self, other ): return bool( self ) >  other
c_ypObject_p_value( "yp_True" )
c_ypObject_p_value( "yp_False" )

@pytype( (iter, type(x for x in ())), 15 )
class yp_iter( ypObject ):
    def _pygenerator_func( self, yp_self, yp_value ):
        try:
            if _yp_isexceptionC( yp_value ):
                result = yp_value # yp_GeneratorExit, in particular
            else:
                py_result = next( self._pyiter )
                result = ypObject.frompython( py_result )
        except BaseException as e:
            result = _pyExc2yp[type( e )]
        return _yp_incref( result )

    def __new__( cls, object, sentinel=_yp_arg_missing ):
        if sentinel is not _yp_arg_missing: object = iter( object, sentinel )
        if isinstance( object, ypObject ): return _yp_iter( object )

        try: lenhint = len( object )
        except: lenhint = 0 # TODO try Python's lenhint?
        self = super( ).__new__( cls )
        self._pyiter = iter( object )
        self._pycallback = c_yp_generator_func_t( self._pygenerator_func )
        self.value = _yp_incref( _yp_generator_fromstructCN( self._pycallback, lenhint, 0, 0 ) )
        return self

    def __iter__( self ): return self
    def __length_hint__( self ): return _yp_iter_lenhintC( self, yp_None )

def _yp_iterable( iterable ):
    """Returns a ypObject that nohtyP can iterate over directly, which may be iterable itself or a
    yp_iter based on iterable."""
    if isinstance( iterable, c_ypObject_p ): return iterable
    if isinstance( iterable, str ): return yp_str( iterable )
    return yp_iter( iterable )

@pytype( int, 10 )
class yp_int( ypObject ):
    def __new__( cls, x=0, base=None ):
        if base is None:
            try: return _yp_intC( x )
            except TypeError: pass
            base = 10
        raise NotImplementedError
    # FIXME When nohtyP has str/repr, use it instead of this faked-out version
    def __str__( self ): return str( _yp_asintC( self, yp_None ) )
    def __repr__( self ): return repr( _yp_asintC( self, yp_None ) )

@pytype( float, 12 )
class yp_float( ypObject ):
    def __new__( cls, x=0.0 ):
        return _yp_floatC( x )
    # FIXME When nohtyP has str/repr, use it instead of this faked-out version
    def __str__( self ): return str( _yp_asfloatC( self, yp_None ) )
    def __repr__( self ): return repr( _yp_asfloatC( self, yp_None ) )

# FIXME When nohtyP can encode/decode Unicode directly, use it instead of Python's encode()
# FIXME Just generally move more of this logic into nohtyP, when available
@pytype( bytes, 16 )
class yp_bytes( ypObject ):
    def __new__( cls, source=0, encoding=None, errors=None ):
        if isinstance( source, (bytes, bytearray) ):
            return _yp_bytesC( source, len( source ) )
        elif isinstance( source, str ):
            raise NotImplementedError
        elif isinstance( source, (int, yp_int) ):
            return _yp_bytesC( None, source )
        # else if it has the buffer interface
        # else if it is an iterable
        else:
            raise TypeError( type( source ) )

# FIXME When nohtyP has types that have string representations, update this
# FIXME When nohtyP can decode arbitrary encodings, use that instead of str.encode
# FIXME Just generally move more of this logic into nohtyP, when available
@pytype( str, 18 )
class yp_str( ypObject ):
    def __new__( cls, object=_yp_arg_missing, encoding=_yp_arg_missing, errors=_yp_arg_missing ):
        if encoding is _yp_arg_missing and errors is _yp_arg_missing:
            if object is _yp_arg_missing:
                return _yp_str_frombytesC( None, 0, yp_s_latin_1, yp_s_strict )
            encoded = str( object ).encode( "latin-1" )
            return _yp_str_frombytesC( encoded, len( encoded ), yp_s_latin_1, yp_s_strict )
        else:
            raise NotImplementedError
    # Just as yp_bool.__bool__ must return a bool, so to must this return a str
    def __str__( self ):
        encoded = c_char_pp( c_char_p( ) )
        size = c_yp_ssize_t_p( c_yp_ssize_t( 0 ) )
        encoding = c_ypObject_pp( yp_None )
        _yp_asencodedCX( self, encoded, size, encoding )
        assert encoding[0] == yp_s_latin_1
        return string_at( encoded.contents, size.contents ).decode( "latin-1" )
    # FIXME When nohtyP supports repr, replace this faked-out version
    def __repr__( self ): return repr( str( self ) )
c_ypObject_p_value( "yp_s_ascii" )
c_ypObject_p_value( "yp_s_latin_1" )
c_ypObject_p_value( "yp_s_strict" )

class _ypTuple( ypObject ):
    # nohtyP currently doesn't overload yp_add et al, but Python expects this
    def __add__( self, other ): return _yp_concat( self, other )

@pytype( tuple, 20 )
class yp_tuple( _ypTuple ):
    def __new__( cls, iterable=_yp_arg_missing ):
        if iterable is _yp_arg_missing: return _yp_tupleN( )
        return _yp_tuple( _yp_iterable( iterable ) )
_yp_tuple_empty = yp_tuple( )

@pytype( list, 21 )
class yp_list( _ypTuple ):
    def __new__( cls, iterable=_yp_tuple_empty ):
        return _yp_list( _yp_iterable( iterable ) )

class _ypSet( ypObject ):
    def __new__( cls, iterable=_yp_tuple_empty ):
        return cls._ypSet_constructor( _yp_iterable( iterable ) )
    @staticmethod
    def _bad_other( other ): return not isinstance( other, (_ypSet, frozenset, set) )

    def __or__( self, other ):
        if self._bad_other( other ): return NotImplemented
        return _yp_unionN( self, other )
    def __and__( self, other ):
        if self._bad_other( other ): return NotImplemented
        return _yp_intersectionN( self, other )
    def __sub__( self, other ):
        if self._bad_other( other ): return NotImplemented
        return _yp_differenceN( self, other )
    def __xor__( self, other ):
        if self._bad_other( other ): return NotImplemented
        return _yp_symmetric_difference( self, other )

    def __ror__( self, other ):
        if self._bad_other( other ): return NotImplemented
        return _yp_unionN( other, self )
    def __rand__( self, other ):
        if self._bad_other( other ): return NotImplemented
        return _yp_intersectionN( other, self )
    def __rsub__( self, other ):
        if self._bad_other( other ): return NotImplemented
        return _yp_differenceN( other, self )
    def __rxor__( self, other ):
        if self._bad_other( other ): return NotImplemented
        return _yp_symmetric_difference( other, self )

    def __ior__( self, other ):
        if self._bad_other( other ): return NotImplemented
        _yp_updateN( self, other )
        return self
    def __iand__( self, other ):
        if self._bad_other( other ): return NotImplemented
        _yp_intersection_updateN( self, other )
        return self
    def __isub__( self, other ):
        if self._bad_other( other ): return NotImplemented
        _yp_difference_updateN( self, other )
        return self
    def __ixor__( self, other ):
        if self._bad_other( other ): return NotImplemented
        _yp_symmetric_difference_update( self, other )
        return self

    def add( self, elem ): _yp_set_add( self, elem )

@pytype( frozenset, 22 )
class yp_frozenset( _ypSet ):
    _ypSet_constructor = _yp_frozenset

@pytype( set, 23 )
class yp_set( _ypSet ):
    _ypSet_constructor = _yp_set

# Python dict objects need to be passed through this then sent to the "K" version of the function;
# all other objects can be converted to nohtyP and passed in thusly
def _yp_flatten_dict( args ):
    items = args.items( )
    retval = []
    for item in items:
        assert len( item ) == 2
        retval.extend( item )
    return retval

# TODO If nohtyP ever supports "dict view" objects, replace these faked-out versions
class _setlike_dictview:
    def __init__( self, mp ): self._mp = mp
    def __iter__( self ): return self._iter_func( self._mp )
    def _as_set( self ): return yp_set( self._iter_func( self._mp ) )
    @staticmethod
    def _conv_other( other ):
        if isinstance( other, _setlike_dictview ): return other._as_set( )
        return other
    def __lt__( self, other ): return self._as_set( ) <  self._conv_other( other )
    def __le__( self, other ): return self._as_set( ) <= self._conv_other( other )
    def __eq__( self, other ): return self._as_set( ) == self._conv_other( other )
    def __ne__( self, other ): return self._as_set( ) != self._conv_other( other )
    def __ge__( self, other ): return self._as_set( ) >= self._conv_other( other )
    def __gt__( self, other ): return self._as_set( ) >  self._conv_other( other )
    def __or__( self, other ):  return self._as_set( ) | self._conv_other( other )
    def __and__( self, other ): return self._as_set( ) & self._conv_other( other )
    def __sub__( self, other ): return self._as_set( ) - self._conv_other( other )
    def __xor__( self, other ): return self._as_set( ) ^ self._conv_other( other )
    def __ror__( self, other ):  return self._conv_other( other ) | self._as_set( )
    def __rand__( self, other ): return self._conv_other( other ) & self._as_set( )
    def __rsub__( self, other ): return self._conv_other( other ) - self._as_set( )
    def __rxor__( self, other ): return self._conv_other( other ) ^ self._as_set( )
class _keys_dictview( _setlike_dictview ):
    _iter_func = staticmethod( _yp_iter_keys )
class _values_dictview:
    def __init__( self, mp ): self._mp = mp
    def __iter__( self ): return _yp_iter_values( self._mp )
class _items_dictview( _setlike_dictview ):
    _iter_func = staticmethod( _yp_iter_items )

# FIXME Adapt the Python test suite to test for frozendict, adding in tests similar to those found
# between list/tuple and set/frozenset (ie the singleton empty frozendict, etc)

@pytype( dict, 25 )
class yp_dict( ypObject ):
    def __new__( cls, *args, **kwargs ):
        if len( args ) == 0:
            return _yp_dictK( *_yp_flatten_dict( kwargs ) )
        if len( args ) > 1:
            raise TypeError( "yp_dict expected at most 1 arguments, got %d" % len( args ) )
        if isinstance( args[0], dict ):
            self = _yp_dictK( *_yp_flatten_dict( args[0] ) )
        else:
            self = _yp_dict( _yp_iterable( args[0] ) )
        if len( kwargs ) > 0: _yp_updateK( self, *_yp_flatten_dict( kwargs ) )
        return self
    # TODO A version of yp_dict_fromkeys that accepts a fellow mapping (use only that mapping's
    # keys) or an iterable (each yielded item is a key)...actually I just said the same thing twice
    @classmethod
    def fromkeys( cls, seq, value=None ): return _yp_dict_fromkeysN( value, *seq )
    def keys( self ): return _keys_dictview( self )
    def values( self ): return _values_dictview( self )
    def items( self ): return _items_dictview( self )
    def pop( self, key, default=c_ypObject_p_no_errcheck( _yp_KeyError.value ) ):
        return _yp_popvalue3( self, key, default )
    def popitem( self ):
        key_p = c_ypObject_pp( yp_None )
        value_p = c_ypObject_pp( yp_None )
        _yp_popitem( self, key_p, value_p )
        return (key_p[0], value_p[0])

# FIXME integrate this somehow with unittest
#import os
#os.system( "ypExamples.exe" )

