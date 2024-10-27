import socket
import os
import sys
import argparse
import random
from datetime import datetime
import time
import re
import fnmatch
import glob
def is_valid_ip(ip):
    pattern = r'^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$'
    if re.match(pattern, ip):
        octets = ip.split('.')
        return all(0 <= int(octet) <= 255 for octet in octets)
    return False

def is_valid_port(port):
    try:
        port_num = int(port)
        return 1 <= port_num <= 65535
    except ValueError:
        return False

def get_sencond_parameter(parameter):
    if len(parameter.split(' ', 1))>1:
        return parameter.split(' ', 1)[1]
    else:
        return None
def check_parameter(line):
    if len(line.split(' ', 1))>1:
        return True
    else:
        print("Error: Missing parameters.")
        return False