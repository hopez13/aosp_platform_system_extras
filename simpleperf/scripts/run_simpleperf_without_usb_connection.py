#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
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

from __future__ import print_function

help_msg="""
    Support profiling without usb connection in below steps:
    1. With usb connection, start simpleperf recording.
    2. Unplug the usb cable and play the app you want to profile, while the process of
       simpleperf keeps running and collecting samples.
    3. Replug the usb cable, stop simpleperf recording and pull recording file on host.

    Note that recording is stopped once the app is killed. So if you restart the app
    during profiling time, simpleperf only records the first running.
"""

import argparse
import subprocess
import sys
import time

from utils import *

def start_recording(args):
    adb = AdbHelper()
    device_arch = adb.get_device_arch()
    simpleperf_binary = get_target_binary_path(device_arch, 'simpleperf')
    adb.check_run(['push', simpleperf_binary, '/data/local/tmp'])
    adb.check_run(['shell', 'chmod', 'a+x', '/data/local/tmp/simpleperf'])
    adb.check_run(['shell', 'rm', '-rf', '/data/local/tmp/perf.data',
                   '/data/local/tmp/simpleperf_output'])
    shell_cmd = 'cd /data/local/tmp && nohup ./simpleperf record ' + args.record_options
    if args.app:
        shell_cmd += ' --app ' + args.app
    shell_cmd += ' >/data/local/tmp/simpleperf_output 2>&1'
    print('shell_cmd: %s' % shell_cmd)
    subproc = subprocess.Popen([adb.adb_path, 'shell', shell_cmd])
    time.sleep(2)
    subproc.poll()
    if subproc.returncode is None:
        print('Simpleperf recording has started. Please unplug the usb cable and run the app.')
        print('After that, run `%s stop` to get recording result.' % sys.argv[0])
    else:
        adb.run(['shell', 'cat', '/data/local/tmp/simpleperf_output'])
        sys.exit(subproc.returncode)

def stop_recording(args):
    adb = AdbHelper()
    result = adb.run(['shell', 'pidof', 'simpleperf'])
    if not result:
        log_warning("""No simpleperf process on device. The recording has ended.""")
    else:
        adb.run(['shell', 'pkill', '-l', '2', 'simpleperf'])
        print('Waiting for simpleperf process to finish...')
        while True:
            time.sleep(1)
            if not adb.run(['shell', 'pidof', 'simpleperf']):
                break
    adb.check_run(['pull', '/data/local/tmp/perf.data', args.perf_data_path])
    adb.run(['shell', 'cat', '/data/local/tmp/simpleperf_output'])
    print('The recording data has been collected in %s.' % args.perf_data_path)

def main():
    parser = argparse.ArgumentParser(description=help_msg,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    subparsers = parser.add_subparsers()
    start_parser = subparsers.add_parser('start', help='Start recording.')
    start_parser.add_argument('-r', '--record_options',
                              default='-e task-clock:u -g --no-post-unwind',
                              help="""Set options for `simpleperf record` command.
                                      Default is '-e task-clock:u -g --no-post-unwind.""")
    start_parser.add_argument('-p', '--app', help="""Profile an Android app, given the package
                              name. Like -p com.example.android.myapp.""")
    start_parser.set_defaults(func=start_recording)
    stop_parser = subparsers.add_parser('stop', help='Stop recording.')
    stop_parser.add_argument('-o', '--perf_data_path', default='perf.data', help="""The path to
                             store profiling data on host. Default is perf.data.""")
    stop_parser.set_defaults(func=stop_recording)
    args = parser.parse_args()
    args.func(args)

if __name__ == '__main__':
    main()
