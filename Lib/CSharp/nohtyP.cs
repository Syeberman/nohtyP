#using <mscorlib.dll>
using namespace System::Runtime::InteropServices; 

namespace nohtyP.raw
{
    typedef struct _yp_initialize_kwparams yp_initialize_kwparams;

    [DllImport("nohtyP.dll")]
    ypAPI void yp_initialize( const yp_initialize_kwparams *kwparams );

    typedef struct _ypObject ypObject;

    ypAPI ypObject * const yp_None;

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_incref( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_increfN( int n, ... );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_decref( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_decrefN( int n, ... );

    [DllImport("nohtyP.dll")]
    ypAPI int yp_isexceptionC( ypObject *x );

    typedef signed char         yp_int8_t;
    typedef unsigned char       yp_uint8_t;
    typedef short               yp_int16_t;
    typedef unsigned short      yp_uint16_t;
    #if UINT_MAX == 0xFFFFFFFFu
    typedef int                 yp_int32_t;
    typedef unsigned int        yp_uint32_t;
    #else
    typedef long                yp_int32_t;
    typedef unsigned long       yp_uint32_t;
    #endif
    typedef long long           yp_int64_t;
    typedef unsigned long long  yp_uint64_t;
    typedef float               yp_float32_t;
    typedef double              yp_float64_t;

    // Size- and length-related C types
    #ifdef SSIZE_MAX
    typedef ssize_t     yp_ssize_t;
    #define yp_SSIZE_T_MAX SSIZE_MAX
    #else
    #if SIZE_MAX == 0xFFFFFFFFu
    typedef yp_int32_t  yp_ssize_t;
    #define yp_SSIZE_T_MAX (0x7FFFFFFF)
    #else
    typedef yp_int64_t  yp_ssize_t;
    #define yp_SSIZE_T_MAX (0x7FFFFFFFFFFFFFFFLL)
    #endif
    #endif
    #define yp_SSIZE_T_MIN (-yp_SSIZE_T_MAX - 1)
    typedef yp_ssize_t  yp_hash_t;

    // C types used to represent the numeric objects within nohtyP
    typedef yp_int64_t      yp_int_t;
    #define yp_INT_T_MAX LLONG_MAX
    #define yp_INT_T_MIN LLONG_MIN
    typedef yp_float64_t    yp_float_t;

    typedef ypObject *(*yp_generator_func_t)( ypObject *self, ypObject *value );


    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_intC( yp_int_t value );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_intstoreC( yp_int_t value );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_int_baseC( ypObject *x, yp_int_t base );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_intstore_baseC( ypObject *x, yp_int_t base );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_int( ypObject *x );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_intstore( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_floatCF( yp_float_t value );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_floatstoreCF( yp_float_t value );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_float( ypObject *x );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_floatstore( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_iter( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_generatorCN( yp_generator_func_t func, yp_ssize_t lenhint, int n, ... );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_generatorCNV( yp_generator_func_t func, yp_ssize_t lenhint,
            int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_generator_fromstructCN( yp_generator_func_t func, yp_ssize_t lenhint,
            void *state, yp_ssize_t size, int n, ... );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_generator_fromstructCNV( yp_generator_func_t func, yp_ssize_t lenhint,
            void *state, yp_ssize_t size, int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_rangeC3( yp_int_t start, yp_int_t stop, yp_int_t step );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_rangeC( yp_int_t stop );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_bytesC( const yp_uint8_t *source, yp_ssize_t len );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_bytearrayC( const yp_uint8_t *source, yp_ssize_t len );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_bytes3( ypObject *source, ypObject *encoding, ypObject *errors );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_bytearray3( ypObject *source, ypObject *encoding, ypObject *errors );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_bytes( ypObject *source );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_bytearray( ypObject *source );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_bytes0( void );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_bytearray0( void );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_str_frombytesC4( const yp_uint8_t *source, yp_ssize_t len,
            ypObject *encoding, ypObject *errors );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_chrarray_frombytesC4( const yp_uint8_t *source, yp_ssize_t len,
            ypObject *encoding, ypObject *errors );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_str_frombytesC2( const yp_uint8_t *source, yp_ssize_t len );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_chrarray_frombytesC2( const yp_uint8_t *source, yp_ssize_t len );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_str3( ypObject *object, ypObject *encoding, ypObject *errors );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_chrarray3( ypObject *object, ypObject *encoding, ypObject *errors );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_str( ypObject *object );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_chrarray( ypObject *object );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_str0( void );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_chrarray0( void );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_chrC( yp_int_t i );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_tupleN( int n, ... );
    ypAPI ypObject *yp_tupleNV( int n, va_list args );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_listN( int n, ... );
    ypAPI ypObject *yp_listNV( int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_tuple_repeatCN( yp_ssize_t factor, int n, ... );
    ypAPI ypObject *yp_tuple_repeatCNV( yp_ssize_t factor, int n, va_list args );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_list_repeatCN( yp_ssize_t factor, int n, ... );
    ypAPI ypObject *yp_list_repeatCNV( yp_ssize_t factor, int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_tuple( ypObject *iterable );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_list( ypObject *iterable );

    typedef ypObject *(*yp_sort_key_func_t)( ypObject *x );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_sorted3( ypObject *iterable, yp_sort_key_func_t key, ypObject *reverse );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_sorted( ypObject *iterable );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_frozensetN( int n, ... );
    ypAPI ypObject *yp_frozensetNV( int n, va_list args );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_setN( int n, ... );
    ypAPI ypObject *yp_setNV( int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_frozenset( ypObject *iterable );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_set( ypObject *iterable );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_frozendictK( int n, ... );
    ypAPI ypObject *yp_frozendictKV( int n, va_list args );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_dictK( int n, ... );
    ypAPI ypObject *yp_dictKV( int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_frozendict_fromkeysN( ypObject *value, int n, ... );
    ypAPI ypObject *yp_frozendict_fromkeysNV( ypObject *value, int n, va_list args );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_dict_fromkeysN( ypObject *value, int n, ... );
    ypAPI ypObject *yp_dict_fromkeysNV( ypObject *value, int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_frozendict_fromkeys( ypObject *iterable, ypObject *value );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_dict_fromkeys( ypObject *iterable, ypObject *value );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_frozendict( ypObject *x );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_dict( ypObject *x );

    ypAPI ypObject * const yp_True;
    ypAPI ypObject * const yp_False;

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_bool( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_not( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_or( ypObject *x, ypObject *y );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_orN( int n, ... );
    ypAPI ypObject *yp_orNV( int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_anyN( int n, ... );
    ypAPI ypObject *yp_anyNV( int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_any( ypObject *iterable );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_and( ypObject *x, ypObject *y );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_andN( int n, ... );
    ypAPI ypObject *yp_andNV( int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_allN( int n, ... );
    ypAPI ypObject *yp_allNV( int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_all( ypObject *iterable );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_lt( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_le( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_eq( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_ne( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_ge( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_gt( ypObject *x, ypObject *y );

    [DllImport("nohtyP.dll")]
    ypAPI yp_hash_t yp_hashC( ypObject *x, ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI yp_hash_t yp_currenthashC( ypObject *x, ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_send( ypObject *iterator, ypObject *value );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_next( ypObject *iterator );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_next2( ypObject *iterator, ypObject *defval );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_throw( ypObject *iterator, ypObject *exc );

    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_iter_lenhintC( ypObject *iterator, ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_iter_stateCX( ypObject *iterator, void **state, yp_ssize_t *size );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_close( ypObject **iterator );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_unpackN( ypObject *iterable, int n, ... );

    typedef ypObject *(*yp_filter_function_t)( ypObject *x );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_filter( yp_filter_function_t function, ypObject *iterable );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_filterfalse( yp_filter_function_t function, ypObject *iterable );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_max_keyN( yp_sort_key_func_t key, int n, ... );
    ypAPI ypObject *yp_max_keyNV( yp_sort_key_func_t key, int n, va_list args );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_min_keyN( yp_sort_key_func_t key, int n, ... );
    ypAPI ypObject *yp_min_keyNV( yp_sort_key_func_t key, int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_maxN( int n, ... );
    ypAPI ypObject *yp_maxNV( int n, va_list args );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_minN( int n, ... );
    ypAPI ypObject *yp_minNV( int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_max_key( ypObject *iterable, yp_sort_key_func_t key );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_min_key( ypObject *iterable, yp_sort_key_func_t key );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_max( ypObject *iterable );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_min( ypObject *iterable );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_reversed( ypObject *seq );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_zipN( int n, ... );
    ypAPI ypObject *yp_zipNV( int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_contains( ypObject *container, ypObject *x );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_in( ypObject *x, ypObject *container );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_not_in( ypObject *x, ypObject *container );

    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_lenC( ypObject *container, ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_push( ypObject **container, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_clear( ypObject **container );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_pop( ypObject **container );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_concat( ypObject *sequence, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_repeatC( ypObject *sequence, yp_ssize_t factor );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_getindexC( ypObject *sequence, yp_ssize_t i );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_getsliceC4( ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_getitem( ypObject *sequence, ypObject *key );

    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_findC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
            ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_findC( ypObject *sequence, ypObject *x, ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_indexC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
            ypObject **exc );
    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_indexC( ypObject *sequence, ypObject *x, ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_rfindC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
            ypObject **exc );
    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_rfindC( ypObject *sequence, ypObject *x, ypObject **exc );
    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_rindexC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
            ypObject **exc );
    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_rindexC( ypObject *sequence, ypObject *x, ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_countC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
            ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI yp_ssize_t yp_countC( ypObject *sequence, ypObject *x, ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_setindexC( ypObject **sequence, yp_ssize_t i, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_setsliceC5( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k,
            ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_setitem( ypObject **sequence, ypObject *key, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_delindexC( ypObject **sequence, yp_ssize_t i );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_delsliceC4( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_delitem( ypObject **sequence, ypObject *key );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_append( ypObject **sequence, ypObject *x );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_push( ypObject **sequence, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_extend( ypObject **sequence, ypObject *t );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_irepeatC( ypObject **sequence, yp_ssize_t factor );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_insertC( ypObject **sequence, yp_ssize_t i, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_popindexC( ypObject **sequence, yp_ssize_t i );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_pop( ypObject **sequence );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_remove( ypObject **sequence, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_discard( ypObject **sequence, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_reverse( ypObject **sequence );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_sort3( ypObject **sequence, yp_sort_key_func_t key, ypObject *reverse );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_sort( ypObject **sequence );

    #define yp_SLICE_DEFAULT yp_SSIZE_T_MIN

    #define yp_SLICE_USELEN  yp_SSIZE_T_MAX

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_isdisjoint( ypObject *set, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_issubset( ypObject *set, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_lt( ypObject *set, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_issuperset( ypObject *set, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_gt( ypObject *set, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_unionN( ypObject *set, int n, ... );
    ypAPI ypObject *yp_unionNV( ypObject *set, int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_intersectionN( ypObject *set, int n, ... );
    ypAPI ypObject *yp_intersectionNV( ypObject *set, int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_differenceN( ypObject *set, int n, ... );
    ypAPI ypObject *yp_differenceNV( ypObject *set, int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_symmetric_difference( ypObject *set, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_updateN( ypObject **set, int n, ... );
    ypAPI void yp_updateNV( ypObject **set, int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_intersection_updateN( ypObject **set, int n, ... );
    ypAPI void yp_intersection_updateNV( ypObject **set, int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_difference_updateN( ypObject **set, int n, ... );
    ypAPI void yp_difference_updateNV( ypObject **set, int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_symmetric_difference_update( ypObject **set, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_push( ypObject **set, ypObject *x );
    ypAPI void yp_set_add( ypObject **set, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_pushuniqueE( ypObject *set, ypObject *x, ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_remove( ypObject **set, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_discard( ypObject **set, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_pop( ypObject **set );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_getitem( ypObject *mapping, ypObject *key );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_getdefault( ypObject *mapping, ypObject *key, ypObject *defval );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_iter_items( ypObject *mapping );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_iter_keys( ypObject *mapping );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_iter_values( ypObject *mapping );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_setitem( ypObject **mapping, ypObject *key, ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_delitem( ypObject **mapping, ypObject *key );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_popvalue3( ypObject **mapping, ypObject *key, ypObject *defval );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_popvalue2( ypObject **mapping, ypObject *key );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_popitem( ypObject **mapping, ypObject **key, ypObject **value );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_setdefault( ypObject **mapping, ypObject *key, ypObject *defval );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_updateK( ypObject **mapping, int n, ... );
    ypAPI void yp_updateKV( ypObject **mapping, int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_updateN( ypObject **mapping, int n, ... );
    ypAPI void yp_updateNV( ypObject **mapping, int n, va_list args );

    ypAPI ypObject * const yp_s_ascii;     // "ascii"
    ypAPI ypObject * const yp_s_latin_1;   // "latin_1"
    ypAPI ypObject * const yp_s_utf_32;    // "utf_32"
    ypAPI ypObject * const yp_s_utf_32_be; // "utf_32_be"
    ypAPI ypObject * const yp_s_utf_32_le; // "utf_32_le"
    ypAPI ypObject * const yp_s_utf_16;    // "utf_16"
    ypAPI ypObject * const yp_s_utf_16_be; // "utf_16_be"
    ypAPI ypObject * const yp_s_utf_16_le; // "utf_16_le"
    ypAPI ypObject * const yp_s_utf_8;     // "utf_8"

    ypAPI ypObject * const yp_s_strict;    // "strict"
    ypAPI ypObject * const yp_s_ignore;    // "ignore"
    ypAPI ypObject * const yp_s_replace;   // "replace"

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_add( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_sub( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_mul( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_truediv( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_floordiv( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_mod( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_divmod( ypObject *x, ypObject *y, ypObject **div, ypObject **mod );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_pow( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_pow3( ypObject *x, ypObject *y, ypObject *z );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_neg( ypObject *x );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_pos( ypObject *x );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_abs( ypObject *x );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_lshift( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_rshift( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_amp( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_xor( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_bar( ypObject *x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_invert( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_iadd( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_isub( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_imul( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_itruediv( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_ifloordiv( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_imod( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_ipow( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_ipow3( ypObject **x, ypObject *y, ypObject *z );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_ineg( ypObject **x );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_ipos( ypObject **x );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_iabs( ypObject **x );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_ilshift( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_irshift( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_iamp( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_ixor( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_ibar( ypObject **x, ypObject *y );
    [DllImport("nohtyP.dll")]
    ypAPI void yp_iinvert( ypObject **x );

    ypAPI void yp_iaddC( ypObject **x, yp_int_t y );
    ypAPI void yp_isubC( ypObject **x, yp_int_t y );
    ypAPI void yp_imulC( ypObject **x, yp_int_t y );
    ypAPI void yp_itruedivC( ypObject **x, yp_int_t y );
    ypAPI void yp_ifloordivC( ypObject **x, yp_int_t y );
    ypAPI void yp_imodC( ypObject **x, yp_int_t y );
    ypAPI void yp_ipowC( ypObject **x, yp_int_t y );
    ypAPI void yp_ipowC3( ypObject **x, yp_int_t y, yp_int_t z );
    ypAPI void yp_ilshiftC( ypObject **x, yp_int_t y );
    ypAPI void yp_irshiftC( ypObject **x, yp_int_t y );
    ypAPI void yp_iampC( ypObject **x, yp_int_t y );
    ypAPI void yp_ixorC( ypObject **x, yp_int_t y );
    ypAPI void yp_ibarC( ypObject **x, yp_int_t y );

    ypAPI void yp_iaddCF( ypObject **x, yp_float_t y );
    ypAPI void yp_isubCF( ypObject **x, yp_float_t y );
    ypAPI void yp_imulCF( ypObject **x, yp_float_t y );
    ypAPI void yp_itruedivCF( ypObject **x, yp_float_t y );
    ypAPI void yp_ifloordivCF( ypObject **x, yp_float_t y );
    ypAPI void yp_imodCF( ypObject **x, yp_float_t y );
    ypAPI void yp_ipowCF( ypObject **x, yp_float_t y );

    ypAPI yp_int_t yp_addL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI yp_int_t yp_subL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI yp_int_t yp_mulL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI yp_float_t yp_truedivL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI yp_int_t yp_floordivL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI yp_int_t yp_modL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI void yp_divmodL( yp_int_t x, yp_int_t y, yp_int_t *div, yp_int_t *mod, ypObject **exc );
    ypAPI yp_int_t yp_powL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI yp_int_t yp_powL3( yp_int_t x, yp_int_t y, yp_int_t z, ypObject **exc );
    ypAPI yp_int_t yp_negL( yp_int_t x, ypObject **exc );
    ypAPI yp_int_t yp_posL( yp_int_t x, ypObject **exc );
    ypAPI yp_int_t yp_absL( yp_int_t x, ypObject **exc );
    ypAPI yp_int_t yp_lshiftL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI yp_int_t yp_rshiftL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI yp_int_t yp_ampL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI yp_int_t yp_xorL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI yp_int_t yp_barL( yp_int_t x, yp_int_t y, ypObject **exc );
    ypAPI yp_int_t yp_invertL( yp_int_t x, ypObject **exc );

    ypAPI yp_float_t yp_addLF( yp_float_t x, yp_float_t y, ypObject **exc );
    ypAPI yp_float_t yp_subLF( yp_float_t x, yp_float_t y, ypObject **exc );
    ypAPI yp_float_t yp_mulLF( yp_float_t x, yp_float_t y, ypObject **exc );
    ypAPI yp_float_t yp_truedivLF( yp_float_t x, yp_float_t y, ypObject **exc );
    ypAPI yp_float_t yp_floordivLF( yp_float_t x, yp_float_t y, ypObject **exc );
    ypAPI yp_float_t yp_modLF( yp_float_t x, yp_float_t y, ypObject **exc );
    ypAPI void yp_divmodLF( yp_float_t x, yp_float_t y,
            yp_float_t *div, yp_float_t *mod, ypObject **exc );
    ypAPI yp_float_t yp_powLF( yp_float_t x, yp_float_t y, ypObject **exc );
    ypAPI yp_float_t yp_negLF( yp_float_t x, ypObject **exc );
    ypAPI yp_float_t yp_posLF( yp_float_t x, ypObject **exc );
    ypAPI yp_float_t yp_absLF( yp_float_t x, ypObject **exc );

    ypAPI yp_int_t yp_asintC( ypObject *x, ypObject **exc );
    ypAPI yp_int8_t yp_asint8C( ypObject *x, ypObject **exc );
    ypAPI yp_uint8_t yp_asuint8C( ypObject *x, ypObject **exc );
    ypAPI yp_int16_t yp_asint16C( ypObject *x, ypObject **exc );
    ypAPI yp_uint16_t yp_asuint16C( ypObject *x, ypObject **exc );
    ypAPI yp_int32_t yp_asint32C( ypObject *x, ypObject **exc );
    ypAPI yp_uint32_t yp_asuint32C( ypObject *x, ypObject **exc );
    ypAPI yp_int64_t yp_asint64C( ypObject *x, ypObject **exc );
    ypAPI yp_uint64_t yp_asuint64C( ypObject *x, ypObject **exc );
    ypAPI yp_float_t yp_asfloatC( ypObject *x, ypObject **exc );
    ypAPI yp_float32_t yp_asfloat32C( ypObject *x, ypObject **exc );
    ypAPI yp_float64_t yp_asfloat64C( ypObject *x, ypObject **exc );
    ypAPI yp_ssize_t yp_asssizeC( ypObject *x, ypObject **exc );
    ypAPI yp_hash_t yp_ashashC( ypObject *x, ypObject **exc );
    ypAPI yp_float_t yp_asfloatL( yp_int_t x, ypObject **exc );
    ypAPI yp_int_t yp_asintLF( yp_float_t x, ypObject **exc );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_roundC( ypObject *x, int ndigits );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_sumN( int n, ... );
    ypAPI ypObject *yp_sumNV( int n, va_list args );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_sum( ypObject *iterable );

    [DllImport("nohtyP.dll")]
    ypAPI yp_int_t yp_int_bit_lengthC( ypObject *x, ypObject **exc );

    ypAPI ypObject * const yp_sys_maxint;
    ypAPI ypObject * const yp_sys_minint;

    ypAPI ypObject * const yp_i_neg_one;
    ypAPI ypObject * const yp_i_zero;
    ypAPI ypObject * const yp_i_one;
    ypAPI ypObject * const yp_i_two;

    [DllImport("nohtyP.dll")]
    ypAPI void yp_freeze( ypObject **x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_deepfreeze( ypObject **x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_unfrozen_copy( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_unfrozen_deepcopy( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_frozen_copy( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_frozen_deepcopy( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_copy( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_deepcopy( ypObject *x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_invalidate( ypObject **x );

    [DllImport("nohtyP.dll")]
    ypAPI void yp_deepinvalidate( ypObject **x );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_type( ypObject *object );

    ypAPI ypObject * const yp_type_invalidated;
    ypAPI ypObject * const yp_type_exception;
    ypAPI ypObject * const yp_type_type;
    ypAPI ypObject * const yp_type_NoneType;
    ypAPI ypObject * const yp_type_bool;
    ypAPI ypObject * const yp_type_int;
    ypAPI ypObject * const yp_type_intstore;
    ypAPI ypObject * const yp_type_float;
    ypAPI ypObject * const yp_type_floatstore;
    ypAPI ypObject * const yp_type_iter;
    ypAPI ypObject * const yp_type_bytes;
    ypAPI ypObject * const yp_type_bytearray;
    ypAPI ypObject * const yp_type_str;
    ypAPI ypObject * const yp_type_chrarray;
    ypAPI ypObject * const yp_type_tuple;
    ypAPI ypObject * const yp_type_list;
    ypAPI ypObject * const yp_type_frozenset;
    ypAPI ypObject * const yp_type_set;
    ypAPI ypObject * const yp_type_frozendict;
    ypAPI ypObject * const yp_type_dict;
    ypAPI ypObject * const yp_type_range;

    ypAPI ypObject * const yp_BaseException;
    ypAPI ypObject * const yp_Exception;
    ypAPI ypObject * const yp_StopIteration;
    ypAPI ypObject * const yp_GeneratorExit;
    ypAPI ypObject * const yp_ArithmeticError;
    ypAPI ypObject * const yp_LookupError;
    ypAPI ypObject * const yp_AssertionError;
    ypAPI ypObject * const yp_AttributeError;
    ypAPI ypObject * const yp_EOFError;
    ypAPI ypObject * const yp_FloatingPointError;
    ypAPI ypObject * const yp_OSError;
    ypAPI ypObject * const yp_ImportError;
    ypAPI ypObject * const yp_IndexError;
    ypAPI ypObject * const yp_KeyError;
    ypAPI ypObject * const yp_KeyboardInterrupt;
    ypAPI ypObject * const yp_MemoryError;
    ypAPI ypObject * const yp_NameError;
    ypAPI ypObject * const yp_OverflowError;
    ypAPI ypObject * const yp_RuntimeError;
    ypAPI ypObject * const yp_NotImplementedError;
    ypAPI ypObject * const yp_ReferenceError;
    ypAPI ypObject * const yp_SystemError;
    ypAPI ypObject * const yp_SystemExit;
    ypAPI ypObject * const yp_TypeError;
    ypAPI ypObject * const yp_UnboundLocalError;
    ypAPI ypObject * const yp_UnicodeError;
    ypAPI ypObject * const yp_UnicodeEncodeError;
    ypAPI ypObject * const yp_UnicodeDecodeError;
    ypAPI ypObject * const yp_UnicodeTranslateError;
    ypAPI ypObject * const yp_ValueError;
    ypAPI ypObject * const yp_ZeroDivisionError;
    ypAPI ypObject * const yp_BufferError;

    ypAPI ypObject * const yp_MethodError;
    ypAPI ypObject * const yp_MemorySizeOverflowError;
    ypAPI ypObject * const yp_SystemLimitationError;
    ypAPI ypObject * const yp_InvalidatedError;

    [DllImport("nohtyP.dll")]
    ypAPI int yp_isexceptionC2( ypObject *x, ypObject *exc );

    [DllImport("nohtyP.dll")]
    ypAPI int yp_isexceptionCN( ypObject *x, int n, ... );

    typedef struct _yp_initialize_kwparams {
        yp_ssize_t struct_size;
        void *(*yp_malloc)( yp_ssize_t *actual, yp_ssize_t size );
        void *(*yp_malloc_resize)( yp_ssize_t *actual, void *p, yp_ssize_t size, yp_ssize_t extra );
        void (*yp_free)( void *p );
        int everything_immortal;
    } yp_initialize_kwparams;

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_asbytesCX( ypObject *seq, const yp_uint8_t * *bytes, yp_ssize_t *len );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_asencodedCX( ypObject *seq, const yp_uint8_t * *encoded, yp_ssize_t *size,
            ypObject * *encoding );

    [DllImport("nohtyP.dll")]
    ypAPI ypObject *yp_itemarrayCX( ypObject *seq, ypObject * const * *array, yp_ssize_t *len );


}
