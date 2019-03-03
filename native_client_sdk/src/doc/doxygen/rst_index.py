#!/usr/bin/env python
# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate rst index file doxygen generated html files.
"""

import argparse
import cStringIO
import fnmatch
import os
import re
import sys

VALID_CHANNELS = ('stable', 'beta', 'dev')

ROOT_FILE_CONTENTS = """\
.. _pepper_%(channel)s_index:

:orphan:

.. DO NOT EDIT! This document is auto-generated by doxygen/rst_index.py.

.. include:: /migration/deprecation.inc

########################################
Pepper API Reference (%(channel_title)s)
########################################

This page lists the API for Pepper %(version)s. Apps that use this API can
run in Chrome %(version)s or higher.

:ref:`Pepper C API Reference <pepper_%(channel)s_c_index>`
===========================================================

:ref:`Pepper C++ API Reference <pepper_%(channel)s_cpp_index>`
===============================================================

"""

C_FILE_CONTENTS = """\
.. _pepper_%(channel)s_c_index:
.. _c-api%(channel_alt)s:

.. DO NOT EDIT! This document is auto-generated by doxygen/rst_index.py.

.. include:: /migration/deprecation.inc

##########################################
Pepper C API Reference (%(channel_title)s)
##########################################

This page lists the C API for Pepper %(version)s. Apps that use this API can
run in Chrome %(version)s or higher.

`Interfaces <pepper_%(channel)s/c/group___interfaces.html>`__
=============================================================
%(interfaces)s

`Structures <pepper_%(channel)s/c/group___structs.html>`__
==========================================================
%(structures)s

`Functions <pepper_%(channel)s/c/group___functions.html>`__
===========================================================

`Enums <pepper_%(channel)s/c/group___enums.html>`__
===================================================

`Typedefs <pepper_%(channel)s/c/group___typedefs.html>`__
=========================================================

`Macros <pepper_%(channel)s/c/globals_defs.html>`__
===================================================

Files
=====
%(files)s
"""

C_INTERFACE_WILDCARDS =  ['struct_p_p_p__*', 'struct_p_p_b__*']

C_STRUCT_WILDCARDS =  ['struct_p_p__*', 'union_p_p__*']

CPP_FILE_CONTENTS = """\
.. _pepper_%(channel)s_cpp_index:
.. _cpp-api%(channel_alt)s:

.. DO NOT EDIT! This document is auto-generated by doxygen/rst_index.py.

.. include:: /migration/deprecation.inc

############################################
Pepper C++ API Reference (%(channel_title)s)
############################################

This page lists the C++ API for Pepper %(version)s. Apps that use this API can
run in Chrome %(version)s or higher.

`Classes <pepper_%(channel)s/cpp/inherits.html>`__
==================================================
%(classes)s

Files
=====
%(files)s
"""

CPP_CLASSES_WILDCARDS = ['classpp_1_1*.html']
CPP_CLASSES_EXCLUDES = ['*-members*']

FILE_WILDCARDS = ['*_8h.html']


def GetName(filename):
  filename = os.path.splitext(filename)[0]
  out = ''
  if filename.startswith('struct_p_p_b__'):
    mangle = filename[7:]  # skip "struct_"
  elif filename.startswith('struct_p_p_p__'):
    mangle = filename[7:]  # skip "struct_"
  elif filename.startswith('struct_p_p__'):
    mangle = filename[7:]  # skip "struct_"
  elif filename.startswith('union_p_p__'):
    mangle = filename[6:]  # skip "union_"
  elif filename.startswith('classpp_1_1_'):
    mangle = filename[12:]
  elif filename.startswith('classpp_1_1ext_1_1_'):
    out = 'Ext::'      # maybe 'ext::' ?
    mangle = filename[19:]
  elif filename.startswith('classpp_1_1internal_1_1_'):
    out = 'Internal::' # maybe 'internal::'
    mangle = filename[24:]
  elif filename.startswith('structpp_1_1internal_1_1_'):
    out = 'Internal::'
    mangle = filename[25:]
  elif filename.endswith('_8h'):
    return filename[:-3].replace('__', '_') + '.h'
  else:
    print 'No match: ' + filename
  cap = True
  for c in mangle:
    if c == '_':
      if cap:
        # If cap is True, we've already read one underscore. The second means
        # that we should insert a literal underscore.
        cap = False
      else:
        cap = True
        continue
    if cap:
      c = c.upper()
      cap = False
    out += c

  # Strip trailing version number (e.g. PPB_Audio_1_1 -> PPB_Audio)
  return re.sub(r'_\d_\d$', '', out)


def GetPath(filepath):
  if os.path.exists(filepath):
    return filepath
  raise OSError('Couldn\'t find: ' + filepath)


def MakeReSTListFromFiles(prefix, path, matches, excludes=None):
  dir_files = os.listdir(path)
  good_files = []
  for match in matches:
    good_files.extend(fnmatch.filter(dir_files, match))

  if excludes:
    for exclude in excludes:
      good_files = [filename for filename in good_files
                    if not fnmatch.fnmatch(filename, exclude)]

  good_files.sort()
  return '\n'.join('  * `%s <%s/%s>`__\n' % (GetName(f), prefix, f)
                   for f in good_files)


def MakeTitleCase(s):
  return s[0].upper() + s[1:]

def MakeChannelAlt(channel):
  if channel == 'stable':
    return ''
  else:
    return '-' + channel


def GenerateRootIndex(channel, version, out_filename):
  channel_title = MakeTitleCase(channel)
  channel_alt = MakeChannelAlt(channel)

  # Use StringIO so we don't write out a partial file on error.
  output = cStringIO.StringIO()
  output.write(ROOT_FILE_CONTENTS % vars())

  with open(out_filename, 'w') as f:
    f.write(output.getvalue())


def GenerateCIndex(root_dir, channel, version, out_filename):
  prefix = 'pepper_%s/c' % channel
  interfaces = MakeReSTListFromFiles(prefix, root_dir, C_INTERFACE_WILDCARDS)
  structures = MakeReSTListFromFiles(prefix, root_dir, C_STRUCT_WILDCARDS)
  files = MakeReSTListFromFiles(prefix, root_dir, FILE_WILDCARDS)
  channel_title = MakeTitleCase(channel)
  channel_alt = MakeChannelAlt(channel)

  # Use StringIO so we don't write out a partial file on error.
  output = cStringIO.StringIO()
  output.write(C_FILE_CONTENTS % vars())

  with open(out_filename, 'w') as f:
    f.write(output.getvalue())


def GenerateCppIndex(root_dir, channel, version, out_filename):
  prefix = 'pepper_%s/cpp' % channel
  classes = MakeReSTListFromFiles(prefix, root_dir, CPP_CLASSES_WILDCARDS,
                                  CPP_CLASSES_EXCLUDES)
  files = MakeReSTListFromFiles(prefix, root_dir, FILE_WILDCARDS)
  channel_title = MakeTitleCase(channel)
  channel_alt = MakeChannelAlt(channel)

  # Use StringIO so we don't write out a partial file on error.
  output = cStringIO.StringIO()
  output.write(CPP_FILE_CONTENTS % vars())

  with open(out_filename, 'w') as f:
    f.write(output.getvalue())


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--channel', help='pepper channel (stable, beta, dev)')
  parser.add_argument('--version', help='pepper version (e.g. 32, 33, etc.)')
  parser.add_argument('--root', help='Generate root API index',
                      action='store_true', default=False)
  parser.add_argument('--c', help='Generate C API index', action='store_true',
                      default=False)
  parser.add_argument('--cpp', help='Generate C++ API index',
                      action='store_true', default=False)
  parser.add_argument('directory', help='input directory')
  parser.add_argument('output_file', help='output file')
  options = parser.parse_args(argv)

  if options.channel not in VALID_CHANNELS:
    parser.error('Expected channel to be one of %s' % ', '.join(VALID_CHANNELS))

  if sum((options.c, options.cpp, options.root)) != 1:
    parser.error('Exactly one of --c/--cpp/--root flags is required.')


  if options.c:
    GenerateCIndex(options.directory, options.channel, options.version,
                   options.output_file)
  elif options.cpp:
    GenerateCppIndex(options.directory, options.channel, options.version,
                     options.output_file)
  elif options.root:
    GenerateRootIndex(options.channel, options.version,
                      options.output_file)
  else:
    assert(False)
  return 0


if __name__ == '__main__':
  try:
    rtn = main(sys.argv[1:])
  except KeyboardInterrupt:
    sys.stderr.write('%s: interrupted\n' % os.path.basename(__file__))
    rtn = 1
  sys.exit(rtn)
