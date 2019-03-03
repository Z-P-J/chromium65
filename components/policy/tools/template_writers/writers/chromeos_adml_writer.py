#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import base64

from writers import adml_writer


def GetWriter(config):
  '''Factory method for creating ADMLWriter objects for the Chrome OS platform.
  See the constructor of TemplateWriter for description of arguments.
  '''
  return ChromeOSADMLWriter(['chrome_os'], config)

class ChromeOSADMLWriter(adml_writer.ADMLWriter):
  ''' Class for generating Chrome OS ADML policy templates. It is used by the
  PolicyTemplateGenerator to write the ADML file.
  '''

  # Overridden.
  # These ADML files are used to generate GPO for Active Directory managed
  # Chrome OS devices.
  def IsPolicySupported(self, policy):
    return self.IsCrOSManagementSupported(policy, 'active_directory') and \
           super(ChromeOSADMLWriter, self).IsPolicySupported(policy)
