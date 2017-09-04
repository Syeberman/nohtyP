@rem "-C" ensures SCons uses the top level project directory
@python %~dp0..\Tools\scons\scons.py -C %~dp0.. -f Build\make.scons %*
