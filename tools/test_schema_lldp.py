#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate LLDP code generation/schema using the pinned existing ESPHome environment."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);a=p.parse_args();base=a.base.resolve()
source=(base/'usb-rtl8152.yaml').read_text().split('\nlldp:\n',1)[0].replace('components: [ethernet, logger, ethernet_info]','components: [ethernet, logger, ethernet_info, lldp]')
cases={'default':('',True,''),'rx-only':('mode: RX_ONLY',True,''),'tx-only':('mode: TX_ONLY',True,''),'off':('enabled: false',True,''),'identity':('chassis_id: rack-A\nport_id: usb0\nsystem_name: Testgerät\nport_description: PoE\nsystem_description: ESPHome',True,''),'management4':('management_address: 192.0.2.1',True,''),'management6':('management_address: 2001:db8::1',True,''),'custom-entities':('neighbor_system_name:\n  name: My LLDP Peer\nenable_switch:\n  name: My LLDP',True,''),'max-utf8':('system_name: '+('ä'*127)+'a',True,''),'too-long-utf8':('system_name: '+('ä'*128),False,'255 UTF-8'),'empty-port':('port_id: ""',False,'must not be empty'),'control-text':('system_name: "bad\\nvalue"',False,'control characters'),'bad-mode':('mode: NONE',False,'mode'),'short-ttl':('ttl: 59',False,'twice'),'zero-ttl':('ttl: 0',False,'at least'),'huge-ttl':('ttl: 65536',False,'at most'),'fast-interval':('transmit_interval: 1s',False,'at least'),'slow-interval':('transmit_interval: 2h',False,'at most'),'matching-long-ttl':('transmit_interval: 1h\nttl: 7200',True,''),'bad-capability-subset':('capabilities: 4\nenabled_capabilities: 128',False,'subset'),'bad-management':('management_address: not-an-ip',False,'IP')}

rule='unifi_port_mapping:\n  - chassis_mac: "02:00:00:00:00:10"\n    port_count: 24'
cases.update({
 'unifi-rule':(rule,True,''),
 'unifi-rule-base':(rule+'\n    port_mac_base: "02:00:00:00:00:01"',True,''),
 'unifi-disabled':('unifi_port_mapping: []',True,''),
 'unifi-multicast':(rule.replace('02:00','03:00'),False,'unicast'),
 'unifi-zero':(rule.replace('02:00:00:00:00:10','00:00:00:00:00:00'),False,'unicast'),
 'unifi-zero-ports':(rule.replace('port_count: 24','port_count: 0'),False,'at least'),
 'unifi-many-ports':(rule.replace('port_count: 24','port_count: 257'),False,'at most'),
 'unifi-duplicate':(rule+'\n  - chassis_mac: "02:00:00:00:00:10"\n    port_count: 24',False,'Duplicate'),
 'unifi-rule-limit':('unifi_port_mapping:\n'+''.join('  - chassis_mac: "02:00:00:00:00:'+format(i,'02X')+'"\n    port_count: 24\n' for i in range(1,10)),False,'At most8'),
})

for name,(settings,valid,error) in cases.items():
 path=base/('schema-lldp-'+name+'.yaml');assert not path.exists()
 try:
  path.write_text(source+'\nlldp:\n'+''.join('  '+line+'\n' for line in settings.splitlines()))
  r=subprocess.run(['esphome','config',str(path)],capture_output=True,text=True)
  assert (r.returncode==0)==valid,name+': unexpected acceptance/rejection'
  assert valid or error.lower() in (r.stdout+r.stderr).lower(),name+': wrong validation reason'
  print('PASS',name,flush=True)
 finally:path.unlink(missing_ok=True)
