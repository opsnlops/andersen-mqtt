# andersen-mqtt

Bridges an Andersen window controller to MQTT. It talks to the controller
through a TCP serial gateway, polls the status of its four windows, and
publishes each window's state to an MQTT broker as retained topics. Commands
published to a window's `command` topic open, close or stop it.

It ships as a Debian package for amd64 and arm64, with a systemd service.
`man 8 andersen-mqtt` covers every option, the configuration file and the
topics.

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
  and `s` stops (any case, so Home Assistant's `OPEN`/`CLOSE`/`STOP` work)

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
