#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
components=root/"components"
with tempfile.TemporaryDirectory(prefix='lldp-tests-') as folder:
 for name in ('protocol','component','unifi'):
  binary=str(Path(folder)/name)
  cmd=[os.environ.get('CXX','clang++'),'-std=c++17','-fsanitize=address,undefined','-I',str(components/'lldp')]
  if name=='protocol':cmd += [str(root/'tests/test_lldp.cpp')]
  elif name=='unifi':cmd += ['-Wall','-Wextra','-Werror',str(root/'tests/test_lldp_unifi.cpp')]
  else:cmd += ['-I',str(root/'tests/lldp_stubs'),str(root/'tests/test_lldp_component.cpp'),str(components/'lldp/lldp_component.cpp')]
  subprocess.run(cmd+['-o',binary],check=True)
  fixtures=['lldp-node.bin','lldp-switch.bin'] if name=='protocol' else (['lldp-switch.bin'] if name=='component' else [])
  subprocess.run([binary]+[str(root/'tests/fixtures'/f) for f in fixtures],check=True)
  print('PASS lldp-'+name,flush=True)
