
using System.Runtime.InteropServices;
using ComTypes = System.Runtime.InteropServices.ComTypes;

using ypObject_p = System.UIntPtr;

using yp_int8_t = System.SByte;
using yp_uint8_t = System.Byte;
using yp_int16_t = System.Int16;
using yp_uint16_t = System.UInt16;
using yp_int32_t = System.Int32;
using yp_uint32_t = System.UInt32;
using yp_int64_t = System.Int64;
using yp_uint64_t = System.UInt64;
using yp_float32_t = System.Single;
using yp_float64_t = System.Double;

using yp_ssize_t = System.IntPtr;
using yp_hash_t = System.IntPtr;

using yp_int_t = System.Int64;
using yp_float_t = System.Double;

using yp_generator_func_t = System.UIntPtr;
using yp_sort_key_func_t = System.UIntPtr;
using yp_filter_function_t = System.UIntPtr;
using System;

// TODO Everything here is designed against nohtyP.h, but be sure to go back to Python to ensure
// consistency with the original

// TODO ... or maybe nohtyP.CSharp?
namespace nohtyP.cs
{
    public class yp_int : yp_object
    {
        public yp_int( long value ) :
            base( yp_intC( value ) ) { }
        public yp_int( yp_object x, long @base ) :
            base( yp_int_baseC( x.self, @base ) ) { }
        public yp_int( yp_object x ) :
            base( yp_int( x.self ) ) { }

#if NOTDEFINED
        // TODO Allow casts between yp_object and specific types?
        public static explicit operator yp_int( yp_object x ) {
            if( yp_type( x.self ) != yp_type_int ) throw new ArgumentException; // TODO yp_TypeError?
            return yp_int( x.self );
        }
#endif
    }

    public class yp_intstore : yp_object
    {
        public yp_intstore( long value ) :
            base( yp_intstoreC( value ) ) { }
        public yp_intstore( yp_object x, long @base ) :
            base( yp_intstore_baseC( x.self, @base ) ) { }
        public yp_intstore( yp_object x ) :
            base( yp_intstore( x.self ) ) { }
    }

    public class yp_float : yp_object
    {
        public yp_float( double value ) :
            base( yp_floatCF( value ) ) { }
        public yp_float( yp_object x ) :
            base( yp_float( x.self ) ) { }
    }

    public class yp_floatstore : yp_object
    {
        public yp_floatstore( double value ) :
            base( yp_floatstoreCF( value ) ) { }
        public yp_floatstore( yp_object x ) :
            base( yp_floatstore( x.self ) ) { }
    }

    public class yp_bool : yp_object // TODO yp_int, to mimic Python?
    {
        public yp_bool( yp_object x ) :
            base( yp_bool( x.self ) ) { }
    }

    public class yp_iter : yp_object
    {
        public yp_iter( yp_object x ) :
            base( yp_iter( x.self ) ) { }
        // TODO yp_generatorCN, etc, supporting a callback
    }

    public class yp_range : yp_object
    {
        public yp_range( long start, long stop, long step=1 ) :
            base( yp_rangeC3( start, stop, step ) ) { }
        public yp_range( long stop ) :
            base( yp_rangeC( stop ) ) { }
    }

#if NOTDEFINED
    // TODO How to make this work...
    public class yp_bytes : yp_object
    {
        public unsafe yp_bytes( byte[] source ) :
            base( UIntPtr.Zero ) 
        {
            IntPtr source_ptr = Marshal.AllocHGlobal( source.Length );
            try {
                Marshal.Copy( source, 0, source_ptr, source.Length );
                this.self = yp_bytesC( (yp_uint8_t *) source_ptr, (yp_ssize_t) source.Length );
            } finally {
                Marshal.FreeHGlobal( source_ptr );
            }
        }
    }
#endif

    public unsafe class yp_object
    {
        internal readonly ypObject_p self;

        // Steals the reference to self
        protected yp_object( ypObject_p self )
        {
            this.self = self;
        }

        ~yp_object()
        {
            yp_decref( self );
        }


        #region Imports from nohtyP.dll
        // TODO Remove unused functions/types/etc from this class

        protected const string DLL_NAME = "nohtyP.dll";
        protected const CallingConvention CALLCONV = CallingConvention.Cdecl;

#pragma warning disable 169     // "field not used"
        protected struct yp_initialize_kwparams
        {
            yp_ssize_t struct_size;
            void* yp_malloc;
            void* yp_malloc_resize;
            void* yp_free;
            int everything_immortal;
        };
#pragma warning restore

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_initialize( yp_initialize_kwparams* kwparams );

        //[DllImport("nohtyP.dll")]
        //protected static extern ypObject_p yp_None;

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_incref( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_increfN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_decref( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_decrefN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern int yp_isexceptionC( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_intC( yp_int_t value );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_intstoreC( yp_int_t value );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_int_baseC( ypObject_p x, yp_int_t @base );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_intstore_baseC( ypObject_p x, yp_int_t @base );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_int( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_intstore( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_floatCF( yp_float_t value );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_floatstoreCF( yp_float_t value );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_float( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_floatstore( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_iter( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_generatorCN( yp_generator_func_t func, yp_ssize_t lenhint, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_generator_fromstructCN( yp_generator_func_t func, yp_ssize_t lenhint,
                void* state, yp_ssize_t size, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_rangeC3( yp_int_t start, yp_int_t stop, yp_int_t step );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_rangeC( yp_int_t stop );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_bytesC( yp_uint8_t* source, yp_ssize_t len );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_bytearrayC( yp_uint8_t* source, yp_ssize_t len );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_bytes3( ypObject_p source, ypObject_p encoding, ypObject_p errors );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_bytearray3( ypObject_p source, ypObject_p encoding, ypObject_p errors );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_bytes( ypObject_p source );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_bytearray( ypObject_p source );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_bytes0();
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_bytearray0();

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_str_frombytesC4( yp_uint8_t* source, yp_ssize_t len,
                ypObject_p encoding, ypObject_p errors );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_chrarray_frombytesC4( yp_uint8_t* source, yp_ssize_t len,
                ypObject_p encoding, ypObject_p errors );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_str_frombytesC2( yp_uint8_t* source, yp_ssize_t len );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_chrarray_frombytesC2( yp_uint8_t* source, yp_ssize_t len );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_str3( ypObject_p @object, ypObject_p encoding, ypObject_p errors );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_chrarray3( ypObject_p @object, ypObject_p encoding, ypObject_p errors );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_str( ypObject_p @object );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_chrarray( ypObject_p @object );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_str0();
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_chrarray0();

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_chrC( yp_int_t i );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_tupleN( int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_listN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_tuple_repeatCN( yp_ssize_t factor, int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_list_repeatCN( yp_ssize_t factor, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_tuple( ypObject_p iterable );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_list( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_sorted3( ypObject_p iterable, yp_sort_key_func_t key, ypObject_p reverse );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_sorted( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_frozensetN( int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_setN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_frozenset( ypObject_p iterable );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_set( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_frozendictK( int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_dictK( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_frozendict_fromkeysN( ypObject_p value, int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_dict_fromkeysN( ypObject_p value, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_frozendict_fromkeys( ypObject_p iterable, ypObject_p value );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_dict_fromkeys( ypObject_p iterable, ypObject_p value );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_frozendict( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_dict( ypObject_p x );

        //[DllImport("nohtyP.dll")]
        //protected static extern ypObject_p yp_True;
        //[DllImport("nohtyP.dll")]
        //protected static extern ypObject_p yp_False;

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_bool( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_not( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_or( ypObject_p x, ypObject_p y );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_orN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_anyN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_any( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_and( ypObject_p x, ypObject_p y );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_andN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_allN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_all( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_lt( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_le( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_eq( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_ne( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_ge( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_gt( ypObject_p x, ypObject_p y );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_hash_t yp_hashC( ypObject_p x, ypObject_p* exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_hash_t yp_currenthashC( ypObject_p x, ypObject_p* exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_send( ypObject_p iterator, ypObject_p value );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_next( ypObject_p iterator );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_next2( ypObject_p iterator, ypObject_p defval );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_throw( ypObject_p iterator, ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_iter_lenhintC( ypObject_p iterator, ypObject_p* exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_iter_stateCX( ypObject_p iterator, void** state, yp_ssize_t* size );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_close( ypObject_p* iterator );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_unpackN( ypObject_p iterable, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_filter( yp_filter_function_t function, ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_filterfalse( yp_filter_function_t function, ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_max_keyN( yp_sort_key_func_t key, int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_min_keyN( yp_sort_key_func_t key, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_maxN( int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_minN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_max_key( ypObject_p iterable, yp_sort_key_func_t key );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_min_key( ypObject_p iterable, yp_sort_key_func_t key );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_max( ypObject_p iterable );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_min( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_reversed( ypObject_p seq );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_zipN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_contains( ypObject_p container, ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_in( ypObject_p x, ypObject_p container );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_not_in( ypObject_p x, ypObject_p container );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_lenC( ypObject_p container, ypObject_p* exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_push( ypObject_p* container, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_clear( ypObject_p* container );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_pop( ypObject_p* container );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_concat( ypObject_p sequence, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_repeatC( ypObject_p sequence, yp_ssize_t factor );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_getindexC( ypObject_p sequence, yp_ssize_t i );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_getsliceC4( ypObject_p sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_getitem( ypObject_p sequence, ypObject_p key );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_findC4( ypObject_p sequence, ypObject_p x, yp_ssize_t i, yp_ssize_t j,
                ypObject_p* exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_findC( ypObject_p sequence, ypObject_p x, ypObject_p* exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_indexC4( ypObject_p sequence, ypObject_p x, yp_ssize_t i, yp_ssize_t j,
                ypObject_p* exc );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_indexC( ypObject_p sequence, ypObject_p x, ypObject_p* exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_rfindC4( ypObject_p sequence, ypObject_p x, yp_ssize_t i, yp_ssize_t j,
                ypObject_p* exc );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_rfindC( ypObject_p sequence, ypObject_p x, ypObject_p* exc );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_rindexC4( ypObject_p sequence, ypObject_p x, yp_ssize_t i, yp_ssize_t j,
                ypObject_p* exc );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_rindexC( ypObject_p sequence, ypObject_p x, ypObject_p* exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_countC4( ypObject_p sequence, ypObject_p x, yp_ssize_t i, yp_ssize_t j,
                ypObject_p* exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_ssize_t yp_countC( ypObject_p sequence, ypObject_p x, ypObject_p* exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_setindexC( ypObject_p* sequence, yp_ssize_t i, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_setsliceC5( ypObject_p* sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k,
                ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_setitem( ypObject_p* sequence, ypObject_p key, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_delindexC( ypObject_p* sequence, yp_ssize_t i );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_delsliceC4( ypObject_p* sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_delitem( ypObject_p* sequence, ypObject_p key );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_append( ypObject_p* sequence, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_extend( ypObject_p* sequence, ypObject_p t );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_irepeatC( ypObject_p* sequence, yp_ssize_t factor );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_insertC( ypObject_p* sequence, yp_ssize_t i, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_popindexC( ypObject_p* sequence, yp_ssize_t i );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_remove( ypObject_p* sequence, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_discard( ypObject_p* sequence, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_reverse( ypObject_p* sequence );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_sort3( ypObject_p* sequence, yp_sort_key_func_t key, ypObject_p reverse );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_sort( ypObject_p* sequence );

        //protected static const yp_ssize_t yp_SLICE_DEFAULT = System.UIntPtr.MinValue;
        //protected static const yp_ssize_t yp_SLICE_USELEN = System.UIntPtr.MaxValue;

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_isdisjoint( ypObject_p set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_issubset( ypObject_p set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_issuperset( ypObject_p set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_unionN( ypObject_p set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_intersectionN( ypObject_p set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_differenceN( ypObject_p set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_symmetric_difference( ypObject_p set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_updateN( ypObject_p* set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_intersection_updateN( ypObject_p* set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_difference_updateN( ypObject_p* set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_symmetric_difference_update( ypObject_p* set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_set_add( ypObject_p* set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_pushuniqueE( ypObject_p set, ypObject_p x, ypObject_p* exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_getdefault( ypObject_p mapping, ypObject_p key, ypObject_p defval );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_iter_items( ypObject_p mapping );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_iter_keys( ypObject_p mapping );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_iter_values( ypObject_p mapping );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_popvalue3( ypObject_p* mapping, ypObject_p key, ypObject_p defval );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_popvalue2( ypObject_p* mapping, ypObject_p key );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_popitem( ypObject_p* mapping, ypObject_p* key, ypObject_p* value );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_setdefault( ypObject_p* mapping, ypObject_p key, ypObject_p defval );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_updateK( ypObject_p* mapping, int n, params ypObject_p[] args );

#if NOTDEFINED
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_ascii;     // "ascii"
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_latin_1;   // "latin_1"
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_utf_32;    // "utf_32"
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_utf_32_be; // "utf_32_be"
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_utf_32_le; // "utf_32_le"
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_utf_16;    // "utf_16"
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_utf_16_be; // "utf_16_be"
        [DllImport("nohtyP.dll")]
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_utf_16_le; // "utf_16_le"
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_utf_8;     // "utf_8"

        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_strict;    // "strict"
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_ignore;    // "ignore"
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_s_replace;   // "replace"
#endif

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_add( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_sub( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_mul( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_truediv( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_floordiv( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_mod( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_divmod( ypObject_p x, ypObject_p y, ypObject_p* div, ypObject_p* mod );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_pow( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_pow3( ypObject_p x, ypObject_p y, ypObject_p z );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_neg( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_pos( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_abs( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_lshift( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_rshift( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_amp( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_xor( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_bar( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_invert( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_iadd( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_isub( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_imul( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_itruediv( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_ifloordiv( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_imod( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_ipow( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_ipow3( ypObject_p* x, ypObject_p y, ypObject_p z );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_ineg( ypObject_p* x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_ipos( ypObject_p* x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_iabs( ypObject_p* x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_ilshift( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_irshift( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_iamp( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_ixor( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_ibar( ypObject_p* x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_iinvert( ypObject_p* x );

#if NOTDEFINED
        protected static extern void yp_iaddC( ypObject_p *x, yp_int_t y );
        protected static extern void yp_isubC( ypObject_p *x, yp_int_t y );
        protected static extern void yp_imulC( ypObject_p *x, yp_int_t y );
        protected static extern void yp_itruedivC( ypObject_p *x, yp_int_t y );
        protected static extern void yp_ifloordivC( ypObject_p *x, yp_int_t y );
        protected static extern void yp_imodC( ypObject_p *x, yp_int_t y );
        protected static extern void yp_ipowC( ypObject_p *x, yp_int_t y );
        protected static extern void yp_ipowC3( ypObject_p *x, yp_int_t y, yp_int_t z );
        protected static extern void yp_ilshiftC( ypObject_p *x, yp_int_t y );
        protected static extern void yp_irshiftC( ypObject_p *x, yp_int_t y );
        protected static extern void yp_iampC( ypObject_p *x, yp_int_t y );
        protected static extern void yp_ixorC( ypObject_p *x, yp_int_t y );
        protected static extern void yp_ibarC( ypObject_p *x, yp_int_t y );

        protected static extern void yp_iaddCF( ypObject_p *x, yp_float_t y );
        protected static extern void yp_isubCF( ypObject_p *x, yp_float_t y );
        protected static extern void yp_imulCF( ypObject_p *x, yp_float_t y );
        protected static extern void yp_itruedivCF( ypObject_p *x, yp_float_t y );
        protected static extern void yp_ifloordivCF( ypObject_p *x, yp_float_t y );
        protected static extern void yp_imodCF( ypObject_p *x, yp_float_t y );
        protected static extern void yp_ipowCF( ypObject_p *x, yp_float_t y );

        protected static extern yp_int_t yp_addL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern yp_int_t yp_subL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern yp_int_t yp_mulL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern yp_float_t yp_truedivL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern yp_int_t yp_floordivL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern yp_int_t yp_modL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern void yp_divmodL( yp_int_t x, yp_int_t y, yp_int_t *div, yp_int_t *mod, ypObject_p *exc );
        protected static extern yp_int_t yp_powL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern yp_int_t yp_powL3( yp_int_t x, yp_int_t y, yp_int_t z, ypObject_p *exc );
        protected static extern yp_int_t yp_negL( yp_int_t x, ypObject_p *exc );
        protected static extern yp_int_t yp_posL( yp_int_t x, ypObject_p *exc );
        protected static extern yp_int_t yp_absL( yp_int_t x, ypObject_p *exc );
        protected static extern yp_int_t yp_lshiftL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern yp_int_t yp_rshiftL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern yp_int_t yp_ampL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern yp_int_t yp_xorL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern yp_int_t yp_barL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        protected static extern yp_int_t yp_invertL( yp_int_t x, ypObject_p *exc );

        protected static extern yp_float_t yp_addLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        protected static extern yp_float_t yp_subLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        protected static extern yp_float_t yp_mulLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        protected static extern yp_float_t yp_truedivLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        protected static extern yp_float_t yp_floordivLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        protected static extern yp_float_t yp_modLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        protected static extern void yp_divmodLF( yp_float_t x, yp_float_t y,
                yp_float_t *div, yp_float_t *mod, ypObject_p *exc );
        protected static extern yp_float_t yp_powLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        protected static extern yp_float_t yp_negLF( yp_float_t x, ypObject_p *exc );
        protected static extern yp_float_t yp_posLF( yp_float_t x, ypObject_p *exc );
        protected static extern yp_float_t yp_absLF( yp_float_t x, ypObject_p *exc );

        protected static extern yp_int_t yp_asintC( ypObject_p x, ypObject_p *exc );
        protected static extern yp_int8_t yp_asint8C( ypObject_p x, ypObject_p *exc );
        protected static extern yp_uint8_t yp_asuint8C( ypObject_p x, ypObject_p *exc );
        protected static extern yp_int16_t yp_asint16C( ypObject_p x, ypObject_p *exc );
        protected static extern yp_uint16_t yp_asuint16C( ypObject_p x, ypObject_p *exc );
        protected static extern yp_int32_t yp_asint32C( ypObject_p x, ypObject_p *exc );
        protected static extern yp_uint32_t yp_asuint32C( ypObject_p x, ypObject_p *exc );
        protected static extern yp_int64_t yp_asint64C( ypObject_p x, ypObject_p *exc );
        protected static extern yp_uint64_t yp_asuint64C( ypObject_p x, ypObject_p *exc );
        protected static extern yp_float_t yp_asfloatC( ypObject_p x, ypObject_p *exc );
        protected static extern yp_float32_t yp_asfloat32C( ypObject_p x, ypObject_p *exc );
        protected static extern yp_float64_t yp_asfloat64C( ypObject_p x, ypObject_p *exc );
        protected static extern yp_ssize_t yp_asssizeC( ypObject_p x, ypObject_p *exc );
        protected static extern yp_hash_t yp_ashashC( ypObject_p x, ypObject_p *exc );
        protected static extern yp_float_t yp_asfloatL( yp_int_t x, ypObject_p *exc );
        protected static extern yp_int_t yp_asintLF( yp_float_t x, ypObject_p *exc );
#endif

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_roundC( ypObject_p x, int ndigits );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_sumN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_sum( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern yp_int_t yp_int_bit_lengthC( ypObject_p x, ypObject_p* exc );

#if NOTEDEFINED
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_sys_maxint;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_sys_minint;

        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_i_neg_one;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_i_zero;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_i_one;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_i_two;
#endif

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_freeze( ypObject_p* x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_deepfreeze( ypObject_p* x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_unfrozen_copy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_unfrozen_deepcopy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_frozen_copy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_frozen_deepcopy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_copy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_deepcopy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_invalidate( ypObject_p* x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern void yp_deepinvalidate( ypObject_p* x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_type( ypObject_p @object );

#if NOTDEFINED
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_invalidated;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_exception;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_type;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_NoneType;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_bool;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_int;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_intstore;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_float;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_floatstore;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_iter;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_bytes;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_bytearray;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_str;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_chrarray;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_tuple;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_list;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_frozenset;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_set;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_frozendict;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_dict;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_type_range;

        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_BaseException;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_Exception;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_StopIteration;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_GeneratorExit;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_ArithmeticError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_LookupError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_AssertionError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_AttributeError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_EOFError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_FloatingPointError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_OSError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_ImportError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_IndexError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_KeyError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_KeyboardInterrupt;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_MemoryError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_NameError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_OverflowError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_RuntimeError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_NotImplementedError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_ReferenceError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_SystemError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_SystemExit;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_TypeError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_UnboundLocalError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_UnicodeError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_UnicodeEncodeError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_UnicodeDecodeError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_UnicodeTranslateError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_ValueError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_ZeroDivisionError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_BufferError;

        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_MethodError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_MemorySizeOverflowError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_SystemLimitationError;
        [DllImport("nohtyP.dll")]
        protected static extern ypObject_p yp_InvalidatedError;
#endif

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern int yp_isexceptionC2( ypObject_p x, ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern int yp_isexceptionCN( ypObject_p x, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_asbytesCX( ypObject_p seq, yp_uint8_t** bytes, yp_ssize_t* len );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_asencodedCX( ypObject_p seq, yp_uint8_t** encoded, yp_ssize_t* size,
                ypObject_p* encoding );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        protected static extern ypObject_p yp_itemarrayCX( ypObject_p seq, ypObject_p** array, yp_ssize_t* len );
        #endregion

        static void Main()
        {
            var x = new yp_int( 25 );
            Console.WriteLine( x );
            Console.WriteLine( "Press a key..." );
            Console.ReadKey( true );
        }

    }


}

