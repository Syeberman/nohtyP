"""Utility to find tool executables and query their versions
"""

from collections import OrderedDict
import functools
import os
from pathlib import Path
import SCons.Platform

rsorted = functools.partial(sorted, reverse=True)

_platform_name = SCons.Platform.Platform().name
if _platform_name in ("win32", "cygwin"):

    def _get_dirs(os_dirs):
        # Use an OrderedDict to ensure uniqueness (values are ignored)
        dir_paths = OrderedDict()
        drives = OrderedDict.fromkeys((Path('C:\\'), Path(Path.cwd().anchor)))  # FIXME cwd?

        # Parse PATH, and use those paths to guess the drives where we'll find Program Files
        # (for users that have installed programs on separate drives)
        # TODO Windows allows path parts to be surrounded by quotes, I think?
        environ_paths = [Path(x) for x in os.environ.get('PATH', '').split(';') if x]
        drives.update((Path(x.anchor), None) for x in environ_paths if x.drive)

        # PATH is searched in order, respecting the user's priorities.
        for environ_path in environ_paths:
            try:
                environ_path = environ_path.resolve()
            except FileNotFoundError:
                continue
            dir_paths[environ_path] = None

        for prog_files in ('\\Program Files', '\\Program Files (x86)', '\\'):
            for drive in drives:
                try:
                    prog_files_path = Path(drive, prog_files).resolve()
                except FileNotFoundError:
                    continue
                # Directory patterns are in order, allowing tools to configure.
                for dir_glob in os_dirs:
                    # Matching directories are sorted in reverse, so that if the path happens to
                    # contain version information, later versions are (roughly) yielded first.
                    for os_path in rsorted(prog_files_path.glob(dir_glob)):
                        dir_paths[os_path] = None

        return dir_paths.keys()

    def _iter_paths(win_dirs, posix_dirs, darwin_dirs, exe_globs):
        path_exts = [
            ext for ext in os.environ.get('PATHEXT', '.COM;.EXE;.BAT;.CMD').lower().split(';')
            if ext.startswith(".")
        ]
        exe_ext_globs = [name + ext for name in exe_globs for ext in path_exts]

        # Paths are searched in order, respecting the user's priorities.
        for dir_path in _get_dirs(win_dirs):
            # Executable patterns are in order, allowing tools to configure.
            for exe_ext_glob in exe_ext_globs:
                # Matching executables are sorted in reverse, so that if the path happens to contain
                # version information, later versions are (roughly) yielded first.
                for exe_path in rsorted(dir_path.glob(exe_ext_glob)):
                    yield exe_path

else:

    def _get_dirs(os_dirs):
        # Use an OrderedDict to ensure uniqueness (values are ignored)
        dir_paths = OrderedDict()

        environ_paths = [Path(x) for x in os.environ.get('PATH', '').split(':') if x]

        # PATH is searched in order, respecting the user's priorities.
        for environ_path in environ_paths:
            try:
                environ_path = environ_path.resolve()
            except FileNotFoundError:
                continue
            dir_paths[environ_path] = None

        # Directory patterns are in order, allowing tools to configure.
        for dir_glob in os_dirs:
            # Matching directories are sorted in reverse, so that if the path happens to contain
            # version information, later versions are (roughly) yielded first.
            for os_path in rsorted(Path("/").glob(dir_glob)):
                dir_paths[os_path] = None

        return dir_paths.keys()

    def _iter_paths(win_dirs, posix_dirs, darwin_dirs, exe_globs):
        # Paths are searched in order, respecting the user's priorities.
        os_dirs = darwin_dirs if _platform_name == "darwin" else posix_dirs
        for dir_path in _get_dirs(os_dirs):
            # Executable patterns are in order, allowing tools to configure.
            for exe_glob in exe_globs:
                # Matching executables are sorted in reverse, so that if the path happens to contain
                # version information, later versions are (roughly) yielded first.
                for exe_path in rsorted(dir_path.glob(exe_glob)):
                    yield exe_path


# TODO This, or something like it, should be in SCons
class ToolFinder:
    def __init__(self, win_dirs, posix_dirs, darwin_dirs, exe_globs, version_detector):
        """ToolFinder looks for tools by executable name and tests them for version information.
        It first searches for executables matching exe_globs in PATH, then in the paths identified
        by win_dirs, posix_dirs, or darwin_dirs. The order of globs is respected: each exe_glob
        is tested in a single path before moving to the next path.

        win_dirs
            List of Windows path globs to search for executables. Each glob is prepended with
            'C:\\Program Files', 'C:\\Program Files (x86)', 'C:\\', and similar for detected fixed
            drives.
        posix_dirs
            List of Unix/Linux/BSD path globs to search for executables. Each glob should be an
            absolute path starting with '/'.
        darwin_dirs
            List of MacOS path globs to search for executables. Each glob should be an absolute
            path starting with '/'.
        exe_globs
            List of filename globs for the executables that will be tested with version_detector.
            On Windows each of the PATHEXT extensions is appended (so do not include .exe, etc).
        version_detector
            A function that takes an absolute Path to an executable and returns the version of
            the tool as an arbitrary object. The results of this function are cached and returned
            by __iter__ and the like.
        """
        self.win_dirs = win_dirs
        self.posix_dirs = posix_dirs
        self.darwin_dirs = darwin_dirs
        self.exe_globs = exe_globs
        self.version_detector = version_detector
        self._path2version = None

    def _ensure_path2version(self):
        if self._path2version is None:
            self._path2version = OrderedDict.fromkeys(
                _iter_paths(self.win_dirs, self.posix_dirs, self.darwin_dirs, self.exe_globs), None
            )

    def __iter__(self):
        """Yields (path, version) for all executables that match the globs."""
        self._ensure_path2version()

        for path, version in self._path2version.items():
            if version is None:
                version = self._path2version[path] = self.version_detector(path)
            yield path, version
