#!/usr/bin/env python
from __future__ import print_function
import base64
import os
import re
import sys

int_names = set([
    'valueSize',
    'version',
    'CL_DEVICE_VENDOR_ID',
    'RADIX_BITS',
    'REDUCE_WORK_GROUP_SIZE',
    'SCAN_BLOCKS',
    'SCAN_WORK_GROUP_SIZE',
    'SCAN_WORK_SCALE',
    'SCATTER_WORK_GROUP_SIZE',
    'SCATTER_WORK_SCALE',
    'WARP_SIZE_MEM',
    'WARP_SIZE_SCHEDULE'])

string_names = set([
    'CL_DEVICE_NAME',
    'CL_DRIVER_VERSION',
    'CL_PLATFORM_NAME',
    'algorithm',
    'keyType',
    'elementType'])

def decode(key, value):
    '''
    Decodes the value to the appropriate type for the key.
    '''
    if key in int_names:
        return int(value)
    elif key in string_names:
        return base64.b64decode(value)
    else:
        return value

def parse_cache_file(filename):
    '''
    Parses a cache file and returns a dictionary of keys and a dictionary of values.
    Recognized string values are base64-decoded and recognized integral types are
    converted to int. Everything else is left in raw string form.
    '''
    keys = {}
    values = {}
    with open(filename, 'r') as f:
        for line in f:
            match = re.match('^# ([A-Za-z0-9_]+)=(.*)$', line)
            if match:
                keys[match.group(1)] = decode(match.group(1), match.group(2))
            else:
                match = re.match('^([A-Za-z0-9_]+)=(.*)$', line)
                if match:
                    values[match.group(1)] = decode(match.group(1), match.group(2))
    return keys, values

def matches(require, keys):
    for k in require.keys():
        if k not in keys or keys[k] != require[k]:
            return False
    return True

def main():
    require = {}
    for kv in sys.argv[1:]:
        match = re.match('^([A-Za-z0-9_]+)=(.*)', kv)
        if not match:
            print("Invalid argument", kv, file = sys.stderr)
            return 1
        k = match.group(1)
        v = match.group(2)
        if k in int_names:
            require[k] = int(v)
        else:
            require[k] = v

    home = os.environ['HOME']
    cachedir = os.path.join(home, '.clogs', 'cache')
    for filename in os.listdir(cachedir):
        keys, values = parse_cache_file(os.path.join(cachedir, filename))
        if matches(require, keys):
            print(filename)
            for k in keys.keys():
                print(k, '=', keys[k])
            for k in values.keys():
                if k != 'PROGRAM_BINARY':
                    print(k, '=', values[k])
            print()
    return 0

if __name__ == '__main__':
    sys.exit(main())
