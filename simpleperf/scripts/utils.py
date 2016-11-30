#!/usr/bin/env python
#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""utils.py: export utility functions.
"""

from __future__ import print_function
import logging
import os.path
import subprocess
import sys

def get_script_dir():
    return os.path.dirname(os.path.realpath(__file__))


def is_windows():
    return sys.platform == 'win32' or sys.platform == 'cygwin'


def log_debug(msg):
    logging.debug(msg)


def log_info(msg):
    logging.info(msg)


def log_warning(msg):
    logging.warning(msg)


def log_fatal(msg):
    raise Exception(msg)


def get_target_binary_path(arch, binary_name):
    arch_dir = os.path.join(get_script_path(), "shared_libraries", "target", arch)
    if not os.path.isdir(arch_dir):
        log_fatal("can't find arch directory: %s" % arch_dir)
    binary_path = os.path.join(arch_dir, binary_name)
    if not os.path.isfile(binary_path):
        log_fatal("can't find binary: %s" % binary_path)
    return binary_path


def get_host_binary_path(binary_name):
    dir = os.path.join(get_script_dir(), 'shared_libraries', 'host')
    if not os.path.isdir(dir):
        log_fatal("can't find directory: %s" % dir)
    if is_windows():
        if so_name.endswith('.so'):
            so_name = so_name[0:-3] + '.dll'
        dir = os.path.join(dir, 'windows')
    elif sys.platform == 'darwin': # OSX
        if so_name.endswith('.so'):
            so_name = so_name[0:-3] + '.dylib'
        dir = os.path.join(dir, 'darwin')
    else:
        dir = os.path.join(dir, 'linux')
    if not os.path.isdir(dir):
        log_fatal("can't find directory: %s" % dir)
    binary_path = os.path.join(dir, binary_name)
    if not os.path.isfile(binary_path):
        log_fatal("can't find binary: %s" % binary_path)
    return binary_path


class AdbHelper(object):
    def __init__(self, adb_path):
        self.adb_path = adb_path

    def run(self, adb_args):
        return self.run_and_return_output(adb_args)[0]

    def run_and_return_output(self, adb_args):
        adb_args = [self.adb_path] + adb_args
        log_debug('run adb cmd: %s' % adb_args)
        subproc = subprocess.Popen(adb_args, stdout=subprocess.PIPE)
        (stdoutdata, _) = subproc.communicate()
        returncode = subproc.wait()
        result = (returncode == 0)
        if len(stdoutdata) > 0:
            log_debug(stdoutdata)
        log_debug('run adb cmd: %s  [result %s]' % (adb_args, result))
        return (result, stdoutdata)

    def switch_to_root(self):
        result, stdoutdata = self.run_and_return_output(['shell', 'whoami'])
        if not result:
            return False
        if stdoutdata.find('root') != -1:
            return True
        result, stdoutdata = self.run_and_return_output(['shell', 'getprop', 'ro.build.type'])
        if not result:
            return False
        if stdoutdata.strip() == 'user':
            return False
        self.run(['root'])
        result, stdoutdata = self.run_and_return_output(['shell', 'whoami'])
        if result and stdoutdata.find('root') != -1:
            return True
        return False


logging.getLogger().setLevel(logging.DEBUG)