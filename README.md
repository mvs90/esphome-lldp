# LLDP for ESPHome USB Ethernet

[Deutsch](README.de.md) · [Full configuration](examples/usb-p2c5v-lldp.yaml) · [Protocol/options](docs/LLDP.md) · [Ethernet dependency](https://github.com/mvs90/esphome-usb-ethernet)

An optional LLDP agent for ESPHome: show the directly connected switch in Home Assistant, advertise a configurable device identity, and enable/disable LLDP from an entity.

**This repository contains only LLDP.** It currently needs the Layer-2 interface provided by **mvs90/esphome-usb-ethernet** and supports that overlay's `USB_RTL8152` and `USB_ECM` backends. It is **not** a generic add-on for every stock ESPHome Ethernet driver. The protocol implementation is separate from the Realtek register driver.

Pinned/tested combination: **ESPHome2026.9.1 / ESP-IDF5.5.5**, Ethernet **v0.1.0-rc.5**, LLDP **v0.1.0-rc.1**. Hardware integration was tested with the Waveshare ESP32-S3-LCD-Driver-Board and measured InLine33380J POE01 / P2C5V RTL8152-4C10 adapters. Do not infer compatibility with another board or silicon revision.

## Add LLDP to an existing USB-Ethernet project

Keep your existing board, UART, API/OTA, and single Ethernet instance. Load components from **both** pinned sources:

```yaml
external_components:
  - source: github://mvs90/esphome-usb-ethernet@v0.1.0-rc.5
    components: [ethernet, ethernet_info, logger]
  - source: github://mvs90/esphome-lldp@v0.1.0-rc.1
    components: [lldp]

# Existing block; do not duplicate it.
ethernet:
  id: usb_ethernet
  type: USB_RTL8152

lldp:
  id: lldp_agent
  ethernet_id: usb_ethernet
  enabled: true
  mode: RX_TX
  system_name: ESPHome-PoE
  port_id: eth0
  port_description: USB Ethernet
  system_description: ESPHome network node
  transmit_interval: 30s
  ttl: 120
```

Use [usb-p2c5v-lldp.yaml](examples/usb-p2c5v-lldp.yaml) as a complete board example, with your own adjacent `secrets.yaml`. InLine uses the same native backend. For first flash, PoE wiring and recovery follow the [Ethernet setup guide](https://github.com/mvs90/esphome-usb-ethernet/blob/v0.1.0-rc.5/docs/SETUP.md). Native USB logging conflicts with the USB host; the tested example uses UART0.

## What Home Assistant displays

The component automatically creates an LLDP enable switch and eleven diagnostic text sensors:

- Neighbor system name, chassis ID, port ID, port description, system description and management address.
- Status, statistics, source MAC, derived physical port number, and port-mapping status.

Default status values are `disabled`, `link_down`, `waiting`, `neighbor_present`, and `expired`. Only one directly connected neighbor is retained. LLDP frames do not normally cross a switch, so a packet capture on a different ordinary LAN port is not proof of absence; use a correctly configured mirror if needed.

Modes are `RX_TX`, `RX_ONLY`, and `TX_ONLY`. Receive processing is independent of IP acquisition, but the current backend must have link. TTL must be at least twice the transmit interval. Invalid frames do not renew a neighbor's lifetime. Local send counters show queue acceptance, not confirmed reception by the switch.

## Editable local identity

YAML identity fields are the startup defaults. The complete example also exposes ordinary ESPHome `text` entities for **Own System Name**, **Own Port Description**, and **Own System Description**. Edit them on the ESPHome device page in Home Assistant. Runtime changes are **volatile**: after restart, YAML values apply again.

Without a custom chassis ID the active Ethernet MAC is advertised. `management_address` is an optional explicit announcement, not automatic DHCP synchronization. Text is bounded to255 UTF-8 bytes; runtime setters sanitize input. See [all options and entity overrides](docs/LLDP.md).

## UniFi physical-port mapping

Some measured UniFi firmware advertises a logical port string rather than the physical socket number. Preserve raw IDs; optional mapping is **explicitly calibrated**, not enabled by brand detection.

```yaml
lldp:
  ethernet_id: usb_ethernet
  unifi_port_mapping:
    - chassis_mac: "02:00:00:00:00:10" # synthetic; replace with verified chassis ID
      port_mac_base: "02:00:00:00:00:10" # synthetic; replace with verified base
      port_count: 24
```

Do not copy these example MACs into a live deployment. Verify the source-MAC/physical-port relationship on several known ports of your exact switch/firmware, then set its identity and range. Unsupported or out-of-range peers are left unmapped. Original port ID and port description remain available. This is not a universal UniFi MAC arithmetic rule.

## Validation and scope

Three Clang ASan/UBSan suites cover protocol parsing, the real component lifecycle, and calibrated UniFi mapping: `python3 tools/run_cpp_tests.py`. Synthetic fixture frames can be regenerated with `python3 tools/generate_lldp_fixtures.py`. `tools/test_schema_lldp.py` covers configuration guards in the pinned ESPHome environment. CI compiles the combined example with synthetic credentials; never flash that test image.

Hardware evidence includes independent mirrored transmission, neighbor receipt, runtime identity edits, enable/disable/lifetime behavior and calibration on several physical switch ports. Ethernet qualification limitations remain in the [driver's test status](https://github.com/mvs90/esphome-usb-ethernet/blob/v0.1.0-rc.5/TEST_STATUS.md), including the documented P2C5V host-link exception. No complete LLDP-MED feature set or support for every Ethernet backend is claimed.

C/C++ runtime: GPL-3.0-only; Python: MIT, with preserved upstream notices. See [LICENSE](LICENSE), [NOTICE.md](NOTICE.md), and [CONTRIBUTING.md](CONTRIBUTING.md).
