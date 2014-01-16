@rem "-C" ensures SCons uses the top level project directory
@scons -C %~dp0.. -f Build\make.scons %*
