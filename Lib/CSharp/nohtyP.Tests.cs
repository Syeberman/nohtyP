using System.Text;
using NUnit.Framework;

namespace nohtyP
{
    [TestFixture]
    public class Tests
    {
        [Test]
        public void Constructors()
        {
            // FIXME Actually get information out of this
            yp.@int();
            yp.@int( 25 );
            // TODO @int( @object x, long @base )
            yp.@int( yp.@float( 55.5 ) );

            yp.intstore();
            yp.intstore( 300 );
            // TODO intstore( @object x, long @base )
            yp.intstore( yp.@float( 66.6 ) );

            yp.@float();
            yp.@float( 44.444 );
            yp.@float( yp.@int( 20 ) );

            yp.floatstore();
            yp.floatstore( 99393.9399393 );
            yp.floatstore( yp.@int( 123 ) );

            yp.iter( yp.range( 5 ) );

            yp.range( 32, 64 );
            yp.range( 6, 666, 6 );
            yp.range( 27 );

            yp.bytes( 27 );
            yp.bytes( Encoding.ASCII.GetBytes( "abcd" ) );
            // TODO bytes( @object source, @object encoding, @object errors ) et al
            yp.bytes( yp.range( 7 ) );
            yp.bytes();

            yp.bytearray( 1 );
            yp.bytearray( Encoding.ASCII.GetBytes( "ZYXW" ) );
            // TODO bytearray( @object source, @object encoding, @object errors ) et al
            yp.bytearray( yp.@int( 6 ) );
            yp.bytearray();
            
            // TODO yp.str( "hey hey mamma" )

            // TODO yp.chr

            // TODO yp.chrarray

            yp.tuple( new yp.@object[] { } );
            yp.tuple( new[] {yp.None, yp.False, yp.@int()} );
            yp.tuple( yp.range( 3 ) );
            yp.tuple();

            yp.list( new yp.@object[] { } );
            yp.list( new[] { yp.None, yp.True, yp.@float() } );
            yp.list( yp.range( 5 ) );
            yp.list();

            //yp.sorted

            yp.frozenset( new yp.@object[] { } );
            yp.frozenset( new [] { yp.None, yp.True, yp.@float() } );
            yp.frozenset( yp.range( 16 ) );
            yp.frozenset();

            yp.set( new yp.@object[] { } );
            yp.set( new[] { yp.None, yp.None } );
            yp.set( yp.bytes( Encoding.ASCII.GetBytes( "aaaaaaaaa" ) ) );
            yp.set();
            
            // TODO This form of calling kinda sucks...but that _is_ C# array syntax
            // FIXME yp.frozendict( new yp.@object[0,0] { } );
            yp.frozendict( new[] { new[] { yp.range( 6 ), yp.bytes( 52 ) } } );
            // TODO continue from here
            
            yp.dict( new[] { new[] { yp.@int(), yp.intstore( 52 ) } } );
            
            //yp.@bool
        }
    }
}