# andersen-mqtt

Bridges an Andersen window controller to MQTT. It talks to the controller
through a TCP serial gateway, polls the status of its four windows, and
publishes each window's state to an MQTT broker as retained topics. Commands
published to a window's `command` topic open, close or stop it.

It ships as a Debian package for amd64 and arm64, with a systemd service.
`man 8 andersen-mqtt` covers every option, the configuration file and the
topics.

## How it reaches the windows

andersen-mqtt never opens a serial port, so there is no serial device in its
configuration. The window controller's RS-232 link is plugged into a USB serial
adapter on home-automation, and [ser2net](https://github.com/cminyard/ser2net)
shares it as a raw TCP connection on port 6000. andersen-mqtt connects there
(`gateway.host` and `gateway.port`).

The serial settings live in ser2net's configuration, `/etc/ser2net.yaml`:

```yaml
connection: &con06000
  accepter: tcp,6000
  enable: on
  options:
    kickolduser: true
  connector: serialdev,
             /dev/ttyUSB0,
             9600n81,local
```

- The line is **9600 baud, 8N1, no flow control**. `local` tells ser2net to
  ignore the modem control lines.
- The accepter has to be plain `tcp`, not `telnet`. The window protocol is
  binary, and telnet negotiation would corrupt it.
- `kickolduser: true` means a new connection drops the old one. Only run one
  copy of andersen-mqtt against the gateway, or the two will keep kicking each
  other off. The same goes for testing from another machine.

Until January 2025 the program opened the USB serial adapter directly. It moved
to ser2net because that was easier to run in a container, and it keeps a better
separation of concerns: ser2net owns the serial hardware, and andersen-mqtt only
speaks the window protocol.

## Configuration

The service reads `/etc/andersen-mqtt/andersen-mqtt.json`:

```json
{
  "mqtt": {
    "host": "10.3.2.5",
    "port": 1883,
    "clientId": "andersen-mqtt",
    "topicPrefix": "andersen-mqtt/windows"
  },
  "gateway": {
    "host": "10.3.2.5",
    "port": 6000
  },
  "pollIntervalSeconds": 5,
  "logLevel": "info"
}
```

Every key is optional and defaults to the value above. An unknown key, or a
value of the wrong type, stops the program at startup. Hosts may be names or
addresses.

Command-line options: `--config FILE`, `--log-level LEVEL`, `--version`,
`--help`.

## Topics

For each of `window1` to `window4`, under `<topicPrefix>/<window>/`:

- published (retained): `open`, `movement_obstructed`, `screen_missing`,
  `rf_heard`, `rain_sensed`, `rain_override_active` (`yes`/`no`), and
  `last_polled` (ISO 8601)
- subscribed: `command`, where a message starting with `o` opens, `c` closes,
  and `s` stops (any case, so Home Assistant's `OPEN`/`CLOSE`/`STOP` work).
  Retained commands are ignored, since the broker would replay them on every
  start.

## Building the package

The package is built inside `debian:trixie` with CPack, then installed and
checked in a clean Debian container. You need Docker.

```bash
./make_packages.sh
```

This builds for the machine's own architecture and leaves the `.deb` in
`package/`. To see lintian's findings:

```bash
./report_lintian.sh
```

GitHub Actions runs the same script on amd64 and arm64 runners and uploads
each `.deb` as a build artifact, ready to copy into the local apt repository.

Bump `VERSION` in `CMakeLists.txt` for every release, or apt won't see a new
package.

## Service

Installing the package enables and starts `andersen-mqtt.service`, and
upgrading restarts it.

The program doesn't reconnect by itself. If it loses the broker or the gateway,
or the window controller stops answering for three poll intervals (at least 30
seconds), it exits with a failure status and systemd starts it again 10 seconds
later.

```bash
journalctl -u andersen-mqtt.service -f
```

Override the unit with a drop-in under
`/etc/systemd/system/andersen-mqtt.service.d/`.
