#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tool to produce localized strings for the remoting iOS client.

This script uses a subset of grit-generated string data-packs to produce
localized string files appropriate for iOS.

For each locale, it generates the following:

<locale>.lproj/
  Localizable.strings
  InfoPlist.strings

The strings in Localizable.strings are specified in a file containing a list of
IDS. E.g.:

Given: Localizable_ids.txt:
IDS_PRODUCT_NAME
IDS_SIGN_IN_BUTTON
IDS_CANCEL

Produces: Localizable.strings:
"IDS_PRODUCT_NAME" = "Remote Desktop";
"IDS_SIGN_IN_BUTTON" = "Sign In";
"IDS_CANCEL" = "Cancel";

The InfoPlist.strings is formatted using a Jinja2 template where the "ids"
variable is a dictionary of id -> string. E.g.:

Given: InfoPlist.strings.jinja2:
"CFBundleName" = "{{ ids.IDS_PRODUCT_NAME }}"
"CFCopyrightNotice" = "{{ ids.IDS_COPYRIGHT }}"

Produces: InfoPlist.strings:
"CFBundleName" = "Remote Desktop";
"CFCopyrightNotice" = "Copyright 2014 The Chromium Authors.";

Parameters:
  --print-inputs
     Prints the expected input file list, then exit. This can be used in gyp
     input rules.

  --print-outputs
     Prints the expected output file list, then exit. This can be used in gyp
     output rules.

  --from-dir FROM_DIR
     Specify the directory containing the data pack files generated by grit.
     Each data pack should be named <locale>.pak.

  --to-dir TO_DIR
     Specify the directory to write the <locale>.lproj directories containing
     the string files.

  --localizable-list LOCALIZABLE_ID_LIST
     Specify the file containing the list of the IDs of the strings that each
     Localizable.strings file should contain.

  --infoplist-template INFOPLIST_TEMPLATE
     Specify the Jinja2 template to be used to create each InfoPlist.strings
     file.

  --resources-header RESOURCES_HEADER
     Specifies the grit-generated header file that maps ID names to ID values.
     It's required to map the IDs in LOCALIZABLE_ID_LIST and INFOPLIST_TEMPLATE
     to strings in the data packs.
"""


import codecs
import optparse
import os
import re
import sys

# Prepend the grit module from the source tree so it takes precedence over other
# grit versions that might present in the search path.
sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..', '..', '..',
                                'tools', 'grit'))
from grit.format import data_pack

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..',
                             'third_party'))
import jinja2


LOCALIZABLE_STRINGS = 'Localizable.strings'
INFOPLIST_STRINGS = 'InfoPlist.strings'


class LocalizeException(Exception):
  pass


class LocalizedStringJinja2Adapter:
  """Class that maps ID names to localized strings in Jinja2."""
  def __init__(self, id_map, pack):
    self.id_map = id_map
    self.pack = pack

  def __getattr__(self, name):
    id_value = self.id_map.get(name)
    if not id_value:
      raise LocalizeException('Could not find id %s in resource header' % name)
    data = self.pack.resources.get(id_value)
    if not data:
      raise LocalizeException(
            'Could not find string with id %s (%d) in data pack' %
             (name, id_value))
    return decode_and_escape(data)


def get_inputs(from_dir, locales):
  """Returns the list of files that would be required to run the tool."""
  inputs = []
  for locale in locales:
    inputs.append(os.path.join(from_dir, '%s.pak' % locale))
  return format_quoted_list(inputs)


def get_outputs(to_dir, locales):
  """Returns the list of files that would be produced by the tool."""
  outputs = []
  for locale in locales:
    lproj_dir = format_lproj_dir(to_dir, locale)
    outputs.append(os.path.join(lproj_dir, LOCALIZABLE_STRINGS))
    outputs.append(os.path.join(lproj_dir, INFOPLIST_STRINGS))
  return format_quoted_list(outputs)


def format_quoted_list(items):
  """Formats a list as a string, with items space-separated and quoted."""
  return " ".join(['"%s"' % x for x in items])


def format_lproj_dir(to_dir, locale):
  """Formats the name of the lproj directory for a given locale."""
  locale = locale.replace('-', '_')
  return os.path.join(to_dir, '%s.lproj' % locale)


def read_resources_header(resources_header_path):
  """Reads and parses a grit-generated resource header file.

  This function will parse lines like the following:

  #define IDS_PRODUCT_NAME 28531
  #define IDS_CANCEL 28542

  And return a dictionary like the following:

  { 'IDS_PRODUCT_NAME': 28531, 'IDS_CANCEL': 28542 }
  """
  regex = re.compile(r'^#define\s+(\w+)\s+(\d+)$')
  id_map = {}
  try:
    with open(resources_header_path, 'r') as f:
      for line in f:
        match = regex.match(line)
        if match:
          id_str = match.group(1)
          id_value = int(match.group(2))
          id_map[id_str] = id_value
  except:
    sys.stderr.write('Error while reading header file %s\n'
                     % resources_header_path)
    raise

  return id_map


def read_id_list(id_list_path):
  """Read a text file with ID names.

  Names are stripped of leading and trailing spaces. Empty lines are ignored.
  """
  with open(id_list_path, 'r') as f:
    stripped_lines = [x.strip() for x in f]
    non_empty_lines = [x for x in stripped_lines if x]
    return non_empty_lines


def read_jinja2_template(template_path):
  """Reads a Jinja2 template."""
  (template_dir, template_name) = os.path.split(template_path)
  env = jinja2.Environment(loader = jinja2.FileSystemLoader(template_dir))
  template = env.get_template(template_name)
  return template


def decode_and_escape(data):
  """Decodes utf-8 data, and escapes it appropriately to use in *.strings."""
  u_string = codecs.decode(data, 'utf-8')
  u_string = u_string.replace('\\', '\\\\')
  u_string = u_string.replace('"', '\\"')
  return u_string


def generate(from_dir, to_dir, localizable_list_path, infoplist_template_path,
             resources_header_path, locales):
  """Generates the <locale>.lproj directories and files."""

  id_map = read_resources_header(resources_header_path)
  localizable_ids = read_id_list(localizable_list_path)
  infoplist_template = read_jinja2_template(infoplist_template_path)

  # Generate string files for each locale
  for locale in locales:
    pack = data_pack.ReadDataPack(
        os.path.join(os.path.join(from_dir, '%s.pak' % locale)))

    lproj_dir = format_lproj_dir(to_dir, locale)
    if not os.path.exists(lproj_dir):
      os.makedirs(lproj_dir)

    # Generate Localizable.strings
    localizable_strings_path = os.path.join(lproj_dir, LOCALIZABLE_STRINGS)
    try:
      with codecs.open(localizable_strings_path, 'w', 'utf-16') as f:
        for id_str in localizable_ids:
          id_value = id_map.get(id_str)
          if not id_value:
            raise LocalizeException('Could not find "%s" in %s' %
                                    (id_str, resources_header_path))

          localized_data = pack.resources.get(id_value)
          if not localized_data:
            raise LocalizeException(
                'Could not find localized string in %s for %s (%d)' %
                (localizable_strings_path, id_str, id_value))

          f.write(u'"%s" = "%s";\n' %
                  (id_str, decode_and_escape(localized_data)))
    except:
      sys.stderr.write('Error while creating %s\n' % localizable_strings_path)
      raise

    # Generate InfoPlist.strings
    infoplist_strings_path = os.path.join(lproj_dir, INFOPLIST_STRINGS)
    try:
      with codecs.open(infoplist_strings_path, 'w', 'utf-16') as f:
        infoplist = infoplist_template.render(
            ids = LocalizedStringJinja2Adapter(id_map, pack))
        f.write(infoplist)
    except:
      sys.stderr.write('Error while creating %s\n' % infoplist_strings_path)
      raise


def DoMain(args):
  """Entrypoint used by gyp's pymod_do_main."""
  parser = optparse.OptionParser("usage: %prog [options] locales")
  parser.add_option("--print-inputs", action="store_true", dest="print_input",
                    default=False,
                    help="Print the expected input file list, then exit.")
  parser.add_option("--print-outputs", action="store_true", dest="print_output",
                    default=False,
                    help="Print the expected output file list, then exit.")
  parser.add_option("--from-dir", action="store", dest="from_dir",
                    help="Source data pack directory.")
  parser.add_option("--to-dir", action="store", dest="to_dir",
                    help="Destination data pack directory.")
  parser.add_option("--localizable-list", action="store",
                    dest="localizable_list",
                    help="File with list of IDS to build Localizable.strings")
  parser.add_option("--infoplist-template", action="store",
                    dest="infoplist_template",
                    help="File with list of IDS to build InfoPlist.strings")
  parser.add_option("--resources-header", action="store",
                    dest="resources_header",
                    help="Auto-generated header with resource ids.")
  options, locales = parser.parse_args(args)

  if not locales:
    parser.error('At least one locale is required.')

  if options.print_input and options.print_output:
    parser.error('Only one of --print-inputs or --print-outputs is allowed')

  if options.print_input:
    if not options.from_dir:
      parser.error('--from-dir is required.')
    return get_inputs(options.from_dir, locales)

  if options.print_output:
    if not options.to_dir:
      parser.error('--to-dir is required.')
    return get_outputs(options.to_dir, locales)

  if not (options.from_dir and options.to_dir and options.localizable_list and
          options.infoplist_template and options.resources_header):
    parser.error('--from-dir, --to-dir, --localizable-list, ' +
                 '--infoplist-template and --resources-header are required.')

  try:
    generate(options.from_dir, options.to_dir, options.localizable_list,
             options.infoplist_template, options.resources_header, locales)
  except LocalizeException as e:
    sys.stderr.write('Error: %s\n' % str(e))
    sys.exit(1)

  return ""


def main(args):
  print DoMain(args[1:])


if __name__ == '__main__':
  main(sys.argv)
