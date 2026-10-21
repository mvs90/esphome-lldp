#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent standard-library LLDP wire goldens, not importing production code."""
import argparse,struct
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--write',action='store_true');a=p.parse_args();base=Path(__file__).resolve().parents[1]/'tests/fixtures'
def tlv(kind,value):return struct.pack('!H',(kind<<9)|len(value))+value
def wire(mac,port,name,port_desc,description,cap,management,if_subtype,if_index):
 frame=bytes.fromhex('0180c200000e')+mac+bytes.fromhex('88cc')
 frame+=tlv(1,b'\x04'+mac)+tlv(2,b'\x07'+port)+tlv(3,struct.pack('!H',120))
 frame+=tlv(4,port_desc)+tlv(5,name)+tlv(6,description)+tlv(7,struct.pack('!HH',cap,cap))
 frame+=tlv(8,b'\x05\x01'+bytes(management)+bytes([if_subtype])+struct.pack('!I',if_index)+b'\0')+tlv(0,b'')
 return frame.ljust(60,b'\0')
cases={'lldp-node.bin':wire(bytes.fromhex('020000000001'),b'eth0',b'Node-Test',b'USB Ethernet',b'ESPHome LLDP',128,(192,0,2,1),1,0),'lldp-switch.bin':wire(bytes.fromhex('020000000002'),b'Port 9',b'Switch-Test',b'Test port',b'Independent LLDP fixture',4,(192,0,2,254),2,1)}
for name,data in cases.items():
 path=base/name
 if a.write:path.write_bytes(data)
 else:assert path.read_bytes()==data,name+' differs from independent wire fixture'
 print('PASS',name)
