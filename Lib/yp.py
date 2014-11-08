"""
yp.py - Python wrapper for nohtyP
    http://bitbucket.org/Syeberman/nohtyp   [v0.1.0 $Change$]
    Copyright © 2001-2013 Python Software Foundation; All Rights Reserved

    License: http://docs.python.org/3/license.html
"""

# TODO __all__, or underscores

from ctypes import *
from ctypes import _SimpleCData
import sys, weakref, operator, pickle, reprlib

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
    if obj.value is None: raise ValueError
    try: return _yp_pyobj_cache[obj.value] # try to use an existing object
    except KeyError:
        obj.__class__ = ypObject._yptype2yp[_yp_type( obj ).value]
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
        elif issubclass( self.type, _SimpleCData ):
            if self.type._type_ in "PzZ": return x  # skip pointers
            if isinstance( x, yp_int ): x = x._asint( )
            converted = self.type( x ).value
            if converted != x:
                raise OverflowError( "overflow in ctypes argument (%r != %r)" % (converted, x) )
            return x
        else:
            return x

def yp_func_errcheck( result, func, args ):
    if isinstance( result, c_ypObject_p ):
        result = _yp_transmute_and_cache( result )
        # Returned references are always new; no need to incref
    getattr( result, "_yp_errcheck", int )( )
    for arg in args: getattr( arg, "_yp_errcheck", int )( )
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
    def _yp_errcheck( self ): pass
    def __reduce__( self ): raise pickle.PicklingError( "can't pickle nohtyP types (yet)" )

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
        self[0]._yp_errcheck( )
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
    def __reduce__( self ): raise pickle.PicklingError( "can't pickle nohtyP types (yet)" )

# ypObject *yp_None;

# Special-case arguments
c_ypObject_pp_exc = (c_ypObject_pp, "exc", None)
c_multiN_ypObject_p = (c_int, "n", 0)
c_multiK_ypObject_p = (c_int, "n", 0)
assert c_multiN_ypObject_p is not c_multiK_ypObject_p

# void yp_initialize( yp_initialize_kwparams *kwparams );

# ypObject *yp_incref( ypObject *x );
yp_func( c_void_p, "yp_incref", ((c_ypObject_p, "x"), ), errcheck=False )

# void yp_increfN( int n, ... );
# void yp_increfNV( int n, va_list args );

# void yp_decref( ypObject *x );
yp_func( c_void, "yp_decref", ((c_ypObject_p, "x"), ), errcheck=False )

# void yp_decrefN( int n, ... );
# void yp_decrefNV( int n, va_list args );

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
# ypObject *yp_orNV( int n, va_list args );

# ypObject *yp_anyN( int n, ... );
# ypObject *yp_anyNV( int n, va_list args );

# ypObject *yp_any( ypObject *iterable );

# ypObject *yp_and( ypObject *x, ypObject *y );

# ypObject *yp_andN( int n, ... );
# ypObject *yp_andNV( int n, va_list args );

# ypObject *yp_allN( int n, ... );
# ypObject *yp_allNV( int n, va_list args );

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
#define yp_SSIZE_T_MAX ((yp_ssize_t) (SIZE_MAX / 2))
_yp_SSIZE_T_MAX = sys.maxsize
#define yp_SSIZE_T_MIN (-yp_SSIZE_T_MAX - 1)
_yp_SSIZE_T_MIN = -_yp_SSIZE_T_MAX - 1

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

# ypObject *yp_int_baseC( ypObject *x, yp_int_t base );
yp_func( c_ypObject_p, "yp_int_baseC", ((c_ypObject_p, "x"), (c_yp_int_t, "base")) )
# ypObject *yp_intstore_baseC( ypObject *x, yp_int_t base );

# ypObject *yp_int( ypObject *x );
yp_func( c_ypObject_p, "yp_int", ((c_ypObject_p, "x"), ) )
# ypObject *yp_intstore( ypObject *x );

# ypObject *yp_floatCF( yp_float_t value );
yp_func( c_ypObject_p, "yp_floatCF", ((c_yp_float_t, "value"), ) )
# ypObject *yp_floatstoreCF( yp_float_t value );

# ypObject *yp_float_strC( const char *string );
# ypObject *yp_floatstore_strC( const char *string );

# ypObject *yp_float( ypObject *x );
yp_func( c_ypObject_p, "yp_float", ((c_ypObject_p, "x"), ) )
# ypObject *yp_floatstore( ypObject *x );

# ypObject *yp_iter( ypObject *x );
yp_func( c_ypObject_p, "yp_iter", ((c_ypObject_p, "x"), ) )

# ypObject *yp_generatorCN( yp_generator_func_t func, yp_ssize_t lenhint, int n, ... );
# ypObject *yp_generatorCNV( yp_generator_func_t func, yp_ssize_t lenhint, int n, va_list args );

# ypObject *yp_generator_fromstructCN( yp_generator_func_t func, yp_ssize_t lenhint,
#         void *state, yp_ssize_t size, int n, ... );
# ypObject *yp_generator_fromstructCNV( yp_generator_func_t func, yp_ssize_t lenhint,
#         void *state, yp_ssize_t size, int n, va_list args );
yp_func( c_ypObject_p, "yp_generator_fromstructCN",
        ((c_yp_generator_func_t, "func"), (c_yp_ssize_t, "lenhint"),
            (c_void_p, "state"), (c_yp_ssize_t, "size"), c_multiN_ypObject_p) )

# ypObject *yp_rangeC3( yp_int_t start, yp_int_t stop, yp_int_t step );
yp_func( c_ypObject_p, "yp_rangeC3",
        ((c_yp_int_t, "start"), (c_yp_int_t, "stop"), (c_yp_int_t, "step")) )
# ypObject *yp_rangeC( yp_int_t stop );
yp_func( c_ypObject_p, "yp_rangeC", ((c_yp_int_t, "stop"), ) )

# ypObject *yp_bytesC( const yp_uint8_t *source, yp_ssize_t len );
yp_func( c_ypObject_p, "yp_bytesC", ((c_char_p, "source"), (c_yp_ssize_t, "len")) )
# ypObject *yp_bytearrayC( const yp_uint8_t *source, yp_ssize_t len );
yp_func( c_ypObject_p, "yp_bytearrayC", ((c_char_p, "source"), (c_yp_ssize_t, "len")) )

# ypObject *yp_bytes3( ypObject *source, ypObject *encoding, ypObject *errors );
yp_func( c_ypObject_p, "yp_bytes3", ((c_ypObject_p, "source"),
            (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")) )
# ypObject *yp_bytearray3( ypObject *source, ypObject *encoding, ypObject *errors );
yp_func( c_ypObject_p, "yp_bytearray3", ((c_ypObject_p, "source"),
            (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")) )

# ypObject *yp_bytes( ypObject *source );
yp_func( c_ypObject_p, "yp_bytes", ((c_ypObject_p, "source"), ) )
# ypObject *yp_bytearray( ypObject *source );
yp_func( c_ypObject_p, "yp_bytearray", ((c_ypObject_p, "source"), ) )

# ypObject *yp_bytes0( void );
yp_func( c_ypObject_p, "yp_bytes0", () )
# ypObject *yp_bytearray0( void );
yp_func( c_ypObject_p, "yp_bytearray0", () )

# ypObject *yp_str_frombytesC4( const yp_uint8_t *source, yp_ssize_t len,
#         ypObject *encoding, ypObject *errors );
yp_func( c_ypObject_p, "yp_str_frombytesC4", ((c_char_p, "source"), (c_yp_ssize_t, "len"),
            (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")) )
# ypObject *yp_chrarray_frombytesC4( const yp_uint8_t *source, yp_ssize_t len,
#         ypObject *encoding, ypObject *errors );
yp_func( c_ypObject_p, "yp_chrarray_frombytesC4", ((c_char_p, "source"), (c_yp_ssize_t, "len"),
            (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")) )

# ypObject *yp_str_frombytesC2( const yp_uint8_t *source, yp_ssize_t len );
yp_func( c_ypObject_p, "yp_str_frombytesC2", ((c_char_p, "source"), (c_yp_ssize_t, "len")) )
# ypObject *yp_chrarray_frombytesC2( const yp_uint8_t *source, yp_ssize_t len );
yp_func( c_ypObject_p, "yp_chrarray_frombytesC2", ((c_char_p, "source"), (c_yp_ssize_t, "len")) )

# ypObject *yp_str3( ypObject *object, ypObject *encoding, ypObject *errors );
yp_func( c_ypObject_p, "yp_str3", ((c_ypObject_p, "object"),
            (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")) )
# ypObject *yp_chrarray3( ypObject *object, ypObject *encoding, ypObject *errors );
yp_func( c_ypObject_p, "yp_chrarray3", ((c_ypObject_p, "object"),
            (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")) )

# ypObject *yp_str( ypObject *object );
yp_func( c_ypObject_p, "yp_str", ((c_ypObject_p, "object"), ) )
# ypObject *yp_chrarray( ypObject *object );
yp_func( c_ypObject_p, "yp_chrarray", ((c_ypObject_p, "object"), ) )

# ypObject *yp_str0( void );
yp_func( c_ypObject_p, "yp_str0", () )
# ypObject *yp_chrarray0( void );
yp_func( c_ypObject_p, "yp_chrarray0", () )

# ypObject *yp_chrC( yp_int_t i );

# ypObject *yp_tupleN( int n, ... );
yp_func( c_ypObject_p, "yp_tupleN", (c_multiN_ypObject_p, ) )

# ypObject *yp_tupleNV( int n, va_list args );
# ypObject *yp_listN( int n, ... );
# ypObject *yp_listNV( int n, va_list args );

# ypObject *yp_tuple_repeatCN( yp_ssize_t factor, int n, ... );
# ypObject *yp_tuple_repeatCNV( yp_ssize_t factor, int n, va_list args );
# ypObject *yp_list_repeatCN( yp_ssize_t factor, int n, ... );
# ypObject *yp_list_repeatCNV( yp_ssize_t factor, int n, va_list args );

# ypObject *yp_tuple( ypObject *iterable );
yp_func( c_ypObject_p, "yp_tuple", ((c_ypObject_p, "iterable"), ) )
# ypObject *yp_list( ypObject *iterable );
yp_func( c_ypObject_p, "yp_list", ((c_ypObject_p, "iterable"), ) )

# typedef ypObject *(*yp_sort_key_func_t)( ypObject *x );
# ypObject *yp_sorted3( ypObject *iterable, yp_sort_key_func_t key, ypObject *reverse );

# ypObject *yp_sorted( ypObject *iterable );

# ypObject *yp_frozensetN( int n, ... );
# ypObject *yp_frozensetNV( int n, va_list args );
yp_func( c_ypObject_p, "yp_frozensetN", (c_multiN_ypObject_p, ) )
# ypObject *yp_setN( int n, ... );
# ypObject *yp_setNV( int n, va_list args );
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
# ypObject *yp_frozendict_fromkeysNV( ypObject *value, int n, va_list args );
# ypObject *yp_dict_fromkeysN( ypObject *value, int n, ... );
# ypObject *yp_dict_fromkeysNV( ypObject *value, int n, va_list args );
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
yp_func( c_ypObject_p, "yp_next", ((c_ypObject_pp, "iterator"), ) )

# ypObject *yp_next2( ypObject *iterator, ypObject *defval );

# ypObject *yp_throw( ypObject *iterator, ypObject *exc );

# yp_ssize_t yp_iter_lenhintC( ypObject *iterator, ypObject **exc );
yp_func( c_yp_ssize_t, "yp_iter_lenhintC", ((c_ypObject_p, "iterator"), c_ypObject_pp_exc) )

# ypObject *yp_iter_stateCX( ypObject *iterator, void **state, yp_ssize_t *size );

# void yp_close( ypObject **iterator );

# typedef ypObject *(*yp_filter_function_t)( ypObject *x );
# ypObject *yp_filter( yp_filter_function_t function, ypObject *iterable );

# ypObject *yp_filterfalse( yp_filter_function_t function, ypObject *iterable );

# ypObject *yp_max_keyN( yp_sort_key_func_t key, int n, ... );
# ypObject *yp_max_keyNV( yp_sort_key_func_t key, int n, va_list args );
# ypObject *yp_min_keyN( yp_sort_key_func_t key, int n, ... );
# ypObject *yp_min_keyNV( yp_sort_key_func_t key, int n, va_list args );

# ypObject *yp_maxN( int n, ... );
# ypObject *yp_maxNV( int n, va_list args );
# ypObject *yp_minN( int n, ... );
# ypObject *yp_minNV( int n, va_list args );

# ypObject *yp_max_key( ypObject *iterable, yp_sort_key_func_t key );
# ypObject *yp_min_key( ypObject *iterable, yp_sort_key_func_t key );

# ypObject *yp_max( ypObject *iterable );
# ypObject *yp_min( ypObject *iterable );

# ypObject *yp_reversed( ypObject *seq );
yp_func( c_ypObject_p, "yp_reversed", ((c_ypObject_p, "seq"), ) )

# ypObject *yp_zipN( int n, ... );
# ypObject *yp_zipNV( int n, va_list args );


# ypObject *yp_contains( ypObject *container, ypObject *x );
yp_func( c_ypObject_p, "yp_contains", ((c_ypObject_p, "container"), (c_ypObject_p, "x")) )
# ypObject *yp_in( ypObject *x, ypObject *container );
yp_func( c_ypObject_p, "yp_in", ((c_ypObject_p, "x"), (c_ypObject_p, "container")) )

# ypObject *yp_not_in( ypObject *x, ypObject *container );
yp_func( c_ypObject_p, "yp_not_in", ((c_ypObject_p, "x"), (c_ypObject_p, "container")) )

# yp_ssize_t yp_lenC( ypObject *container, ypObject **exc );
yp_func( c_yp_ssize_t, "yp_lenC", ((c_ypObject_p, "container"), c_ypObject_pp_exc),
        errcheck=False )

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
yp_func( c_yp_ssize_t, "yp_findC4", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
    (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), c_ypObject_pp_exc) )

# yp_ssize_t yp_findC( ypObject *sequence, ypObject *x, ypObject **exc );
yp_func( c_yp_ssize_t, "yp_findC", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
    c_ypObject_pp_exc) )

# yp_ssize_t yp_indexC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
#         ypObject **exc );
yp_func( c_yp_ssize_t, "yp_indexC4", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
    (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), c_ypObject_pp_exc) )
# yp_ssize_t yp_indexC( ypObject *sequence, ypObject *x, ypObject **exc );
yp_func( c_yp_ssize_t, "yp_indexC", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
    c_ypObject_pp_exc) )

# yp_ssize_t yp_rfindC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
#         ypObject **exc );
yp_func( c_yp_ssize_t, "yp_rfindC4", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
    (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), c_ypObject_pp_exc) )
# yp_ssize_t yp_rfindC( ypObject *sequence, ypObject *x, ypObject **exc );
yp_func( c_yp_ssize_t, "yp_rfindC", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
    c_ypObject_pp_exc) )
# yp_ssize_t yp_rindexC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
#         ypObject **exc );
yp_func( c_yp_ssize_t, "yp_rindexC4", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
    (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), c_ypObject_pp_exc) )
# yp_ssize_t yp_rindexC( ypObject *sequence, ypObject *x, ypObject **exc );
yp_func( c_yp_ssize_t, "yp_rindexC", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
    c_ypObject_pp_exc) )

# yp_ssize_t yp_countC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
#        ypObject **exc );
yp_func( c_yp_ssize_t, "yp_countC4", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
    (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), c_ypObject_pp_exc) )

# yp_ssize_t yp_countC( ypObject *sequence, ypObject *x, ypObject **exc );
yp_func( c_yp_ssize_t, "yp_countC", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
    c_ypObject_pp_exc) )

# void yp_setindexC( ypObject **sequence, yp_ssize_t i, ypObject *x );

# void yp_setsliceC5( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k, ypObject *x );
yp_func( c_void, "yp_setsliceC5", ((c_ypObject_pp, "sequence"),
    (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), (c_yp_ssize_t, "k"), (c_ypObject_p, "x")) )

# void yp_setitem( ypObject **sequence, ypObject *key, ypObject *x );

# void yp_delindexC( ypObject **sequence, yp_ssize_t i );

# void yp_delsliceC4( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );
yp_func( c_void, "yp_delsliceC4", ((c_ypObject_pp, "sequence"),
    (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), (c_yp_ssize_t, "k")) )

# void yp_delitem( ypObject **sequence, ypObject *key );

# void yp_append( ypObject **sequence, ypObject *x );
# void yp_push( ypObject **sequence, ypObject *x );
yp_func( c_void, "yp_append", ((c_ypObject_pp, "sequence"), (c_ypObject_p, "x")) )

# void yp_extend( ypObject **sequence, ypObject *t );
yp_func( c_void, "yp_extend", ((c_ypObject_pp, "sequence"), (c_ypObject_p, "t")) )

# void yp_irepeatC( ypObject **sequence, yp_ssize_t factor );
yp_func( c_void, "yp_irepeatC", ((c_ypObject_pp, "sequence"), (c_yp_ssize_t, "factor")) )

# void yp_insertC( ypObject **sequence, yp_ssize_t i, ypObject *x );
yp_func( c_void, "yp_insertC", ((c_ypObject_pp, "sequence"),
    (c_yp_ssize_t, "i"), (c_ypObject_p, "x")) )

# ypObject *yp_popindexC( ypObject **sequence, yp_ssize_t i );
yp_func( c_ypObject_p, "yp_popindexC", ((c_ypObject_pp, "sequence"), (c_yp_ssize_t, "i")) )

# ypObject *yp_pop( ypObject **sequence );

# void yp_remove( ypObject **sequence, ypObject *x );
yp_func( c_void, "yp_remove", ((c_ypObject_pp, "sequence"), (c_ypObject_p, "x")) )

# void yp_reverse( ypObject **sequence );
yp_func( c_void, "yp_reverse", ((c_ypObject_pp, "sequence"), ) )

# void yp_sort3( ypObject **sequence, yp_sort_key_func_t key, ypObject *reverse );

# void yp_sort( ypObject **sequence );

#define yp_SLICE_DEFAULT yp_SSIZE_T_MIN
_yp_SLICE_DEFAULT = _yp_SSIZE_T_MIN
#define yp_SLICE_USELEN  yp_SSIZE_T_MAX
_yp_SLICE_USELEN = _yp_SSIZE_T_MAX

# ypObject *yp_isdisjoint( ypObject *set, ypObject *x );
yp_func( c_ypObject_p, "yp_isdisjoint", ((c_ypObject_p, "set"), (c_ypObject_p, "x")) )

# ypObject *yp_issubset( ypObject *set, ypObject *x );
yp_func( c_ypObject_p, "yp_issubset", ((c_ypObject_p, "set"), (c_ypObject_p, "x")) )

# ypObject *yp_lt( ypObject *set, ypObject *x );

# ypObject *yp_issuperset( ypObject *set, ypObject *x );
yp_func( c_ypObject_p, "yp_issuperset", ((c_ypObject_p, "set"), (c_ypObject_p, "x")) )

# ypObject *yp_gt( ypObject *set, ypObject *x );

# ypObject *yp_unionN( ypObject *set, int n, ... );
# ypObject *yp_unionNV( ypObject *set, int n, va_list args );
yp_func( c_ypObject_p, "yp_unionN", ((c_ypObject_p, "set"), c_multiN_ypObject_p) )

# ypObject *yp_intersectionN( ypObject *set, int n, ... );
# ypObject *yp_intersectionNV( ypObject *set, int n, va_list args );
yp_func( c_ypObject_p, "yp_intersectionN", ((c_ypObject_p, "set"), c_multiN_ypObject_p) )

# ypObject *yp_differenceN( ypObject *set, int n, ... );
# ypObject *yp_differenceNV( ypObject *set, int n, va_list args );
yp_func( c_ypObject_p, "yp_differenceN", ((c_ypObject_p, "set"), c_multiN_ypObject_p) )

# ypObject *yp_symmetric_difference( ypObject *set, ypObject *x );
yp_func( c_ypObject_p, "yp_symmetric_difference", ((c_ypObject_p, "set"), (c_ypObject_p, "x")) )

# void yp_updateN( ypObject **set, int n, ... );
# void yp_updateNV( ypObject **set, int n, va_list args );
yp_func( c_void, "yp_updateN", ((c_ypObject_pp, "set"), c_multiN_ypObject_p) )

# void yp_intersection_updateN( ypObject **set, int n, ... );
# void yp_intersection_updateNV( ypObject **set, int n, va_list args );
yp_func( c_void, "yp_intersection_updateN", ((c_ypObject_pp, "set"), c_multiN_ypObject_p) )

# void yp_difference_updateN( ypObject **set, int n, ... );
# void yp_difference_updateNV( ypObject **set, int n, va_list args );
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
# void yp_updateNV( ypObject **mapping, int n, va_list args );

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

# ypObject *yp_isalnum( ypObject *s );
yp_func( c_ypObject_p, "yp_isalnum", ((c_ypObject_p, "s"), ) )

# ypObject *yp_isalpha( ypObject *s );
yp_func( c_ypObject_p, "yp_isalpha", ((c_ypObject_p, "s"), ) )

# ypObject *yp_isdecimal( ypObject *s );
yp_func( c_ypObject_p, "yp_isdecimal", ((c_ypObject_p, "s"), ) )

# ypObject *yp_isdigit( ypObject *s );
yp_func( c_ypObject_p, "yp_isdigit", ((c_ypObject_p, "s"), ) )

# ypObject *yp_isidentifier( ypObject *s );
yp_func( c_ypObject_p, "yp_isidentifier", ((c_ypObject_p, "s"), ) )

# ypObject *yp_islower( ypObject *s );
yp_func( c_ypObject_p, "yp_islower", ((c_ypObject_p, "s"), ) )

# ypObject *yp_isnumeric( ypObject *s );
yp_func( c_ypObject_p, "yp_isnumeric", ((c_ypObject_p, "s"), ) )

# ypObject *yp_isprintable( ypObject *s );
yp_func( c_ypObject_p, "yp_isprintable", ((c_ypObject_p, "s"), ) )

# ypObject *yp_isspace( ypObject *s );
yp_func( c_ypObject_p, "yp_isspace", ((c_ypObject_p, "s"), ) )

# ypObject *yp_isupper( ypObject *s );
yp_func( c_ypObject_p, "yp_isupper", ((c_ypObject_p, "s"), ) )

# ypObject *yp_startswithC4( ypObject *s, ypObject *prefix, yp_ssize_t start, yp_ssize_t end );
# ypObject *yp_startswithC( ypObject *s, ypObject *prefix );
yp_func( c_ypObject_p, "yp_startswithC4", ((c_ypObject_p, "s"), (c_ypObject_p, "prefix"),
    (c_yp_ssize_t, "start"), (c_yp_ssize_t, "end")) )
yp_func( c_ypObject_p, "yp_startswithC", ((c_ypObject_p, "s"), (c_ypObject_p, "prefix")) )

# ypObject *yp_endswithC4( ypObject *s, ypObject *suffix, yp_ssize_t start, yp_ssize_t end );
# ypObject *yp_endswithC( ypObject *s, ypObject *suffix );
yp_func( c_ypObject_p, "yp_endswithC4", ((c_ypObject_p, "s"), (c_ypObject_p, "suffix"),
    (c_yp_ssize_t, "start"), (c_yp_ssize_t, "end")) )
yp_func( c_ypObject_p, "yp_endswithC", ((c_ypObject_p, "s"), (c_ypObject_p, "suffix")) )

# ypObject *yp_lower( ypObject *s );
yp_func( c_ypObject_p, "yp_lower", ((c_ypObject_p, "s"), ) )

# ypObject *yp_upper( ypObject *s );
yp_func( c_ypObject_p, "yp_upper", ((c_ypObject_p, "s"), ) )

# ypObject *yp_casefold( ypObject *s );
yp_func( c_ypObject_p, "yp_casefold", ((c_ypObject_p, "s"), ) )

# ypObject *yp_swapcase( ypObject *s );
yp_func( c_ypObject_p, "yp_swapcase", ((c_ypObject_p, "s"), ) )

# ypObject *yp_capitalize( ypObject *s );
yp_func( c_ypObject_p, "yp_capitalize", ((c_ypObject_p, "s"), ) )

# ypObject *yp_ljustC3( ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar );
# ypObject *yp_ljustC( ypObject *s, yp_ssize_t width );
yp_func( c_ypObject_p, "yp_ljustC3", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width"),
    (c_yp_int_t, "ord_fillchar")) )
yp_func( c_ypObject_p, "yp_ljustC", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width")) )

# ypObject *yp_rjustC3( ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar );
# ypObject *yp_rjustC( ypObject *s, yp_ssize_t width );
yp_func( c_ypObject_p, "yp_rjustC3", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width"),
    (c_yp_int_t, "ord_fillchar")) )
yp_func( c_ypObject_p, "yp_rjustC", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width")) )

# ypObject *yp_centerC3( ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar );
# ypObject *yp_centerC( ypObject *s, yp_ssize_t width );
yp_func( c_ypObject_p, "yp_centerC3", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width"),
    (c_yp_int_t, "ord_fillchar")) )
yp_func( c_ypObject_p, "yp_centerC", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width")) )

# ypObject *yp_expandtabsC( ypObject *s, yp_ssize_t tabsize );
yp_func( c_ypObject_p, "yp_expandtabsC", ((c_ypObject_p, "s"), (c_yp_ssize_t, "tabsize")) )

# ypObject *yp_replaceC4( ypObject *s, ypObject *oldsub, ypObject *newsub, yp_ssize_t count );
# ypObject *yp_replace( ypObject *s, ypObject *oldsub, ypObject *newsub );
yp_func( c_ypObject_p, "yp_replaceC4", ((c_ypObject_p, "s"),
    (c_ypObject_p, "oldsub"), (c_ypObject_p, "newsub"), (c_yp_ssize_t, "count")) )
yp_func( c_ypObject_p, "yp_replace", ((c_ypObject_p, "s"),
    (c_ypObject_p, "oldsub"), (c_ypObject_p, "newsub")) )

# ypObject *yp_lstrip2( ypObject *s, ypObject *chars );
# ypObject *yp_lstrip( ypObject *s );
yp_func( c_ypObject_p, "yp_lstrip2", ((c_ypObject_p, "s"), (c_ypObject_p, "chars")) )
yp_func( c_ypObject_p, "yp_lstrip", ((c_ypObject_p, "s"), ) )

# ypObject *yp_rstrip2( ypObject *s, ypObject *chars );
# ypObject *yp_rstrip( ypObject *s );
yp_func( c_ypObject_p, "yp_rstrip2", ((c_ypObject_p, "s"), (c_ypObject_p, "chars")) )
yp_func( c_ypObject_p, "yp_rstrip", ((c_ypObject_p, "s"), ) )

# ypObject *yp_strip2( ypObject *s, ypObject *chars );
# ypObject *yp_strip( ypObject *s );
yp_func( c_ypObject_p, "yp_strip2", ((c_ypObject_p, "s"), (c_ypObject_p, "chars")) )
yp_func( c_ypObject_p, "yp_strip", ((c_ypObject_p, "s"), ) )

# ypObject *yp_join( ypObject *s, ypObject *iterable );
yp_func( c_ypObject_p, "yp_join", ((c_ypObject_p, "s"), (c_ypObject_p, "iterable")) )

# ypObject *yp_joinN( ypObject *s, int n, ... );
# ypObject *yp_joinNV( ypObject *s, int n, va_list args );
yp_func( c_ypObject_p, "yp_joinN", ((c_ypObject_p, "s"), c_multiN_ypObject_p) )

# void yp_partition( ypObject *s, ypObject *sep,
#        ypObject **part0, ypObject **part1, ypObject **part2 );

# void yp_rpartition( ypObject *s, ypObject *sep,
#        ypObject **part0, ypObject **part1, ypObject **part2 );

# ypObject *yp_splitC3( ypObject *s, ypObject *sep, yp_ssize_t maxsplit );
# ypObject *yp_split2( ypObject *s, ypObject *sep );
yp_func( c_ypObject_p, "yp_splitC3", ((c_ypObject_p, "s"), (c_ypObject_p, "sep"),
    (c_yp_ssize_t, "maxsplit")) )
yp_func( c_ypObject_p, "yp_split2", ((c_ypObject_p, "s"), (c_ypObject_p, "sep")) )

# ypObject *yp_split( ypObject *s );
yp_func( c_ypObject_p, "yp_split", ((c_ypObject_p, "s"), ) )

# ypObject *yp_rsplitC3( ypObject *s, ypObject *sep, yp_ssize_t maxsplit );
yp_func( c_ypObject_p, "yp_rsplitC3", ((c_ypObject_p, "s"), (c_ypObject_p, "sep"),
    (c_yp_ssize_t, "maxsplit")) )

# ypObject *yp_splitlines2( ypObject *s, ypObject *keepends );
yp_func( c_ypObject_p, "yp_splitlines2", ((c_ypObject_p, "s"), (c_ypObject_p, "keepends")) )

# ypObject *yp_encode3( ypObject *s, ypObject *encoding, ypObject *errors );
# ypObject *yp_encode( ypObject *s );
yp_func( c_ypObject_p, "yp_encode3", ((c_ypObject_p, "s"),
    (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")) )
yp_func( c_ypObject_p, "yp_encode", ((c_ypObject_p, "s"), ) )

# ypObject *yp_decode3( ypObject *b, ypObject *encoding, ypObject *errors );
# ypObject *yp_decode( ypObject *b );
yp_func( c_ypObject_p, "yp_decode3", ((c_ypObject_p, "b"),
    (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")) )
yp_func( c_ypObject_p, "yp_decode", ((c_ypObject_p, "b"), ) )


# ypObject *yp_add( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_add", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# ypObject *yp_sub( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_sub", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# ypObject *yp_mul( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_mul", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# ypObject *yp_truediv( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_truediv", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# ypObject *yp_floordiv( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_floordiv", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# ypObject *yp_mod( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_mod", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# void yp_divmod( ypObject *x, ypObject *y, ypObject **div, ypObject **mod );
yp_func( c_void, "yp_divmod", ((c_ypObject_p, "x"), (c_ypObject_p, "y"),
    (c_ypObject_pp, "div"), (c_ypObject_pp, "mod")) )
# ypObject *yp_pow( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_pow", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# ypObject *yp_pow3( ypObject *x, ypObject *y, ypObject *z );
# ypObject *yp_lshift( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_lshift", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# ypObject *yp_rshift( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_rshift", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# ypObject *yp_amp( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_amp", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# ypObject *yp_xor( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_xor", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# ypObject *yp_bar( ypObject *x, ypObject *y );
yp_func( c_ypObject_p, "yp_bar", ((c_ypObject_p, "x"), (c_ypObject_p, "y")) )
# ypObject *yp_neg( ypObject *x );
yp_func( c_ypObject_p, "yp_neg", ((c_ypObject_p, "x"), ) )
# ypObject *yp_pos( ypObject *x );
yp_func( c_ypObject_p, "yp_pos", ((c_ypObject_p, "x"), ) )
# ypObject *yp_abs( ypObject *x );
yp_func( c_ypObject_p, "yp_abs", ((c_ypObject_p, "x"), ) )
# ypObject *yp_invert( ypObject *x );
yp_func( c_ypObject_p, "yp_invert", ((c_ypObject_p, "x"), ) )

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

# void yp_iaddCF( ypObject **x, yp_float_t y );
# void yp_isubCF( ypObject **x, yp_float_t y );
# void yp_imulCF( ypObject **x, yp_float_t y );
# void yp_itruedivCF( ypObject **x, yp_float_t y );
# void yp_ifloordivCF( ypObject **x, yp_float_t y );
# void yp_imodCF( ypObject **x, yp_float_t y );
# void yp_ipowCF( ypObject **x, yp_float_t y );
# void yp_ilshiftCF( ypObject **x, yp_float_t y );
# void yp_irshiftCF( ypObject **x, yp_float_t y );
# void yp_iampCF( ypObject **x, yp_float_t y );
# void yp_ixorCF( ypObject **x, yp_float_t y );
# void yp_ibarCF( ypObject **x, yp_float_t y );

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

# yp_float_t yp_addLF( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_subLF( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_mulLF( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_truedivLF( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_floordivLF( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_modLF( yp_float_t x, yp_float_t y, ypObject **exc );
# void yp_divmodLF( yp_float_t x, yp_float_t y, yp_float_t *div, yp_float_t *mod, ypObject **exc );
# yp_float_t yp_powLF( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_lshiftLF( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_rshiftLF( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_ampLF( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_xorLF( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_barLF( yp_float_t x, yp_float_t y, ypObject **exc );
# yp_float_t yp_negLF( yp_float_t x, ypObject **exc );
# yp_float_t yp_posLF( yp_float_t x, ypObject **exc );
# yp_float_t yp_absLF( yp_float_t x, ypObject **exc );
# yp_float_t yp_invertLF( yp_float_t x, ypObject **exc );

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
# yp_int_t yp_asintLF( yp_float_t x, ypObject **exc );

# ypObject *yp_roundC( ypObject *x, int ndigits );

# ypObject *yp_sumN( int n, ... );
# ypObject *yp_sumNV( int n, va_list args );

# ypObject *yp_sum( ypObject *iterable );

# yp_int_t yp_int_bit_lengthC( ypObject *x, ypObject **exc );
yp_func( c_yp_int_t, "yp_int_bit_lengthC", ((c_ypObject_p, "x"), c_ypObject_pp_exc) )

# ypObject * const yp_sys_maxint;
# ypObject * const yp_sys_minint;

# ypObject * const yp_i_neg_one;
# ypObject * const yp_i_zero;
# ypObject * const yp_i_one;
# ypObject * const yp_i_two;


# ypObject *yp_type( ypObject *object );
yp_func( c_ypObject_p, "yp_type", ((c_ypObject_p, "object"), ) )

# ypObject * const yp_type_invalidated;
# ypObject * const yp_type_exception;
# ypObject * const yp_type_type;
# ypObject * const yp_type_NoneType;
# ypObject * const yp_type_bool;
# ypObject * const yp_type_int;
# ypObject * const yp_type_intstore;
# ypObject * const yp_type_float;
# ypObject * const yp_type_floatstore;
# ypObject * const yp_type_iter;
# ypObject * const yp_type_bytes;
# ypObject * const yp_type_bytearray;
# ypObject * const yp_type_str;
# ypObject * const yp_type_chrarray;
# ypObject * const yp_type_tuple;
# ypObject * const yp_type_list;
# ypObject * const yp_type_frozenset;
# ypObject * const yp_type_set;
# ypObject * const yp_type_frozendict;
# ypObject * const yp_type_dict;
# ypObject * const yp_type_range;


# ypObject *yp_asbytesCX( ypObject *seq, const yp_uint8_t * *bytes, yp_ssize_t *len );
yp_func( c_ypObject_p, "yp_asbytesCX", ((c_ypObject_p, "seq"),
    (c_char_pp, "bytes"), (c_yp_ssize_t_p, "len")), errcheck=False )

# ypObject *yp_asencodedCX( ypObject *seq, const yp_uint8_t * *encoded, yp_ssize_t *size,
#        ypObject * *encoding );
yp_func( c_ypObject_p, "yp_asencodedCX", ((c_ypObject_p, "seq"),
    (c_char_pp, "encoded"), (c_yp_ssize_t_p, "size"), (c_ypObject_pp, "encoding")),
    errcheck=False )

# ypObject *yp_itemarrayCX( ypObject *seq, ypObject * const * *array, yp_ssize_t *len );


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
# Raised when an allocation size calculation overflows; subexception of yp_MemoryError
ypObject_p_exception( "yp_MemorySizeOverflowError", MemoryError, one_to_one=False )
# Indicates a limitation in the implementation of nohtyP; subexception of yp_SystemError
ypObject_p_exception( "yp_SystemLimitationError", SystemError, one_to_one=False )
# Raised when an invalidated object is passed to a function; subexception of yp_TypeError
ypObject_p_exception( "yp_InvalidatedError", TypeError, one_to_one=False )

# int yp_isexceptionC2( ypObject *x, ypObject *exc );
yp_func( c_int, "yp_isexceptionC2", ((c_ypObject_p, "x"), (c_ypObject_p, "exc")) )

# int yp_isexceptionCN( ypObject *x, int n, ... );

# typedef struct _yp_initialize_kwparams {...} yp_initialize_kwparams;
class c_yp_initialize_kwparams( Structure ):
    _fields_ = [
        ("struct_size", c_yp_ssize_t),
        ("yp_malloc", c_void_p),
        ("yp_malloc_resize", c_void_p),
        ("yp_free", c_void_p),
        ("everything_immortal", c_int),
    ]
# void yp_initialize( yp_initialize_kwparams *kwparams );
yp_func( c_void, "yp_initialize", ((POINTER( c_yp_initialize_kwparams ), "kwparams"), ),
        errcheck=False )

# Initialize nohtyP
_yp_initparams = c_yp_initialize_kwparams(
    struct_size=sizeof( c_yp_initialize_kwparams ),
    yp_malloc=None,
    yp_malloc_resize=None,
    yp_free=None,
    everything_immortal=False
)
_yp_initialize( _yp_initparams )


class ypObject( c_ypObject_p ):
    def __new__( cls ):
        if cls is ypObject: raise NotImplementedError( "can't instantiate ypObject directly" )
        return super( ).__new__( cls )
    def __init__( self, *args, **kwargs ): pass
    def __del__( self ):
        # FIXME It seems that _yp_decref and yp_None gets set to None when Python is closing:
        # "Python guarantees that globals whose name begins with a single underscore are deleted
        # from their module before other globals are deleted"
        # FIXME Why is self.value sometimes None (ie a null pointer)?  Should never happen.
        try:
            if self.value is not None: _yp_decref( self )
        except: pass
        return # FIXME Causing a Segmentation Fault sometimes?!?!
        try: self.value = yp_None.value
        except: pass
    _pytype2yp = {}
    _yptype2yp = {}
    @classmethod
    def frompython( cls, pyobj ):
        """ypObject.frompython is a factory that returns the correct yp_* object based on the type
        of pyobj.  All other .frompython class methods always return that exact type.
        """
        if isinstance( pyobj, ypObject ): return pyobj
        if isinstance( pyobj, type ) and issubclass( pyobj, ypObject ):
            return pyobj._yp_type
        if cls is ypObject: cls = cls._pytype2yp[type( pyobj )]
        return cls._frompython( pyobj )
    @classmethod
    def _frompython( cls, pyobj ):
        """Default implementation for cls.frompython, which simply calls the constructor."""
        # TODO if every class uses the default _frompython, then remove it, it's not needed
        return cls( pyobj )

    # __str__ and __repr__ must always return a Python str, but we want nohtyP-aware code to be
    # able to get the original yp_str object via _yp_str/_yp_repr
    def __str__( self ): return str( self._yp_str( ) )
    def __repr__( self ): return str( self._yp_repr( ) )

    def copy( self ): return _yp_copy( self )
    def __copy__( self ): return _yp_copy( self )
    def __deepcopy__( self, memo ): return _yp_deepcopy( self )

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
    def __reversed__( self ): return _yp_reversed( self )

    def __contains__( self, x ): return _yp_contains( self, x )
    def __len__( self ):
        # errcheck disabled for _yp_lenC, so do it here
        exc = c_ypObject_pp( yp_None )
        result = _yp_lenC( self, exc )
        exc._yp_errcheck( )
        return result
    def push( self, x ): _yp_push( self, x )
    def clear( self ): _yp_clear( self )
    def pop( self ): return _yp_pop( self )

    def _sliceSearch( self, func2, func4, x, i, j ):
        if i is None and j is None:
            return yp_int( func2( self, x, yp_None ) )
        if i is None: i = 0
        if j is None: j = _yp_SLICE_USELEN
        return yp_int( func4( self, x, i, j, yp_None ) )
    def find( self, x, i=None, j=None ):
        return self._sliceSearch( _yp_findC, _yp_findC4, x, i, j )
    def index( self, x, i=None, j=None ):
        return self._sliceSearch( _yp_indexC, _yp_indexC4, x, i, j )
    def rfind( self, x, i=None, j=None ):
        return self._sliceSearch( _yp_rfindC, _yp_rfindC4, x, i, j )
    def rindex( self, x, i=None, j=None ):
        return self._sliceSearch( _yp_rindexC, _yp_rindexC4, x, i, j )
    def count( self, x, i=None, j=None ):
        return self._sliceSearch( _yp_countC, _yp_countC4, x, i, j )

    def append( self, x ): _yp_append( self, x )
    def extend( self, t ): _yp_extend( self, _yp_iterable( t ) )
    def insert( self, i, x ): _yp_insertC( self, i, x )
    def reverse( self ): _yp_reverse( self )

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

    def _slice( self, func, key, *args ):
        start, stop, step = key.start, key.stop, key.step
        if start is None: start = _yp_SLICE_DEFAULT
        if stop is None: stop = _yp_SLICE_DEFAULT
        if step is None: step = 1
        return func( self, start, stop, step, *args )
    def __getitem__( self, key ):
        if isinstance( key, slice ): return self._slice( _yp_getsliceC4, key )
        else: return _yp_getitem( self, key )
    def __setitem__( self, key, value ):
        if isinstance( key, slice ): self._slice( _yp_setsliceC5, key, value )
        else: _yp_setitem( self, key, value )
    def __delitem__( self, key ):
        if isinstance( key, slice ): self._slice( _yp_delsliceC4, key )
        else: _yp_delitem( self, key )
    def get( self, key, defval=None ): return _yp_getdefault( self, key, defval )
    def setdefault( self, key, defval=None ): return _yp_setdefault( self, key, defval )

    def isalnum( self ): return _yp_isalnum( self )
    def isalpha( self ): return _yp_isalpha( self )
    def isdecimal( self ): return _yp_isdecimal( self )
    def isdigit( self ): return _yp_isdigit( self )
    def isidentifier( self ): return _yp_isidentifier( self )
    def islower( self ): return _yp_islower( self )
    def isnumeric( self ): return _yp_isnumeric( self )
    def isprintable( self ): return _yp_isprintable( self )
    def isspace( self ): return _yp_isspace( self )
    def isupper( self ): return _yp_isupper( self )
    def startswith( self, prefix, start=None, end=None ):
        return self._sliceSearch( _yp_startswithC, _yp_startswithC4, prefix, start, end )
    def endswith( self, suffix, start=None, end=None ):
        return self._sliceSearch( _yp_endswithC, _yp_endswithC4, suffix, start, end )
    def lower( self ): return _yp_lower( self )
    def upper( self ): return _yp_upper( self )
    def casefold( self ): return _yp_casefold( self )
    def swapcase( self ): return _yp_swapcase( self )
    def capitalize( self ): return _yp_capitalize( self )
    def ljust( self, width, fillchar=None ):
        if fillchar is None: return _yp_ljustC( self, width )
        return _yp_ljustC3( self, width, ord( fillchar ) )
    def rjust( self, width, fillchar=None ):
        if fillchar is None: return _yp_rjustC( self, width )
        return _yp_rjustC3( self, width, ord( fillchar ) )
    def center( self, width, fillchar=None ):
        if fillchar is None: return _yp_centerC( self, width )
        return _yp_centerC3( self, width, ord( fillchar ) )
    def expandtabs( self, tabsize=8 ): return _yp_expandtabsC( self, tabsize )
    def replace( self, oldsub, newsub, count=None ):
        if count is None: return _yp_replace( self, oldsub, newsub )
        return _yp_replaceC4( self, oldsub, newsub, count )
    def lstrip( self, chars=_yp_arg_missing ):
        if chars is _yp_arg_missing: return _yp_lstrip( self )
        return _yp_lstrip2( self, chars )
    def rstrip( self, chars=_yp_arg_missing ):
        if chars is _yp_arg_missing: return _yp_rstrip( self )
        return _yp_rstrip2( self, chars )
    def strip( self, chars=_yp_arg_missing ):
        if chars is _yp_arg_missing: return _yp_strip( self )
        return _yp_strip2( self, chars )
    def join( self, iterable ): return _yp_join( self, _yp_iterable( iterable ) )
    def _split( self, func3, sep, maxsplit ):
        if maxsplit is _yp_arg_missing:
            if sep is _yp_arg_missing: return _yp_split( self )
            return _yp_split2( self, sep )
        if sep is _yp_arg_missing: sep = yp_None
        return func3( self, sep, maxsplit )
    def split( self, sep=_yp_arg_missing, maxsplit=_yp_arg_missing ):
        return self._split( _yp_splitC3, sep, maxsplit )
    def rsplit( self, sep=_yp_arg_missing, maxsplit=_yp_arg_missing ):
        return self._split( _yp_rsplitC3, sep, maxsplit )
    def splitlines( self, keepends=True ): return _yp_splitlines2( self, keepends )
    def _encdec( self, func1, func3, encoding, errors ):
        if errors is None:
            if encoding is None: return func1( self )
            errors = yp_s_strict
        if encoding is None: encoding = yp_s_utf_8
        return func3( self, encoding, errors )
    def encode( self, encoding=None, errors=None ):
        return self._encdec( _yp_encode, _yp_encode3, encoding, errors )
    def decode( self, encoding=None, errors=None ):
        return self._encdec( _yp_decode, _yp_decode3, encoding, errors )

    # Python requires arithmetic methods to _return_ NotImplemented
    @staticmethod
    def _arithmetic( func, *args ):
        try: return func( *args )
        except TypeError: return NotImplemented
    def __add__( self, other ): return self._arithmetic( _yp_add, self, other )
    def __sub__( self, other ): return self._arithmetic( _yp_sub, self, other )
    def __mul__( self, other ): return self._arithmetic( _yp_mul, self, other )
    def __truediv__( self, other ): return self._arithmetic( _yp_truediv, self, other )
    def __floordiv__( self, other ): return self._arithmetic( _yp_floordiv, self, other )
    def __mod__( self, other ): return self._arithmetic( _yp_mod, self, other )
    def __pow__( self, other ): return self._arithmetic( _yp_pow, self, other )
    def __lshift__( self, other ): return self._arithmetic( _yp_lshift, self, other )
    def __rshift__( self, other ): return self._arithmetic( _yp_rshift, self, other )
    def __and__( self, other ): return self._arithmetic( _yp_amp, self, other )
    def __xor__( self, other ): return self._arithmetic( _yp_xor, self, other )
    def __or__( self, other ): return self._arithmetic( _yp_bar, self, other )
    def __neg__( self ): return self._arithmetic( _yp_neg, self )
    def __pos__( self ): return self._arithmetic( _yp_pos, self )
    def __abs__( self ): return self._arithmetic( _yp_abs, self )
    def __invert__( self ): return self._arithmetic( _yp_invert, self )

    def __radd__( self, other ): return self._arithmetic( _yp_add, other, self )
    def __rsub__( self, other ): return self._arithmetic( _yp_sub, other, self )
    def __rmul__( self, other ): return self._arithmetic( _yp_mul, other, self )
    def __rtruediv__( self, other ): return self._arithmetic( _yp_truediv, other, self )
    def __rfloordiv__( self, other ): return self._arithmetic( _yp_floordiv, other, self )
    def __rmod__( self, other ): return self._arithmetic( _yp_mod, other, self )
    def __rpow__( self, other ): return self._arithmetic( _yp_pow, other, self )
    def __rlshift__( self, other ): return self._arithmetic( _yp_lshift, other, self )
    def __rrshift__( self, other ): return self._arithmetic( _yp_rshift, other, self )
    def __rand__( self, other ): return self._arithmetic( _yp_amp, other, self )
    def __rxor__( self, other ): return self._arithmetic( _yp_xor, other, self )
    def __ror__( self, other ): return self._arithmetic( _yp_bar, other, self )

    # divmod is a little more complicated
    def __divmod__( self, other ):
        div_p = c_ypObject_pp( yp_None )
        mod_p = c_ypObject_pp( yp_None )
        _yp_divmod( self, other, div_p, mod_p )
        return (div_p[0], mod_p[0])
    def __rdivmod__( self, other ):
        return ypObject.frompython( other ).__divmod__( self )

def pytype( yptype, pytypes ):
    if not isinstance( pytypes, tuple ): pytypes = (pytypes, )
    def _pytype( cls ):
        cls._yp_type = yptype
        for pytype in pytypes:
            ypObject._pytype2yp[pytype] = cls
        ypObject._yptype2yp[yptype.value] = cls
        return cls
    return _pytype

# There's a circular reference between yp_type_type and yp_type we need to dance around
yp_type_type = c_ypObject_p.in_dll( ypdll, "yp_type_type" )
class yp_type( ypObject ):
    _yp_type = yp_type_type
    def __new__( cls, object ):
        if not isinstance( object, ypObject ): raise TypeError( "expected ypObject in yp_type" )
        return object._yp_type
yp_type_type.__class__ = yp_type
_yp_pyobj_cache[yp_type_type.value] = yp_type_type
ypObject._pytype2yp[type] = yp_type
ypObject._yptype2yp[yp_type_type.value] = yp_type

# Now that the "type(type) is type" issue has been dealt with, we can import the other type
# objects as normal
c_ypObject_p_value( "yp_type_invalidated" )
c_ypObject_p_value( "yp_type_exception" )
c_ypObject_p_value( "yp_type_NoneType" )
c_ypObject_p_value( "yp_type_bool" )
c_ypObject_p_value( "yp_type_int" )
c_ypObject_p_value( "yp_type_intstore" )
c_ypObject_p_value( "yp_type_float" )
c_ypObject_p_value( "yp_type_floatstore" )
c_ypObject_p_value( "yp_type_iter" )
c_ypObject_p_value( "yp_type_bytes" )
c_ypObject_p_value( "yp_type_bytearray" )
c_ypObject_p_value( "yp_type_str" )
c_ypObject_p_value( "yp_type_chrarray" )
c_ypObject_p_value( "yp_type_tuple" )
c_ypObject_p_value( "yp_type_list" )
c_ypObject_p_value( "yp_type_frozenset" )
c_ypObject_p_value( "yp_type_set" )
c_ypObject_p_value( "yp_type_frozendict" )
c_ypObject_p_value( "yp_type_dict" )
c_ypObject_p_value( "yp_type_range" )

@pytype( yp_type_exception, BaseException )
class yp_BaseException( ypObject ):
    def __new__( cls, *args, **kwargs ):
        raise NotImplementedError( "can't instantiate yp_BaseException directly" )
    def _yp_errcheck( self ):
        """Raises the appropriate Python exception"""
        super( )._yp_errcheck( )
        name, pyExc = _ypExc2py[self.value]
        if issubclass( pyExc, UnicodeEncodeError ):
            raise pyExc( "<null>", "", 0, 0, name )
        elif issubclass( pyExc, UnicodeDecodeError ):
            raise pyExc( "<null>", b"", 0, 0, name )
        else:
            raise pyExc( name )

@pytype( yp_type_NoneType, type( None ) )
class yp_NoneType( ypObject ):
    def __new__( cls, *args, **kwargs ):
        raise NotImplementedError( "can't instantiate yp_NoneType directly" )
    @classmethod
    def _frompython( cls, pyobj ):
        assert pyobj is None
        return yp_None
    # FIXME When nohtyP has str/repr, use it instead of this faked-out version
    def _yp_str( self ): return yp_s_None
    _yp_repr = _yp_str
c_ypObject_p_value( "yp_None" )

@pytype( yp_type_bool, bool )
class yp_bool( ypObject ):
    def __new__( cls, x=False ):
        if isinstance( x, ypObject ): return _yp_bool( x )
        if isinstance( x, bool ): return yp_True if x else yp_False
        raise TypeError( "expected ypObject or bool in yp_bool" )
    def _as_int( self ): return yp_i_one if self.value == yp_True.value else yp_i_zero

    # FIXME When nohtyP has str/repr, use it instead of this faked-out version
    def _yp_str( self ): return yp_s_True if self.value == yp_True.value else yp_s_False
    _yp_repr = _yp_str

    def __bool__( self ): return self.value == yp_True.value
    def __lt__( self, other ): return bool( self ) <  other
    def __le__( self, other ): return bool( self ) <= other
    def __eq__( self, other ): return bool( self ) == other
    def __ne__( self, other ): return bool( self ) != other
    def __ge__( self, other ): return bool( self ) >= other
    def __gt__( self, other ): return bool( self ) >  other

    # TODO If/when nohtyP supports arithmetic on bool, remove these _as_int hacks
    @staticmethod
    def _arithmetic( left, op, right ):
        if isinstance( left, yp_bool ): left = left._as_int( )
        if isinstance( right, yp_bool ): right = right._as_int( )
        return op( left, right )
    def __add__( self, other ): return yp_bool._arithmetic( self, operator.add, other )
    def __sub__( self, other ): return yp_bool._arithmetic( self, operator.sub, other )
    def __mul__( self, other ): return yp_bool._arithmetic( self, operator.mul, other )
    def __truediv__( self, other ): return yp_bool._arithmetic( self, operator.truediv, other )
    def __floordiv__( self, other ): return yp_bool._arithmetic( self, operator.floordiv, other )
    def __mod__( self, other ): return yp_bool._arithmetic( self, operator.mod, other )
    def __pow__( self, other ): return yp_bool._arithmetic( self, operator.pow, other )
    def __lshift__( self, other ): return yp_bool._arithmetic( self, operator.lshift, other )
    def __rshift__( self, other ): return yp_bool._arithmetic( self, operator.rshift, other )
    def __radd__( self, other ): return yp_bool._arithmetic( other, operator.add, self )
    def __rsub__( self, other ): return yp_bool._arithmetic( other, operator.sub, self )
    def __rmul__( self, other ): return yp_bool._arithmetic( other, operator.mul, self )
    def __rtruediv__( self, other ): return yp_bool._arithmetic( other, operator.truediv, self )
    def __rfloordiv__( self, other ): return yp_bool._arithmetic( other, operator.floordiv, self )
    def __rmod__( self, other ): return yp_bool._arithmetic( other, operator.mod, self )
    def __rpow__( self, other ): return yp_bool._arithmetic( other, operator.pow, self )

    def __neg__( self ): return -self._as_int( )
    def __pos__( self ): return +self._as_int( )
    def __abs__( self ): return abs( self._as_int( ) )
    def __invert__( self ): return ~self._as_int( )

    @staticmethod
    def _boolean( left, op, right ):
        result = yp_bool._arithmetic( left, op, right )
        if isinstance( left, yp_bool ) and isinstance( right, yp_bool ):
            return yp_bool( result )
        return result
    def __and__( self, other ): return yp_bool._boolean( self, operator.and_, other )
    def __xor__( self, other ): return yp_bool._boolean( self, operator.xor, other )
    def __or__( self, other ): return yp_bool._boolean( self, operator.or_, other )
    def __rand__( self, other ): return yp_bool._boolean( other, operator.and_, self )
    def __rxor__( self, other ): return yp_bool._boolean( other, operator.xor, self )
    def __ror__( self, other ): return yp_bool._boolean( other, operator.or_, self )

c_ypObject_p_value( "yp_True" )
c_ypObject_p_value( "yp_False" )

@pytype( yp_type_iter, (iter, type(x for x in ())) )
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
        if isinstance( object, (_setlike_dictview, _values_dictview) ): return iter( object )

        lenhint = NotImplemented
        try: lenhint = len( object )
        except TypeError:
            if hasattr( object, "__length_hint__" ):
                try: lenhint = int( object.__length_hint__( ) )
                except TypeError: pass
        if lenhint == NotImplemented: lenhint = 0 # if all else fails...
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
    # FIXME convert other Python types to nohtyP types, or throw error if they aren't already
    if isinstance( iterable, c_ypObject_p ): return iterable
    if isinstance( iterable, str ): return yp_str( iterable )
    return yp_iter( iterable )

def yp_reversed( x ):
    """Returns reversed( x ) of a ypObject as a yp_iter"""
    if not isinstance( x, ypObject ): raise TypeError( "expected ypObject in yp_reversed" )
    return _yp_reversed( x )

@pytype( yp_type_int, int )
class yp_int( ypObject ):
    def __new__( cls, x=0, base=_yp_arg_missing ):
        if base is _yp_arg_missing:
            if isinstance( x, int ): return _yp_intC( x )
            return _yp_int( x )
        else:
            return _yp_int_baseC( x, base )
    def _asint( self ): return _yp_asintC( self, yp_None )
    # FIXME When nohtyP has str/repr, use it instead of this faked-out version
    def _yp_str( self ): return yp_str( str( self._asint( ) ) )
    def _yp_repr( self ): return yp_str( repr( self._asint( ) ) )
    def bit_length( self ): return yp_int( _yp_int_bit_lengthC( self, yp_None ) )
c_ypObject_p_value( "yp_sys_maxint" )
c_ypObject_p_value( "yp_sys_minint" )
c_ypObject_p_value( "yp_i_neg_one" )
c_ypObject_p_value( "yp_i_zero" )
c_ypObject_p_value( "yp_i_one" )
c_ypObject_p_value( "yp_i_two" )
ypObject_LEN_MAX = yp_int( 0x7FFFFFFF )

def yp_len( x ):
    """Returns len( x ) of a ypObject as a yp_int"""
    if not isinstance( x, ypObject ): raise TypeError( "expected ypObject in yp_len" )
    return yp_int( len( x ) )

def yp_hash( x ):
    """Returns hash( x ) of a ypObject as a yp_int"""
    if not isinstance( x, ypObject ): raise TypeError( "expected ypObject in yp_hash" )
    return yp_int( _yp_hashC( x, yp_None ) )

@pytype( yp_type_float, float )
class yp_float( ypObject ):
    def __new__( cls, x=0.0 ):
        if isinstance( x, float ): return _yp_floatCF( x )
        return _yp_float( x )
    def _asfloat( self ): return _yp_asfloatC( self, yp_None )
    # FIXME When nohtyP has str/repr, use it instead of this faked-out version
    def _yp_str( self ): return yp_str( str( self._asfloat( ) ) )
    def _yp_repr( self ): return yp_str( repr( self._asfloat( ) ) )

class _ypBytes( ypObject ):
    def __new__( cls, source=0, encoding=None, errors=None ):
        if isinstance( source, str ):
            return cls._ypBytes_constructor3( source, encoding, errors )
        elif isinstance( source, (int, yp_int) ):
            return cls._ypBytes_constructor( source )
        elif isinstance( source, (bytes, bytearray) ):
            return cls._ypBytes_constructorC( source, len( source ) )
        # else if it has the buffer interface
        else:
            return cls._ypBytes_constructor( _yp_iterable( source ) )
    def _get_data_size( self ):
        data = c_char_pp( c_char_p( ) )
        size = c_yp_ssize_t_p( c_yp_ssize_t( 0 ) )
        # errcheck disabled for _yp_asbytesCX, so do it here
        _yp_asbytesCX( self, data, size )._yp_errcheck( )
        return cast( data.contents, c_void_p ), size[0]
    def _asbytes( self ):
        data, size = self._get_data_size( )
        return string_at( data, size )
    def _yp_errcheck( self ):
        super( )._yp_errcheck( )
        data, size = self._get_data_size( )
        assert string_at( data.value+size, 1 ) == b"\x00", "missing null terminator"
        return data, size

    # nohtyP currently doesn't overload yp_add et al, but Python expects this
    def __add__( self, other ): return _yp_concat( self, other )
    def __mul__( self, factor ):
        if isinstance( factor, float ): raise TypeError
        return _yp_repeatC( self, factor )

    def __rmul__( self, factor ):
        if isinstance( factor, float ): raise TypeError
        return _yp_repeatC( self, factor )

@pytype( yp_type_bytes, bytes )
class yp_bytes( _ypBytes ):
    _ypBytes_constructorC = _yp_bytesC
    _ypBytes_constructor3 = _yp_bytes3
    _ypBytes_constructor = _yp_bytes
    # FIXME When nohtyP has str/repr, use it instead of this faked-out version
    def _yp_str( self ): return yp_str( str( self._asbytes( ) ) )
    _yp_repr = _yp_str
    def _yp_errcheck( self ):
        data, size = super( )._yp_errcheck( )
        # FIXME ...unless it's built with an empty tuple; is it worth replacing with empty?
        #if size < 1 and "_yp_bytes_empty" in globals( ):
        #    assert self is _yp_bytes_empty, "an empty bytes should be _yp_bytes_empty"
_yp_bytes_empty = yp_bytes( )

@pytype( yp_type_bytearray, bytearray )
class yp_bytearray( _ypBytes ):
    _ypBytes_constructorC = _yp_bytearrayC
    _ypBytes_constructor3 = _yp_bytearray3
    _ypBytes_constructor = _yp_bytearray
    # FIXME When nohtyP has str/repr, use it instead of this faked-out version
    def _yp_str( self ): return yp_str( "bytearray(%r)" % self._asbytes( ) )
    _yp_repr = _yp_str
    def pop( self, i=_yp_arg_missing ):
        if i is _yp_arg_missing: return _yp_pop( self )
        else: return _yp_popindexC( self, i )
    def __iadd__( self, other ):
        _yp_extend( self, other )
        return self
    def __imul__( self, factor ):
        if isinstance( factor, float ): raise TypeError
        _yp_irepeatC( self, factor )
        return self

# FIXME When nohtyP has types that have string representations, update this
# FIXME Just generally move more of this logic into nohtyP, when available
@pytype( yp_type_str, str )
class yp_str( ypObject ):
    def __new__( cls, object=_yp_arg_missing, encoding=_yp_arg_missing, errors=_yp_arg_missing ):
        if encoding is _yp_arg_missing and errors is _yp_arg_missing:
            if object is _yp_arg_missing: return _yp_str0( )
            if isinstance( object, ypObject ): return object._yp_str( )
            if isinstance( object, str ):
                encoded = object.encode( "utf-8", "surrogatepass" )
                return _yp_str_frombytesC4( encoded, len( encoded ), 
                    yp_s_utf_8, yp_s_surrogatepass )
            raise TypeError( "expected ypObject or str in yp_str" )
        else:
            if object is _yp_arg_missing: object = _yp_bytes_empty
            if encoding is _yp_arg_missing: encoding = yp_s_utf_8
            if errors is _yp_arg_missing: errors = yp_s_strict
            return _yp_str3( object, encoding, errors )
    def _get_encoded_size_encoding( self ):
        encoded = c_char_pp( c_char_p( ) )
        size = c_yp_ssize_t_p( c_yp_ssize_t( 0 ) )
        encoding = c_ypObject_pp( yp_None )
        # errcheck disabled for _yp_asencodedCX, so do it here
        _yp_asencodedCX( self, encoded, size, encoding )._yp_errcheck( )
        # _yp_asencodedCX should return one of yp_s_latin_1, yp_s_ucs_2, or yp_s_ucs_4
        enc_type, enc_elemsize = _yp_str_enc2type[encoding[0].value]
        return cast( encoded.contents, enc_type ), size[0]//enc_elemsize, encoding[0]
    def _yp_errcheck( self ):
        super( )._yp_errcheck( )
        encoded, size, encoding = self._get_encoded_size_encoding( )
        if encoding is yp_s_ucs_2:
            pass # FIXME ensure string contains at least one >0xFF character
        elif encoding is yp_s_ucs_4:
            pass # FIXME ensure string contains at least one >0xFFFF character
        assert encoded[size] == 0, "missing null terminator"
        # FIXME ...unless it's built with an empty tuple; is it worth replacing with empty?
        #if size < 1 and "_yp_str_empty" in globals( ):
        #    assert self is _yp_str_empty, "an empty str should be _yp_str_empty"

    # Just as yp_bool.__bool__ must return a bool, so to must this return a str
    def __str__( self ): return self.encode( )._asbytes( ).decode( )
    # FIXME When nohtyP supports repr, replace this faked-out version
    def _yp_str( self ): return self
    def _yp_repr( self ): return repr( str( self ) )

    # nohtyP currently doesn't overload yp_add et al, but Python expects this
    def __add__( self, other ): return _yp_concat( self, other )
    def __mul__( self, factor ):
        if isinstance( factor, float ): raise TypeError
        return _yp_repeatC( self, factor )

    def __rmul__( self, factor ):
        if isinstance( factor, float ): raise TypeError
        return _yp_repeatC( self, factor )

c_ypObject_p_value( "yp_s_ascii" )
c_ypObject_p_value( "yp_s_latin_1" )
c_ypObject_p_value( "yp_s_utf_8" )
c_ypObject_p_value( "yp_s_ucs_2" )
c_ypObject_p_value( "yp_s_ucs_4" )
c_ypObject_p_value( "yp_s_strict" )
c_ypObject_p_value( "yp_s_replace" )
c_ypObject_p_value( "yp_s_ignore" )
c_ypObject_p_value( "yp_s_xmlcharrefreplace" )
c_ypObject_p_value( "yp_s_backslashreplace" )
c_ypObject_p_value( "yp_s_surrogateescape" )
c_ypObject_p_value( "yp_s_surrogatepass" )
_yp_str_enc2type = {
        yp_s_latin_1.value: (POINTER( c_uint8 ),  1),
        yp_s_ucs_2.value:   (POINTER( c_uint16 ), 2),
        yp_s_ucs_4.value:   (POINTER( c_uint32 ), 4)}
_yp_str_empty = yp_str( )
yp_s_None = yp_str( "None" )
yp_s_True = yp_str( "True" )
yp_s_False = yp_str( "False" )

def yp_repr( object ):
    """Returns repr( object ) of a ypObject as a yp_str"""
    if not isinstance( object, ypObject ): raise TypeError( "expected ypObject in yp_repr" )
    return object._yp_repr( )

class _ypTuple( ypObject ):
    # nohtyP currently doesn't overload yp_add et al, but Python expects this
    def __add__( self, other ): return _yp_concat( self, other )
    def __mul__( self, factor ):
        if isinstance( factor, float ): raise TypeError
        return _yp_repeatC( self, factor )

    def __rmul__( self, factor ):
        if isinstance( factor, float ): raise TypeError
        return _yp_repeatC( self, factor )

@pytype( yp_type_tuple, tuple )
class yp_tuple( _ypTuple ):
    def __new__( cls, iterable=_yp_arg_missing ):
        if iterable is _yp_arg_missing: return _yp_tupleN( )
        return _yp_tuple( _yp_iterable( iterable ) )
    def _yp_errcheck( self ):
        super( )._yp_errcheck( )
        # FIXME ...unless it's built with an empty tuple; is it worth replacing with empty?
        #if len( self ) < 1 and "_yp_tuple_empty" in globals( ):
        #    assert self is _yp_tuple_empty, "an empty tuple should be _yp_tuple_empty"
    # FIXME When nohtyP supports str/repr, replace this faked-out version
    def _yp_str( self ):
        return yp_str( "(%s)" % ", ".join( repr( x ) for x in self ) )
    _yp_repr = _yp_str
_yp_tuple_empty = yp_tuple( )

@pytype( yp_type_list, list )
class yp_list( _ypTuple ):
    def __new__( cls, iterable=_yp_tuple_empty ):
        return _yp_list( _yp_iterable( iterable ) )
    # FIXME When nohtyP supports str/repr, replace this faked-out version
    @reprlib.recursive_repr( "[...]" )
    def _yp_str( self ):
        return yp_str( "[%s]" % ", ".join( repr( x ) for x in self ) )
    _yp_repr = _yp_str
    def pop( self, i=_yp_arg_missing ):
        if i is _yp_arg_missing: return _yp_pop( self )
        else: return _yp_popindexC( self, i )
    def __iadd__( self, other ):
        _yp_extend( self, other )
        return self
    def __imul__( self, factor ):
        if isinstance( factor, float ): raise TypeError
        _yp_irepeatC( self, factor )
        return self

# FIXME When nohtyP supports sorting, replace this faked-out version
def yp_sorted( x, *, key=None, reverse=False ):
    """Returns sorted( x ) of a ypObject as a yp_list"""
    if not isinstance( x, (ypObject, _setlike_dictview, _values_dictview) ):
        raise TypeError( "expected ypObject in yp_sorted" )
    return yp_list( sorted( x, key=key, reverse=reverse ) )

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

@pytype( yp_type_frozenset, frozenset )
class yp_frozenset( _ypSet ):
    _ypSet_constructor = _yp_frozenset
    def _yp_errcheck( self ):
        super( )._yp_errcheck( )
        # FIXME ...unless it's built with an empty tuple; is it worth replacing with empty?
        #if len( self ) < 1 and "_yp_frozenset_empty" in globals( ):
        #    assert self is _yp_frozenset_empty, "an empty frozenset should be _yp_frozenset_empty"
_yp_frozenset_empty = yp_frozenset( )

@pytype( yp_type_set, set )
class yp_set( _ypSet ):
    _ypSet_constructor = _yp_set

# Python dict objects need to be passed through this then sent to the "K" version of the function;
# all other objects can be converted to nohtyP and passed in thusly
def _yp_flatten_dict( args ):
    keys = args.keys( )
    retval = []
    for key in keys:
        retval.append( key )
        retval.append( args[key] )
    return retval

def _yp_dict_iterable( iterable ):
    """Like _yp_iterable, but returns an "item iterator" if iterable is a mapping object."""
    if isinstance( iterable, c_ypObject_p ): return iterable
    if isinstance( iterable, str ): return yp_str( iterable )
    if hasattr( iterable, "keys" ):
        return yp_iter( (k, iterable[k]) for k in iterable.keys( ) )
    return yp_iter( iterable )

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

@pytype( yp_type_dict, dict )
class yp_dict( ypObject ):
    def __new__( cls, *args, **kwargs ):
        if len( args ) == 0:
            return _yp_dictK( *_yp_flatten_dict( kwargs ) )
        if len( args ) > 1:
            raise TypeError( "yp_dict expected at most 1 arguments, got %d" % len( args ) )
        self = _yp_dict( _yp_dict_iterable( args[0] ) )
        if len( kwargs ) > 0: _yp_updateK( self, *_yp_flatten_dict( kwargs ) )
        return self
    # TODO A version of yp_dict_fromkeys that accepts a fellow mapping (use only that mapping's
    # keys) or an iterable (each yielded item is a key)...actually I just said the same thing twice
    @classmethod
    def fromkeys( cls, seq, value=None ): return _yp_dict_fromkeysN( value, *seq )
    def keys( self ): return _keys_dictview( self )
    def values( self ): return _values_dictview( self )
    def items( self ): return _items_dictview( self )
    def pop( self, key, default=_yp_KeyError ):
        return _yp_popvalue3( self, key, default )
    def popitem( self ):
        key_p = c_ypObject_pp( yp_None )
        value_p = c_ypObject_pp( yp_None )
        _yp_popitem( self, key_p, value_p )
        return (key_p[0], value_p[0])
    def update( self, *args, **kwargs ):
        if len( args ) == 0:
            return _yp_updateK( self, *_yp_flatten_dict( kwargs ) )
        if len( args ) > 1:
            raise TypeError( "update expected at most 1 arguments, got %d" % len( args ) )
        _yp_updateN( self, _yp_dict_iterable( args[0] ) )
        if len( kwargs ) > 0: _yp_updateK( self, *_yp_flatten_dict( kwargs ) )

@pytype( yp_type_range, range )
class yp_range( ypObject ):
    @staticmethod
    def _new_range( start, stop, step=1 ):
        return _yp_rangeC3( start, stop, step )
    def __new__( cls, *args ):
        if len( args ) == 1: return _yp_rangeC( args[0] )
        return cls._new_range( *args )
    def _yp_errcheck( self ):
        super( )._yp_errcheck( )
        if len( self ) < 1 and "_yp_range_empty" in globals( ):
            assert self is _yp_range_empty, "an empty range should be _yp_range_empty"
    # FIXME When nohtyP has str/repr, use it instead of this faked-out version
    def _yp_str( self ):
        self_len = len( self )
        if self_len < 1: return yp_str( "range(0, 0)" )
        self_start = self[0]._asint()
        if self_len < 2: return yp_str( "range(%d, %d)" % (self_start, self_start+1) )
        self_step = self[1]._asint() - self_start
        self_end = self_start + (self_len * self_step)
        if self_step == 1: return yp_str( "range(%d, %d)" % (self_start, self_end) )
        return yp_str( "range(%d, %d, %d)" % (self_start, self_end, self_step) )
    _yp_repr = _yp_str
_yp_range_empty = yp_range( 0 )

# FIXME integrate this somehow with unittest
#import os
#os.system( "ypExamples.exe" )

