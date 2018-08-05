
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
using System.Text;
using System.Collections.Generic;
using System.Diagnostics;

// TODO Everything here is designed against nohtyP.h, but be sure to go back to Python to ensure
// consistency with the original

// TODO ... or maybe nohtyP.CSharp?
namespace nohtyP
{
    public static class yp
    {
        public static unsafe void initialize() {
            _dll.yp_initialize( null );
        }

        public static readonly @object None = new @object( _dll.yp_None );

        public static @object @int( long value=0 ) {
            return new @object( _dll.yp_intC( value ) );
        }
        public static @object @int( @object x, long @base ) {
            return new @object( _dll.yp_int_baseC( x.self, @base ) );
        }
        public static @object @int( @object x ) {
            return new @object( _dll.yp_int( x.self ) );
        }

        public static @object intstore( long value=0 ) {
            return new @object( _dll.yp_intstoreC( value ) );
        }
        public static @object intstore( @object x, long @base ) {
            return new @object( _dll.yp_intstore_baseC( x.self, @base ) );
        }
        public static @object intstore( @object x ) {
            return new @object( _dll.yp_intstore( x.self ) );
        }

        public static @object @float( double value=0.0 ) {
            return new @object( _dll.yp_floatCF( value ) );
        }
        public static @object @float( @object x ) {
            return new @object( _dll.yp_float( x.self ) );
        }

        public static @object floatstore( double value=0.0 ) {
            return new @object( _dll.yp_floatstoreCF( value ) );
        }
        public static @object floatstore( @object x ) {
            return new @object( _dll.yp_floatstore( x.self ) );
        }

        public static @object iter( @object x ) {
            return new @object( _dll.yp_iter( x.self ) );
        }
        // TODO yp_generatorCN, etc, supporting a callback

        public static @object range( long start, long stop, long step=1 ) {
            return new @object( _dll.yp_rangeC3( start, stop, step ) );
        }
        public static @object range( long stop ) {
            return new @object( _dll.yp_rangeC( stop ) );
        }

        public static @object bytes( long source ) {
            return new @object( _dll.yp_bytesC( null, (yp_ssize_t) source ) );
        }
        public static @object bytes( byte[] source ) {
            return new @object( _dll.yp_bytesC( source, (yp_ssize_t) source.Length ) );
        }
        public static @object bytes( @object source, @object encoding, @object errors ) {
            return new @object( _dll.yp_bytes3( source.self, encoding.self, errors.self ) );
        }
        public static @object bytes( @object source, @object encoding ) {
            return bytes( source, encoding, s_strict );
        }
        public static @object bytes( @object source ) {
            return new @object( _dll.yp_bytes( source.self ) );
        }
        public static @object bytes() {
            return new @object( _dll.yp_bytesC( null, (yp_ssize_t) 0 ) ); // TODO yp_bytes0
        }

        public static @object bytearray( long source ) {
            return new @object( _dll.yp_bytearrayC( null, (yp_ssize_t) source ) );
        }
        public static @object bytearray( byte[] source ) {
            return new @object( _dll.yp_bytearrayC( source, (yp_ssize_t) source.Length ) );
        }
        public static @object bytearray( @object source, @object encoding, @object errors ) {
            return new @object( _dll.yp_bytearray3( source.self, encoding.self, errors.self ) );
        }
        public static @object bytearray( @object source, @object encoding ) {
            return bytearray( source, encoding, s_strict );
        }
        public static @object bytearray( @object source ) {
            return new @object( _dll.yp_bytearray( source.self ) );
        }
        public static @object bytearray() {
            return new @object( _dll.yp_bytearrayC( null, (yp_ssize_t) 0 ) ); // TODO yp_bytearray0
        }

        private static ypObject_p _str( string source ) {
            var encoded = Encoding.UTF8.GetBytes( source );
            return _dll.yp_str_frombytesC2( encoded, (yp_ssize_t) encoded.Length );
        }
        public static @object str( string source ) {
            return new @object( _str( source ) );
        }
        // TODO default values? Perhaps just for errors
        public static @object str( byte[] source, @object encoding, @object errors ) {
            return new @object( _dll.yp_str_frombytesC4( source, (yp_ssize_t) source.Length,
                encoding.self, errors.self ) );
        }
        public static @object str( @object @object, @object encoding, @object errors ) {
            return new @object( _dll.yp_str3( @object.self, encoding.self, errors.self ) );
        }
        public static @object str( @object @object, @object encoding ) {
            return str( @object, encoding, s_strict );
        }
        public static @object str( @object @object ) {
            return new @object( _dll.yp_str( @object.self ) );
        }
        public static @object str() {
            return new @object( _dll.yp_str0() );
        }

        public static @object chr( long i ) {
            return new @object( _dll.yp_chrC( i ) );
        }

        private static ypObject_p _chrarray( string source ) {
            var encoded = Encoding.UTF8.GetBytes( source );
            return _dll.yp_str_frombytesC2( encoded, (yp_ssize_t) encoded.Length );
        }
        public static @object chrarray( string source ) {
            return new @object( _chrarray( source ) );
        }
        // TODO default values? Perhaps just for errors
        public static @object chrarray( byte[] source, @object encoding, @object errors ) {
            return new @object( _dll.yp_chrarray_frombytesC4( source, (yp_ssize_t) source.Length,
                encoding.self, errors.self ) );
        }
        public static @object chrarray( @object @object, @object encoding, @object errors ) {
            return new @object( _dll.yp_chrarray3( @object.self, encoding.self, errors.self ) );
        }
        public static @object chrarray( @object @object, @object encoding ) {
            return chrarray( @object, encoding, s_strict );
        }
        public static @object chrarray( @object @object ) {
            return new @object( _dll.yp_chrarray( @object.self ) );
        }
        public static @object chrarray() {
            return new @object( _dll.yp_chrarray0() );
        }

        // TODO Or would a yp_iter wrapper for IEnumerable work better?  (With a good length_hint.)
        private static ypObject_p _tuple( IList<@object> objects ) {
            var result = _list( objects );
            _dll.yp_freeze( ref result );   // convert to tuple
            return result;  // or exception on error
        }
        public static @object tuple( IList<@object> objects ) {
            return new @object( _tuple( objects ) );
        }
        public static @object tuple( @object iterable ) {
            return new @object( _dll.yp_tuple( iterable.self ) );
        }
        public static @object tuple() {
            return new @object( _dll.yp_tupleN( 0 ) );
        }

        // TODO Or would a yp_iter wrapper for IEnumerable work better?  (With a good length_hint.)
        private static ypObject_p _list( IList<@object> objects )
        {
            // Pre-allocate a list of the required size, then add the items.  If Count lies we
            // will get an exception.  C# lists max-out at 2^32 elements.
            int count = objects.Count;
            var result = _dll.yp_list_repeatCN( (yp_ssize_t) count, 1, yp.None.self );
            for( int i = 0; i < count; i += 1 ) {
                _dll.yp_setindexC( ref result, (yp_ssize_t) i, objects[i].self );
            }
            return result;  // or exception on error
        }
        public static @object list( IList<@object> objects ) {
            return new @object( _list( objects ) );
        }
        public static @object list( @object iterable ) {
            return new @object( _dll.yp_list( iterable.self ) );
        }
        public static @object list() {
            return new @object( _dll.yp_listN( 0 ) );
        }

        public static @object sorted( @object iterable, Func<@object, @object> key=null, bool reverse=false ) {
            return yp.sorted( iterable, key, yp.@bool( reverse ) );
        }
        public static @object sorted( @object iterable, Func<@object, @object> key=null, @object reverse=null ) {
            // FIXME This could be a good way to implement default @object values...but then
            // we hide possible null exceptions (if that's even important)
            if( reverse == null ) reverse = yp.False;
            return new @object( _dll.yp_sorted3( iterable.self, UIntPtr.Zero, reverse.self ) );
        }

        // TODO Expose yp_setN et al as a C# method that accepts variable arguments?  Python doesn't do
        // this, but it can simplify yp.set(yp.tuple(...)) into just yp.setN(...)

        // TODO Or would a yp_iter wrapper for IEnumerable work better?  (With a good length_hint.)
        private static ypObject_p _frozenset( IEnumerable<@object> objects )
        {
            var result = _set( objects );
            _dll.yp_freeze( ref result );   // convert to frozenset
            return result;  // or exception on error
        }
        public static @object frozenset( IEnumerable<@object> objects ) {
            return new @object( _frozenset( objects ) );
        }
        public static @object frozenset( @object iterable ) {
            return new @object( _dll.yp_frozenset( iterable.self ) );
        }
        public static @object frozenset() {
            return new @object( _dll.yp_frozensetN( 0 ) );
        }

        // TODO Or would a yp_iter wrapper for IEnumerable work better?  (With a good length_hint.)
        // TODO Really do need a good length_hint...or a "yp_set_fromlength_hint" function
        private static ypObject_p _set( IEnumerable<@object> objects )
        {
            // TODO We should immediately wrap this (and others) in @object to take advantage of
            // auto-decref on exception
            var result = _dll.yp_setN( 0 );
            foreach( var item in objects ) {
                _dll.yp_set_add( ref result, item.self );
            }
            return result;  // or exception on error
        }
        public static @object set( IEnumerable<@object> objects ) {
            return new @object( _set( objects ) );
        }
        public static @object set( @object iterable ) {
            return new @object( _dll.yp_set( iterable.self ) );
        }
        public static @object set() {
            return new @object( _dll.yp_setN( 0 ) );
        }

        // TODO Or would a yp_iter wrapper for IEnumerable work better?  (With a good length_hint.)
        private static ypObject_p _frozendict( IEnumerable<IList<@object>> objects )
        {
            var result = _dict( objects );
            _dll.yp_freeze( ref result );   // convert to frozendict
            return result;  // or exception on error
        }
        public static @object frozendict( IEnumerable<IList<@object>> objects ) {
            return new @object( _frozendict( objects ) );
        }
        public static @object frozendict( @object iterable ) {
            return new @object( _dll.yp_frozendict( iterable.self ) );
        }
        public static @object frozendict() {
            return new @object( _dll.yp_frozendictK( 0 ) );
        }

        // TODO Or would a yp_iter wrapper for IEnumerable work better?  (With a good length_hint.)
        // TODO Another version that takes IEnumerable<@object>, where the @objects are supposedly
        // already 2-tuples or similar
        private static ypObject_p _dict( IEnumerable<IList<@object>> objects )
        {
            // TODO We should immediately wrap this (and others) in @object to take advantage of
            // auto-decref on exception
            var result = _dll.yp_dictK( 0 );
            foreach( var item in objects ) {
                if( item.Count != 2 ) throw new IndexOutOfRangeException(); // FIXME yp_ValueError
                _dll.yp_setitem( ref result, item[0].self, item[1].self );
            }
            return result;  // or exception on error
        }
        public static @object dict( IEnumerable<IList<@object>> objects ) {
            return new @object( _dict( objects ) );
        }
        public static @object dict( @object iterable ) {
            return new @object( _dll.yp_dict( iterable.self ) );
        }
        public static @object dict() {
            return new @object( _dll.yp_dictK( 0 ) );
        }


        // TODO implement yp_bool0?
        public static readonly @object False = new @object( _dll.yp_False );
        public static readonly @object True = new @object( _dll.yp_True );

        public static @object @bool( bool value=false ) {
            return value ? yp.True : yp.False;
        }
        public static @object @bool( @object x ) {
            return new @object( _dll.yp_bool( x.self ) );
        }


        public static readonly @object s_ascii = new @object( _dll.yp_s_ascii );
        public static readonly @object s_latin_1 = new @object( _dll.yp_s_latin_1 );
        public static readonly @object s_utf_32 = new @object( _dll.yp_s_utf_32 );
        public static readonly @object s_utf_32_be = new @object( _dll.yp_s_utf_32_be );
        public static readonly @object s_utf_32_le = new @object( _dll.yp_s_utf_32_le );
        public static readonly @object s_utf_16 = new @object( _dll.yp_s_utf_16 );
        public static readonly @object s_utf_16_be = new @object( _dll.yp_s_utf_16_be );
        public static readonly @object s_utf_16_le = new @object( _dll.yp_s_utf_16_le );
        public static readonly @object s_utf_8 = new @object( _dll.yp_s_utf_8 );

        public static readonly @object s_strict = new @object( _dll.yp_s_strict );
        public static readonly @object s_ignore = new @object( _dll.yp_s_ignore );
        public static readonly @object s_replace = new @object( _dll.yp_s_replace );


        // TODO Because the objects can be frozen underneath, it's misleading that in C# we'll have
        // a yp_list object, say, but underneath it's been frozen and is now a tuple.  Can we
        // transmute in C#, or should we dispense with C# subclasses and only use @object?
        public class @object
        {
            internal readonly ypObject_p self;

            // Steals the reference to self
            internal @object( ypObject_p self ) {
                this.self = self;
            }

            ~@object() {
                _dll.yp_decref( self );
            }

#if NOTDEFINED
        // TODO Allow casts between @object and specific types?
        public static explicit operator yp_int( @object x ) {
            if( yp_type( x.self ) != yp_type_int ) throw new ArgumentException; // TODO yp_TypeError?
            return yp_int( x.self );
        }
#endif
        }
    }

    /// <summary>
    /// Imports from nohtyP.dll
    /// </summary>
    internal unsafe static class _dll {

        // TODO Remove unused functions/types/etc from this class

        internal const string DLL_NAME = "nohtyP.dll";
        internal const CallingConvention CALLCONV = CallingConvention.Cdecl;


        [DllImport( "kernel32.dll" )]
        private static extern UIntPtr LoadLibrary( string dllToLoad );
        [DllImport( "kernel32.dll" )]
        private static extern UIntPtr GetProcAddress( UIntPtr hModule, string procedureName );
        [DllImport( "kernel32.dll" )]
        private static extern bool FreeLibrary( UIntPtr hModule );
        // XXX Why isn't this available directly in C#?
        private static UIntPtr DllImportData( string name ) {
            UIntPtr library = LoadLibrary( DLL_NAME ); // FIXME free this
            // FIXME pick a better exception
            if( library == UIntPtr.Zero ) throw new ArgumentNullException();
            var data = GetProcAddress( library, name );
            FreeLibrary( library ); // ignore errors
            // FIXME pick a better exception
            if( data == UIntPtr.Zero ) throw new ArgumentNullException();
            return data;
        }


#pragma warning disable 169     // "field not used"
        internal struct yp_initialize_kwparams
        {
            yp_ssize_t struct_size;
            void* yp_malloc;
            void* yp_malloc_resize;
            void* yp_free;
            int everything_immortal;
        };
#pragma warning restore

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_initialize( yp_initialize_kwparams* kwparams );

        internal static ypObject_p yp_None = DllImportData( "yp_None" );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_incref( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_increfN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_decref( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_decrefN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern int yp_isexceptionC( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_intC( yp_int_t value );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_intstoreC( yp_int_t value );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_int_baseC( ypObject_p x, yp_int_t @base );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_intstore_baseC( ypObject_p x, yp_int_t @base );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_int( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_intstore( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_floatCF( yp_float_t value );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_floatstoreCF( yp_float_t value );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_float( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_floatstore( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_iter( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_generatorCN( yp_generator_func_t func, yp_ssize_t length_hint, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_generator_fromstructCN( yp_generator_func_t func, yp_ssize_t length_hint,
                void* state, yp_ssize_t size, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_rangeC3( yp_int_t start, yp_int_t stop, yp_int_t step );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_rangeC( yp_int_t stop );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_bytesC( yp_uint8_t[] source, yp_ssize_t len );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_bytearrayC( yp_uint8_t[] source, yp_ssize_t len );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_bytes3( ypObject_p source, ypObject_p encoding, ypObject_p errors );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_bytearray3( ypObject_p source, ypObject_p encoding, ypObject_p errors );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_bytes( ypObject_p source );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_bytearray( ypObject_p source );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_bytes0();
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_bytearray0();

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_str_frombytesC4( yp_uint8_t[] source, yp_ssize_t len,
                ypObject_p encoding, ypObject_p errors );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_chrarray_frombytesC4( yp_uint8_t[] source, yp_ssize_t len,
                ypObject_p encoding, ypObject_p errors );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_str_frombytesC2( yp_uint8_t[] source, yp_ssize_t len );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_chrarray_frombytesC2( yp_uint8_t[] source, yp_ssize_t len );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_str3( ypObject_p @object, ypObject_p encoding, ypObject_p errors );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_chrarray3( ypObject_p @object, ypObject_p encoding, ypObject_p errors );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_str( ypObject_p @object );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_chrarray( ypObject_p @object );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_str0();
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_chrarray0();

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_chrC( yp_int_t i );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_tupleN( int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_listN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_tuple_repeatCN( yp_ssize_t factor, int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_list_repeatCN( yp_ssize_t factor, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_tuple( ypObject_p iterable );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_list( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_sorted3( ypObject_p iterable, yp_sort_key_func_t key, ypObject_p reverse );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_sorted( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_frozensetN( int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_setN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_frozenset( ypObject_p iterable );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_set( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_frozendictK( int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_dictK( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_frozendict_fromkeysN( ypObject_p value, int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_dict_fromkeysN( ypObject_p value, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_frozendict_fromkeys( ypObject_p iterable, ypObject_p value );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_dict_fromkeys( ypObject_p iterable, ypObject_p value );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_frozendict( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_dict( ypObject_p x );

        internal static ypObject_p yp_True = DllImportData( "yp_True" );
        internal static ypObject_p yp_False = DllImportData( "yp_False" );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_bool( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_not( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_or( ypObject_p x, ypObject_p y );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_orN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_anyN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_any( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_and( ypObject_p x, ypObject_p y );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_andN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_allN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_all( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_lt( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_le( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_eq( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_ne( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_ge( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_gt( ypObject_p x, ypObject_p y );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_hash_t yp_hashC( ypObject_p x, ref ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_hash_t yp_currenthashC( ypObject_p x, ref ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_send( ypObject_p iterator, ypObject_p value );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_next( ypObject_p iterator );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_next2( ypObject_p iterator, ypObject_p defval );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_throw( ypObject_p iterator, ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_iter_length_hintC( ypObject_p iterator, ref ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_iter_stateCX( ypObject_p iterator, out UIntPtr state, out yp_ssize_t size );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_close( ref ypObject_p iterator );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_unpackN( ypObject_p iterable, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_filter( yp_filter_function_t function, ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_filterfalse( yp_filter_function_t function, ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_max_keyN( yp_sort_key_func_t key, int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_min_keyN( yp_sort_key_func_t key, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_maxN( int n, params ypObject_p[] args );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_minN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_max_key( ypObject_p iterable, yp_sort_key_func_t key );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_min_key( ypObject_p iterable, yp_sort_key_func_t key );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_max( ypObject_p iterable );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_min( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_reversed( ypObject_p seq );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_zipN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_contains( ypObject_p container, ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_in( ypObject_p x, ypObject_p container );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_not_in( ypObject_p x, ypObject_p container );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_lenC( ypObject_p container, ref ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_push( ref ypObject_p container, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_clear( ref ypObject_p container );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_pop( ref ypObject_p container );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_concat( ypObject_p sequence, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_repeatC( ypObject_p sequence, yp_ssize_t factor );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_getindexC( ypObject_p sequence, yp_ssize_t i );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_getsliceC4( ypObject_p sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_getitem( ypObject_p sequence, ypObject_p key );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_findC4( ypObject_p sequence, ypObject_p x, yp_ssize_t i, yp_ssize_t j,
                ref ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_findC( ypObject_p sequence, ypObject_p x, ref ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_indexC4( ypObject_p sequence, ypObject_p x, yp_ssize_t i, yp_ssize_t j,
                ref ypObject_p exc );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_indexC( ypObject_p sequence, ypObject_p x, ref ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_rfindC4( ypObject_p sequence, ypObject_p x, yp_ssize_t i, yp_ssize_t j,
                ref ypObject_p exc );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_rfindC( ypObject_p sequence, ypObject_p x, ref ypObject_p exc );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_rindexC4( ypObject_p sequence, ypObject_p x, yp_ssize_t i, yp_ssize_t j,
                ref ypObject_p exc );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_rindexC( ypObject_p sequence, ypObject_p x, ref ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_countC4( ypObject_p sequence, ypObject_p x, yp_ssize_t i, yp_ssize_t j,
                ref ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_ssize_t yp_countC( ypObject_p sequence, ypObject_p x, ref ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_setindexC( ref ypObject_p sequence, yp_ssize_t i, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_setsliceC5( ref ypObject_p sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k,
                ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_setitem( ref ypObject_p sequence, ypObject_p key, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_delindexC( ref ypObject_p sequence, yp_ssize_t i );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_delsliceC4( ref ypObject_p sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_delitem( ref ypObject_p sequence, ypObject_p key );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_append( ref ypObject_p sequence, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_extend( ref ypObject_p sequence, ypObject_p t );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_irepeatC( ref ypObject_p sequence, yp_ssize_t factor );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_insertC( ref ypObject_p sequence, yp_ssize_t i, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_popindexC( ref ypObject_p sequence, yp_ssize_t i );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_remove( ref ypObject_p sequence, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_discard( ref ypObject_p sequence, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_reverse( ref ypObject_p sequence );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_sort3( ref ypObject_p sequence, yp_sort_key_func_t key, ypObject_p reverse );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_sort( ref ypObject_p sequence );

        //internal static const yp_ssize_t yp_SLICE_DEFAULT = yp_ssize_t.MinValue;
        //internal static const yp_ssize_t yp_SLICE_USELEN = yp_ssize_t.MaxValue;

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_isdisjoint( ypObject_p set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_issubset( ypObject_p set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_issuperset( ypObject_p set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_unionN( ypObject_p set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_intersectionN( ypObject_p set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_differenceN( ypObject_p set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_symmetric_difference( ypObject_p set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_updateN( ref ypObject_p set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_intersection_updateN( ref ypObject_p set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_difference_updateN( ref ypObject_p set, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_symmetric_difference_update( ref ypObject_p set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_set_add( ref ypObject_p set, ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_pushuniqueE( ypObject_p set, ypObject_p x, ref ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_getdefault( ypObject_p mapping, ypObject_p key, ypObject_p defval );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_iter_items( ypObject_p mapping );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_iter_keys( ypObject_p mapping );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_iter_values( ypObject_p mapping );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_popvalue3( ref ypObject_p mapping, ypObject_p key, ypObject_p defval );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_popvalue2( ref ypObject_p mapping, ypObject_p key );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_popitem( ref ypObject_p mapping, ref ypObject_p key, ref ypObject_p value );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_setdefault( ref ypObject_p mapping, ypObject_p key, ypObject_p defval );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_updateK( ref ypObject_p mapping, int n, params ypObject_p[] args );

        internal static ypObject_p yp_s_ascii = DllImportData( "yp_s_ascii" );
        internal static ypObject_p yp_s_latin_1 = DllImportData( "yp_s_latin_1" );
        internal static ypObject_p yp_s_utf_32 = DllImportData( "yp_s_utf_32" );
        internal static ypObject_p yp_s_utf_32_be = DllImportData( "yp_s_utf_32_be" );
        internal static ypObject_p yp_s_utf_32_le = DllImportData( "yp_s_utf_32_le" );
        internal static ypObject_p yp_s_utf_16 = DllImportData( "yp_s_utf_16" );
        internal static ypObject_p yp_s_utf_16_be = DllImportData( "yp_s_utf_16_be" );
        internal static ypObject_p yp_s_utf_16_le = DllImportData( "yp_s_utf_16_le" );
        internal static ypObject_p yp_s_utf_8 = DllImportData( "yp_s_utf_8" );

        internal static ypObject_p yp_s_strict = DllImportData( "yp_s_strict" );
        internal static ypObject_p yp_s_ignore = DllImportData( "yp_s_ignore" );
        internal static ypObject_p yp_s_replace = DllImportData( "yp_s_replace" );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_add( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_sub( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_mul( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_truediv( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_floordiv( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_mod( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_divmod( ypObject_p x, ypObject_p y, ref ypObject_p div, ref ypObject_p mod );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_pow( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_pow3( ypObject_p x, ypObject_p y, ypObject_p z );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_neg( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_pos( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_abs( ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_lshift( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_rshift( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_amp( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_xor( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_bar( ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_invert( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_iadd( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_isub( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_imul( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_itruediv( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_ifloordiv( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_imod( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_ipow( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_ipow3( ref ypObject_p x, ypObject_p y, ypObject_p z );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_ineg( ref ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_ipos( ref ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_iabs( ref ypObject_p x );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_ilshift( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_irshift( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_iamp( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_ixor( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_ibar( ref ypObject_p x, ypObject_p y );
        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_iinvert( ref ypObject_p x );

#if NOTDEFINED
        internal static extern void yp_iaddC( ypObject_p *x, yp_int_t y );
        internal static extern void yp_isubC( ypObject_p *x, yp_int_t y );
        internal static extern void yp_imulC( ypObject_p *x, yp_int_t y );
        internal static extern void yp_itruedivC( ypObject_p *x, yp_int_t y );
        internal static extern void yp_ifloordivC( ypObject_p *x, yp_int_t y );
        internal static extern void yp_imodC( ypObject_p *x, yp_int_t y );
        internal static extern void yp_ipowC( ypObject_p *x, yp_int_t y );
        internal static extern void yp_ipowC3( ypObject_p *x, yp_int_t y, yp_int_t z );
        internal static extern void yp_ilshiftC( ypObject_p *x, yp_int_t y );
        internal static extern void yp_irshiftC( ypObject_p *x, yp_int_t y );
        internal static extern void yp_iampC( ypObject_p *x, yp_int_t y );
        internal static extern void yp_ixorC( ypObject_p *x, yp_int_t y );
        internal static extern void yp_ibarC( ypObject_p *x, yp_int_t y );

        internal static extern void yp_iaddCF( ypObject_p *x, yp_float_t y );
        internal static extern void yp_isubCF( ypObject_p *x, yp_float_t y );
        internal static extern void yp_imulCF( ypObject_p *x, yp_float_t y );
        internal static extern void yp_itruedivCF( ypObject_p *x, yp_float_t y );
        internal static extern void yp_ifloordivCF( ypObject_p *x, yp_float_t y );
        internal static extern void yp_imodCF( ypObject_p *x, yp_float_t y );
        internal static extern void yp_ipowCF( ypObject_p *x, yp_float_t y );

        internal static extern yp_int_t yp_addL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern yp_int_t yp_subL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern yp_int_t yp_mulL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern yp_float_t yp_truedivL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern yp_int_t yp_floordivL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern yp_int_t yp_modL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern void yp_divmodL( yp_int_t x, yp_int_t y, yp_int_t *div, yp_int_t *mod, ypObject_p *exc );
        internal static extern yp_int_t yp_powL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern yp_int_t yp_powL3( yp_int_t x, yp_int_t y, yp_int_t z, ypObject_p *exc );
        internal static extern yp_int_t yp_negL( yp_int_t x, ypObject_p *exc );
        internal static extern yp_int_t yp_posL( yp_int_t x, ypObject_p *exc );
        internal static extern yp_int_t yp_absL( yp_int_t x, ypObject_p *exc );
        internal static extern yp_int_t yp_lshiftL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern yp_int_t yp_rshiftL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern yp_int_t yp_ampL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern yp_int_t yp_xorL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern yp_int_t yp_barL( yp_int_t x, yp_int_t y, ypObject_p *exc );
        internal static extern yp_int_t yp_invertL( yp_int_t x, ypObject_p *exc );

        internal static extern yp_float_t yp_addLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        internal static extern yp_float_t yp_subLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        internal static extern yp_float_t yp_mulLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        internal static extern yp_float_t yp_truedivLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        internal static extern yp_float_t yp_floordivLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        internal static extern yp_float_t yp_modLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        internal static extern void yp_divmodLF( yp_float_t x, yp_float_t y,
                yp_float_t *div, yp_float_t *mod, ypObject_p *exc );
        internal static extern yp_float_t yp_powLF( yp_float_t x, yp_float_t y, ypObject_p *exc );
        internal static extern yp_float_t yp_negLF( yp_float_t x, ypObject_p *exc );
        internal static extern yp_float_t yp_posLF( yp_float_t x, ypObject_p *exc );
        internal static extern yp_float_t yp_absLF( yp_float_t x, ypObject_p *exc );

        internal static extern yp_int_t yp_asintC( ypObject_p x, ypObject_p *exc );
        internal static extern yp_int8_t yp_asint8C( ypObject_p x, ypObject_p *exc );
        internal static extern yp_uint8_t yp_asuint8C( ypObject_p x, ypObject_p *exc );
        internal static extern yp_int16_t yp_asint16C( ypObject_p x, ypObject_p *exc );
        internal static extern yp_uint16_t yp_asuint16C( ypObject_p x, ypObject_p *exc );
        internal static extern yp_int32_t yp_asint32C( ypObject_p x, ypObject_p *exc );
        internal static extern yp_uint32_t yp_asuint32C( ypObject_p x, ypObject_p *exc );
        internal static extern yp_int64_t yp_asint64C( ypObject_p x, ypObject_p *exc );
        internal static extern yp_uint64_t yp_asuint64C( ypObject_p x, ypObject_p *exc );
        internal static extern yp_float_t yp_asfloatC( ypObject_p x, ypObject_p *exc );
        internal static extern yp_float32_t yp_asfloat32C( ypObject_p x, ypObject_p *exc );
        internal static extern yp_float64_t yp_asfloat64C( ypObject_p x, ypObject_p *exc );
        internal static extern yp_ssize_t yp_asssizeC( ypObject_p x, ypObject_p *exc );
        internal static extern yp_hash_t yp_ashashC( ypObject_p x, ypObject_p *exc );
        internal static extern yp_float_t yp_asfloatL( yp_int_t x, ypObject_p *exc );
        internal static extern yp_int_t yp_asintLF( yp_float_t x, ypObject_p *exc );
#endif

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_roundC( ypObject_p x, int ndigits );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_sumN( int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_sum( ypObject_p iterable );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern yp_int_t yp_int_bit_lengthC( ypObject_p x, ref ypObject_p exc );

        internal static ypObject_p yp_sys_maxint = DllImportData( "yp_sys_maxint" );
        internal static ypObject_p yp_sys_minint = DllImportData( "yp_sys_minint" );

        internal static ypObject_p yp_i_neg_one = DllImportData( "yp_i_neg_one");
        internal static ypObject_p yp_i_zero = DllImportData( "yp_i_zero" );
        internal static ypObject_p yp_i_one = DllImportData( "yp_i_one" );
        internal static ypObject_p yp_i_two = DllImportData( "yp_i_two" );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_freeze( ref ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_deepfreeze( ref ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_unfrozen_copy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_unfrozen_deepcopy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_frozen_copy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_frozen_deepcopy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_copy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_deepcopy( ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_invalidate( ref ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern void yp_deepinvalidate( ref ypObject_p x );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_type( ypObject_p @object );

        internal static ypObject_p yp_type_invalidated = DllImportData( "yp_type_invalidated" );
        internal static ypObject_p yp_type_exception = DllImportData( "yp_type_exception" );
        internal static ypObject_p yp_type_type = DllImportData( "yp_type_type" );
        internal static ypObject_p yp_type_NoneType = DllImportData( "yp_type_NoneType" );
        internal static ypObject_p yp_type_bool = DllImportData( "yp_type_bool" );
        internal static ypObject_p yp_type_int = DllImportData( "yp_type_int" );
        internal static ypObject_p yp_type_intstore = DllImportData( "yp_type_intstore" );
        internal static ypObject_p yp_type_float = DllImportData( "yp_type_float" );
        internal static ypObject_p yp_type_floatstore = DllImportData( "yp_type_floatstore" );
        internal static ypObject_p yp_type_iter = DllImportData( "yp_type_iter" );
        internal static ypObject_p yp_type_bytes = DllImportData( "yp_type_bytes" );
        internal static ypObject_p yp_type_bytearray = DllImportData( "yp_type_bytearray" );
        internal static ypObject_p yp_type_str = DllImportData( "yp_type_str" );
        internal static ypObject_p yp_type_chrarray = DllImportData( "yp_type_chrarray" );
        internal static ypObject_p yp_type_tuple = DllImportData( "yp_type_tuple" );
        internal static ypObject_p yp_type_list = DllImportData( "yp_type_list" );
        internal static ypObject_p yp_type_frozenset = DllImportData( "yp_type_frozenset" );
        internal static ypObject_p yp_type_set = DllImportData( "yp_type_set" );
        internal static ypObject_p yp_type_frozendict = DllImportData( "yp_type_frozendict" );
        internal static ypObject_p yp_type_dict = DllImportData( "yp_type_dict" );
        internal static ypObject_p yp_type_range = DllImportData( "yp_type_range" );

        internal static ypObject_p yp_BaseException = DllImportData( "yp_BaseException" );
        internal static ypObject_p yp_Exception = DllImportData( "yp_Exception" );
        internal static ypObject_p yp_StopIteration = DllImportData( "yp_StopIteration" );
        internal static ypObject_p yp_GeneratorExit = DllImportData( "yp_GeneratorExit" );
        internal static ypObject_p yp_ArithmeticError = DllImportData( "yp_ArithmeticError" );
        internal static ypObject_p yp_LookupError = DllImportData( "yp_LookupError" );
        internal static ypObject_p yp_AssertionError = DllImportData( "yp_AssertionError" );
        internal static ypObject_p yp_AttributeError = DllImportData( "yp_AttributeError" );
        internal static ypObject_p yp_EOFError = DllImportData( "yp_EOFError" );
        internal static ypObject_p yp_FloatingPointError = DllImportData( "yp_FloatingPointError" );
        internal static ypObject_p yp_OSError = DllImportData( "yp_OSError" );
        internal static ypObject_p yp_ImportError = DllImportData( "yp_ImportError" );
        internal static ypObject_p yp_IndexError = DllImportData( "yp_IndexError" );
        internal static ypObject_p yp_KeyError = DllImportData( "yp_KeyError" );
        internal static ypObject_p yp_KeyboardInterrupt = DllImportData( "yp_KeyboardInterrupt" );
        internal static ypObject_p yp_MemoryError = DllImportData( "yp_MemoryError" );
        internal static ypObject_p yp_NameError = DllImportData( "yp_NameError" );
        internal static ypObject_p yp_OverflowError = DllImportData( "yp_OverflowError" );
        internal static ypObject_p yp_RuntimeError = DllImportData( "yp_RuntimeError" );
        internal static ypObject_p yp_NotImplementedError = DllImportData( "yp_NotImplementedError" );
        internal static ypObject_p yp_ReferenceError = DllImportData( "yp_ReferenceError" );
        internal static ypObject_p yp_SystemError = DllImportData( "yp_SystemError" );
        internal static ypObject_p yp_SystemExit = DllImportData( "yp_SystemExit" );
        internal static ypObject_p yp_TypeError = DllImportData( "yp_TypeError" );
        internal static ypObject_p yp_UnboundLocalError = DllImportData( "yp_UnboundLocalError" );
        internal static ypObject_p yp_UnicodeError = DllImportData( "yp_UnicodeError" );
        internal static ypObject_p yp_UnicodeEncodeError = DllImportData( "yp_UnicodeEncodeError" );
        internal static ypObject_p yp_UnicodeDecodeError = DllImportData( "yp_UnicodeDecodeError" );
        internal static ypObject_p yp_UnicodeTranslateError = DllImportData( "yp_UnicodeTranslateError" );
        internal static ypObject_p yp_ValueError = DllImportData( "yp_ValueError" );
        internal static ypObject_p yp_ZeroDivisionError = DllImportData( "yp_ZeroDivisionError" );
        internal static ypObject_p yp_BufferError = DllImportData( "yp_BufferError" );

        internal static ypObject_p yp_MethodError = DllImportData( "yp_MethodError" );
        internal static ypObject_p yp_MemorySizeOverflowError = DllImportData( "yp_MemorySizeOverflowError" );
        internal static ypObject_p yp_SystemLimitationError = DllImportData( "yp_SystemLimitationError" );
        internal static ypObject_p yp_InvalidatedError = DllImportData( "yp_InvalidatedError" );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern int yp_isexceptionC2( ypObject_p x, ypObject_p exc );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern int yp_isexceptionCN( ypObject_p x, int n, params ypObject_p[] args );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_asbytesCX( ypObject_p seq, out yp_uint8_t[] bytes, out yp_ssize_t len );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_asencodedCX( ypObject_p seq, out yp_uint8_t[] encoded, out yp_ssize_t size,
                out ypObject_p encoding );

        [DllImport( DLL_NAME, CallingConvention = CALLCONV )]
        internal static extern ypObject_p yp_itemarrayCX( ypObject_p seq, ref ypObject_p* array, yp_ssize_t* len );

        // FIXME Remove
        public static void Main()
        {
            Console.WriteLine( "Press a key..." );
            Console.ReadKey( true );
        }
    }
}

