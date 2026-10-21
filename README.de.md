# LLDP als externe ESPHome-Komponente

**Ohne Dateien zu kopieren oder Terminal:** [YAML im Webeditor einfügen und mit ESPHome Web erstflashen](docs/WEB_EDITOR.de.md). Vollständige Vorlage ohne `secrets.yaml` und mit automatisch geladenen GitHub-Komponenten.

[English](README.md) · [Vollständiges Beispiel](examples/usb-p2c5v-lldp.yaml) · [Alle Optionen](docs/LLDP.md)

LLDP zeigt in Home Assistant den direkt angeschlossenen Switch/Nachbarn und sendet eine einstellbare eigene Geräteidentität. Ein Schalter aktiviert/deaktiviert LLDP. Systemname, Portbeschreibung und Systembeschreibung können über normale Textentitäten verändert werden.

## Voraussetzung

Dieses Repository enthält **nur die LLDP-Komponente**. Sie benötigt derzeit die Layer-2-Schnittstelle aus [mvs90/esphome-usb-ethernet](https://github.com/mvs90/esphome-usb-ethernet) mit `USB_RTL8152` oder `USB_ECM`. Sie funktioniert nicht automatisch mit allen normalen ESPHome-Ethernettreibern.

Die geprüfte Kombination ist ESPHome **2026.9.1**, ESP-IDF **5.5.5**, Ethernet **v0.1.0-rc.5** und LLDP **v0.1.0-rc.1**. Im vorhandenen USB-Ethernet-Projekt zwei Quellen laden:

```yaml
external_components:
  - source: github://mvs90/esphome-usb-ethernet@v0.1.0-rc.5
    components: [ethernet, ethernet_info, logger]
  - source: github://mvs90/esphome-lldp@v0.1.0-rc.1
    components: [lldp]
```

Dann LLDP an die vorhandene Ethernet-ID binden; keinen zweiten Ethernet-Block erzeugen:

```yaml
lldp:
  id: lldp_agent
  ethernet_id: usb_ethernet
  mode: RX_TX
  enabled: true
  system_name: Mein-ESPHome-Geraet
  port_id: eth0
  port_description: USB Ethernet
  system_description: ESPHome PoE
  transmit_interval: 30s
  ttl: 120
```

Wer neu beginnt, kopiert das [vollständige Beispiel](examples/usb-p2c5v-lldp.yaml), legt daneben eigene `secrets.yaml` an und folgt der [Ethernet-Einrichtungsanleitung](https://github.com/mvs90/esphome-usb-ethernet/blob/v0.1.0-rc.5/README.de.md). InLine und P2C5V benutzen denselben nativen Backendtyp; Platine und Chiprevision müssen zum getesteten Aufbau passen.

## Anzeige und eigene Angaben

Nach dem Verbinden mit Home Assistant erscheinen ein LLDP-Schalter und elf Diagnose-Textsensoren: unter anderem Switchname, Chassis-/Port-ID, Beschreibung, Managementadresse, Quell-MAC, Status und Statistik. Es wird ein direkt angeschlossener Nachbar gespeichert. Linkverlust, Abschaltung oder abgelaufene TTL entfernen die Nachbardaten.

Das vollständige Beispiel ergänzt **LLDP Own System Name**, **LLDP Own Port Description** und **LLDP Own System Description** als editierbare Textfelder. Änderungen dort sind **flüchtig**. Nach einem Neustart gelten wieder die Angaben aus YAML. Die LLDP-Managementadresse ist eine ausdrücklich konfigurierte Ankündigung und folgt DHCP nicht automatisch.

## UniFi-Portnummern

Rohe Port-ID und Beschreibung bleiben sichtbar. Die optionale Ableitung einer physischen Portnummer benötigt eine ausdrücklich kalibrierte Regel für genau den eigenen Switch und dessen Firmware. Die MAC-/Port-Beziehung an mehreren bekannten Ports prüfen; keine fremde Kalibrierung kopieren. Das Beispiel im englischen README verwendet ausschließlich synthetische MACs und muss angepasst werden. Aus dem Markennamen UniFi wird keine allgemeine Portformel abgeleitet.

## Grenzen und Entwicklung

LLDP ist lokal zum Ethernetsegment; auf einem anderen normalen Switchport sieht ein Mac diese Frames meist nicht. Für einen Sendenachweis braucht es einen passenden Spiegelport. LLDP-MED und beliebige ESPHome-Ethernettreiber sind nicht allgemein implementiert. Hardwaregrenzen des darunterliegenden Ethernettreibers gelten weiterhin.

Drei Sanitizer-Suiten lassen sich mit `python3 tools/run_cpp_tests.py` ausführen. Die CI baut das kombinierte Beispiel mit synthetischen Zugangsdaten; solche Images niemals flashen. Lizenz: C/C++ GPL-3.0-only, Python MIT, siehe [LICENSE](LICENSE).
