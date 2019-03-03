#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import base64
import hashlib
import re
import os
import sys


def ComputeIntegrity(input_path):
  hasher = hashlib.sha256()
  with open(input_path, 'r') as f:
    hasher.update(f.read())
  return base64.b64encode(hasher.digest())


def WriteHeader(input_paths_and_integrity, output_path):
  with open(output_path, 'w') as f:
    f.write('// DO NOT MODIFY THIS FILE DIRECTLY!\n')
    f.write('// IT IS GENERATED BY generate_integrity_header.py\n')
    f.write('// FROM:\n')
    for (input_filename, _) in input_paths_and_integrity:
      f.write('//     * ' + input_filename + '\n')

    f.write('\n')

    for (input_filename, integrity) in input_paths_and_integrity:
      define_name = re.sub('\W', '_', input_filename.upper())
      define_name = define_name + '_INTEGRITY'

      f.write('#define ' + define_name + ' "' + integrity + '"\n')

      f.write('\n')


def main():
  parser = argparse.ArgumentParser(
      description='Generate a C++ header containing a sha256 checksum of the '
      'input files.')
  parser.add_argument('input_path', help='Path to an input file.', nargs='+')
  parser.add_argument('--output_path', help='Path to an output header file.')
  args = parser.parse_args()

  input_paths = args.input_path
  output_path = args.output_path

  input_paths_and_integrity = [(os.path.basename(path), ComputeIntegrity(path))
                          for path in input_paths]
  WriteHeader(input_paths_and_integrity, output_path)

  return 0


if __name__ == '__main__':
  sys.exit(main())
