# LLDP über USB-Ethernet

Die optionale externe Komponente `lldp` sendet und empfängt klassisches LLDP
(EtherType0x88CC, nächster Bridge-Nachbar01:80:C2:00:00:0E). Sie verwendet die
begrenzte Layer2-Schnittstelle des mitgelieferten `ethernet`-Overlays, unabhängig
von DHCP, IPv4 oder IPv6. Im Realtek-Registertreiber steckt keine LLDP-Protokolllogik.
Derzeit sind die USB_ECM- und USB_RTL8152-Pfade dieses Projekts unterstützt.
Dies ist keine allgemeine LLDP-Implementierung für alle ESPHome-Ethernettreiber.

## Konfiguration

`lldp` zusätzlich in `external_components.components` aufnehmen. Beispiel:

```yaml
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

Modi: RX_TX, RX_ONLY und TX_ONLY. Standard30s Sendeintervall, TTL120s;
TTL muss mindestens doppelt so lang wie das Sendeintervall sein. Ohne
`chassis_id` wird die tatsächlich verwendete Ethernet-MAC angekündigt.
`system_name` verwendet ohne Angabe den ESPHome-Gerätenamen. Optionale eigene
`chassis_id` und `port_id` verwenden lokale bzw. lokale Port-IDs; die empfangenen
Subtype-Angaben werden separat nach Standard dekodiert. Eigene Fähigkeiten
`capabilities` und `enabled_capabilities` sind16-Bit-Masken, standardmäßig128
(Station Only). Aktivierte Fähigkeiten müssen eine Teilmenge sein.
`management_address` kann optional eine eigene IPv4- oder IPv6-Adresse enthalten;
sie ist eine konfigurierte Ankündigung, keine automatische DHCP-Synchronisierung.
Ein nicht belegbarer ifIndex wird nicht erfunden.

Alle Identitätstexte sind auf255UTF8-Bytes begrenzt. YAML mit längeren Texten
oder Steuerzeichen wird abgewiesen. Laufzeitsetter bereinigen und begrenzen Text
an gültigen UTF8-Grenzen. Das Betriebsbeispiel enthält zusätzlich normale
ESPHome-Textentitäten für eigenen Systemnamen, Port- und Systembeschreibung.
Diese Laufzeitänderungen sind flüchtig; nach Neustart gelten die YAML-Werte.

## Entitäten und Lebenszyklus

Automatisch entstehen ein Aktivierungsschalter `LLDP` und elf Diagnose-
Textsensoren: Nachbar-Systemname, Chassis-ID, Port-ID, Portbeschreibung,
Systembeschreibung, Managementadresse, Status, Statistik sowie Quell-MAC,
abgeleitete physische Portnummer und Portzuordnungsstatus. Ihre Namen und
Entity-Optionen können über `enable_switch`, `neighbor_system_name`,
`neighbor_chassis_id`, `neighbor_port_id`, `neighbor_port_description`,
`neighbor_system_description`, `neighbor_management_address`, `status` und
`statistics`, `neighbor_source_mac`, `neighbor_physical_port` und
`neighbor_port_mapping` überschrieben werden. HA übernimmt sie in das vorhandene ESPHome-Gerät.

Ein physischer Punkt-zu-Punkt-Nachbar wird gespeichert, kein vollständiger
Nachbarkatalog mehrerer LLDP-Agenten. Linkverlust oder Deaktivierung leert die
Nachbardaten; Ablauf der angekündigten TTL meldet `expired`. TTL0 entfernt nur
die passende Chassis-/Portidentität. Ungültige Frames erneuern die TTL nicht.
`disabled`, `link_down`, `waiting`, `neighbor_present` und `expired` sind die
Statuswerte. RX_ONLY sendet keine Ankündigungen; TX_ONLY pflegt keine Nachbardaten.

RX hat zwei feste Slots; der Callback kopiert begrenzt und allokiert keinen
Speicher. Parsing und Veröffentlichung laufen in der ESPHome-Hauptschleife.
`queue_drop` zählt Überläufe, `invalid` abgewiesene Frames. `tx_queued` bedeutet
Annahme durch den lokalen Sendepfad, keinen bestätigten Empfang am Switch.
`tx_rejected` zählt lokal abgelehnte Sendeversuche. Erfolgloses Senden wird im
Abstand von mindestens1s wieder versucht. Eine Deaktivierung sendet bei aktivem
TX möglichst eine TTL0-Abmeldung; eine Zustellgarantie bei Shutdown gibt es nicht.

Bei selektiven Realtek-Multicastfiltern wird die LLDP-Zielgruppe zusammen mit den
Basisgruppen erhalten, sofern `preserve_basic_groups: true` gesetzt ist.
Bei bewusster Abschaltung dieser Gruppen muss die LLDP-MAC ausdrücklich in die
Adressliste aufgenommen werden. LLDP bleibt lokal zum Ethernetsegment und wird
nicht an einen Mac hinter einem weiteren Switch weitergeleitet.

## Aussagekraft und Grenzen

LLDP besitzt keine kryptographische Authentifizierung. Empfangene Texte sind
Angaben des Nachbarn, keine verifizierte Modell-/Portdatenbank. Der getestete
USW Pro XG24PoE kündigt am bestätigten physischen Port9 die Interface-ID
`te1/0/1` und eine generische Modellbeschreibung an. Unabhängige Rohframe-
Decodierung bestätigt diese Werte; der Treiber ersetzt sie nicht durch
vermeintlich richtige Herstellerwerte. Die Originalwerte bleiben sichtbar. Eine optionale, ausdrücklich kalibrierte
Zuordnung ergänzt die physische Portnummer; sie ersetzt keine empfangenen TLVs.

LLDP-MED, DCBX, Leistungsanforderungen für PoE und organisatorische Erweiterungen
werden nicht erzeugt oder als vollständige Featurefamilie unterstützt.
Unbekannte, korrekt begrenzte TLVs werden übersprungen. Empfangene
Managementadressen werden nur angezeigt; die Komponente verbindet sich nicht zu ihnen.
LLDP-Empfang, Laufzeit-Aktivierung und tatsächliches Senden wurden an echter
Hardware geprüft. Ein120s-Mitschnitt am Switch-Spiegelport enthält vier korrekte
ESP-Ankündigungen mitTTL120 und30s-Intervall sowie vier Switch-Ankündigungen;
keine Kernel-Drops. Das belegt Ankunft am Switch-Port, nicht die Verarbeitung
in einer nicht auslesbaren Nachbardatenbank des Switches.
Wire-Goldens, fehlerhafte TLVs, TTL, RX-/TX-Modi und Zustandswechsel zusätzlich
unter ASan/UBSan. Die vollständige Betriebsabnahme steht in
RESULTS.md bzw. TEST_STATUS.md. Ein Firmware-Build allein ist kein Wire-Test.

## Kalibrierte UniFi-Portzuordnung

Am vorhandenen USW Pro XG 24 PoE wurde an den physischen Ports 7, 9, 20 und 21
unabhängig mit LLDP-Paketmitschnitten bestätigt:
`physischer Port = 48-Bit(Quell-MAC) - 48-Bit(Chassis-MAC)`.
Dies ist ein Testbefund dieses Switches, keine allgemeine UniFi-Garantie.
Die SFP-Uplinks und andere Modelle wurden nicht damit geprüft. Nach Änderungen
an Switch-Firmware oder MAC-Zuordnung sollte die Kalibrierung erneut geprüft werden.

Die Zuordnung ist standardmäßig deaktiviert. Für einen selbst kalibrierten
Switch kann sie explizit gesetzt werden (folgende MAC ist ein Beispiel):

```yaml
lldp:
  unifi_port_mapping:
    - chassis_mac: "02:00:00:00:00:10"
      port_count: 24
      # port_mac_base: "02:00:00:00:00:10"  # Standard: chassis_mac
```

Maximal acht Regeln, 1–256 Ports je Regel. Die Rechnung berücksichtigt Überträge
über alle sechs MAC-Bytes. Nur ein binärer MAC-Chassis-Identifier und eine gültige
Unicast-Quell-MAC passen auf eine Regel. Unbekannte Switches, eine Quelle unterhalb
der Basis und Differenzen außerhalb 1..port_count liefern keine Portnummer.
Die drei zusätzlichen Diagnosen zeigen Quell-MAC, physische Portnummer und
Zuordnungsstatus (`calibrated_source_mac_offset`, `disabled`,
`uncalibrated_switch`, `non_mac_chassis`, `outside_calibrated_range` oder
`no_neighbor`). Linkverlust leert die Portnummer.

LLDP ist nicht authentifiziert: Auch eine plausible Zuordnung eignet sich nicht
als Sicherheitsnachweis oder zur automatischen Steuerung von PoE/Ports.
Die Korrektur gehört in diese LLDP-Auswertung, nicht in den Realtek-Registerpfad
oder in die Home-Assistant-UniFi-Integration. Auf dem Testboard und in HA wurde
die zusätzliche Portnummer 9 mit unveränderter Original-ID te1/0/1 geprüft.
