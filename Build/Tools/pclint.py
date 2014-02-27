
# XXX This supports both PC-lint and FlexeLint by Gimpel Software

raise NotImplementedError( "TODO implement PC-Lint support" )

# Default on Windows is c:\lint

"""
lint-nt.exe

// This will depend on if we're running on "debug" or "analyze"
// TODO do we also want to support "release"?  I don't think so, because asserts help lint...
-w2

// Some compilers' headers have long lines
+linebuf

// Don't warn on sign differences in char pointers
-epuc

// The last value of args by va_arg (or va_end?) looks like it's not being used
// TODO more-precise fix needed
-e438

// "For clause irregularity: variable tested in 2nd expression does not match that modified in 3rd"
-e440

// Some if statements always evaluate to true/false on certain targets: compiler can easily remove
-e506 -e685

// Can ignore return value
-ecall(534,yp_incref)

// div is a function defined in compiler headers
-esym(578,div)

// We perform some questionable operations in yp_STATIC_ASSERT in order to test the compiler
-emacro(572,yp_STATIC_ASSERT) -emacro(649,yp_STATIC_ASSERT)

// These macros appear to dereference NULL...but only to get the size/offset of a member
-emacro((413),yp_sizeof_member,yp_offsetof)

// We don't necessarily use all these tp_* stubs, and that's OK (the compiler will remove anyway)
-esym(528,MethodError_*,TypeError_*,InvalidatedError_*,ExceptionMethod_*)

// ypTypeTable is a static array that must be declared at the top and defined at the bottom...but
// the strict C standard doesn't allow for that (although compilers do)
-esym(31,ypTypeTable)

// Some compilers' headers have data following incomplete arrays (?!)
-elib(157)

// Will need to pull this information from SCons
co-msc100.lnt -Dyp_DEBUG_LEVEL=1 nohtyP.c

"""
