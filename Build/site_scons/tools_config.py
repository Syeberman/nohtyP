"""Site-specific tool configuration.  For some tools, this caches the results of lengthy
auto-detection; for other tools that don't have version-dependent auto-detect, this allows the
user to tell us where they put the other versions.
"""

import os
import io
import pprint
import errno


class ToolsConfig:
    def __init__(self, path):
        self.path = os.path.abspath(path)
        self.basename = os.path.basename(self.path)
        self._config = self._readOrCreate()

    def _readOrCreate(self):
        """Called on initialization to either read in the current configuration or create a new,
        empty configuration file.  Returns a dictionary of global variables from that file.
        """
        globalDict = {}
        try:
            infile = open(self.path)
        except IOError as e:
            if e.errno != errno.ENOENT:
                raise
            with open(self.path, "w") as outfile:
                outfile.write(
                    "# Holds site-specific configuration options for the compilers and other tools.\n")
                outfile.write(
                    "# Edit as appropriate.  Missing entries added automatically.  Rebuilt if deleted.\n")
                outfile.write("# None disables autodetection of that tool.\n\n")
        else:
            with infile:
                exec(infile.read(), globalDict)
        return globalDict

    def _formatConfiguration(self, config):
        """Validates and returns a formatted representation of the config dictionary suitable to
        write out to the file.
        """
        with io.StringIO() as outfile:
            prettify = pprint.PrettyPrinter(indent=1, width=120, stream=outfile)
            for key, value in sorted(config.items()):
                if not key.isidentifier:
                    raise ValueError("invalid key (must be identifier): %r" % (key, ))

                if not prettify.isreadable(value):
                    raise ValueError(
                        "invalid value (must round-trip through pprint): %r=%r" % (key, value))

                outfile.write("%s = " % (key, ))
                prettify.pprint(value)
                outfile.write("\n")

            return outfile.getvalue()

    def get(self, key, default):
        return self._config.get(key, default)

    def update(self, other):
        """Updates our configurations with data from the other dictionary.  This also writes the
        updated configuration to disk (appending to the file).
        """
        towrite = self._formatConfiguration(other)

        # We've verified all the key/value pairs above, so can update the file and dictionary
        with open(self.path, "a") as outfile:
            outfile.write(towrite)
        self._config.update(other)

    # Prevent copying this class when cloning SCons environments
    def __copy__(self):
        return self

    def __deepcopy__(self, memo):
        return self
