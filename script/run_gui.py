#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import subprocess

def execute(command):
    proc = subprocess.Popen(
        command,
        shell = False,
        stdin  = subprocess.PIPE,
        stdout = subprocess.PIPE,
        stderr = subprocess.PIPE)
    
    stdout_data, stderr_data = proc.communicate()
    print " ".join(command);
    if stdout_data:
        print stdout_data
    if stderr_data:
        print stderr_data

ROOT_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
execute(['electron',
         os.path.join(ROOT_PATH, 'src', 'electron')])
