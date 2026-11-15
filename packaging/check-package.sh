#!/bin/sh
# Install the package in a disposable Debian container and check it. Run as root.
set -eu
package=${1:?Usage: check-package.sh andersen-mqtt.deb}

[ "$(dpkg-deb -f "$package" Package)" = andersen-mqtt ]
[ "$(dpkg-deb -f "$package" Architecture)" = "$(dpkg --print-architecture)" ]

# Debian Policy 10.1: nothing ships under an aliased top-level directory such as
# /bin. The usr-merge symlinks hide that on an installed system, so check the
# archive's own paths.
if dpkg-deb -c "$package" | awk '{print $6}' | grep -E '^\./(bin|sbin|lib[^/]*)/'; then exit 1; fi
dpkg-deb -c "$package" | awk '{print $6}' | grep -qx './usr/bin/andersen-mqtt'

apt-get update
apt-get install -y "$package" systemd
andersen-mqtt --version
andersen-mqtt --help
if ldd /usr/bin/andersen-mqtt | grep -q 'not found'; then exit 1; fi
systemd-analyze --man=no verify /usr/lib/systemd/system/andersen-mqtt.service
dpkg-query -W -f='${Conffiles}\n' andersen-mqtt | grep -q /etc/andersen-mqtt/andersen-mqtt.json

# The packaged configuration loads, and bad configuration stops the program at
# startup. The gateway port is closed here, so a good configuration gets as far
# as trying to connect, which is a different failure than a bad file.
work=$(mktemp -d)
sed -e 's/"10.3.2.5"/"127.0.0.1"/' -e 's/"port": 1883/"port": 1/' \
    /etc/andersen-mqtt/andersen-mqtt.json > "$work/good.json"
printf '{"mqtt": {"host": "127.0.0.1", "hots": "x"}}\n' > "$work/unknown.json"
printf '{"gateway": {"port": "6000"}}\n' > "$work/badtype.json"
printf '{"logLevel": "loud"}\n' > "$work/badlevel.json"
printf '{"mqtt": {"port": true}}\n' > "$work/boolport.json"
printf '{"pollIntervalSeconds": 1.5}\n' > "$work/floatpoll.json"
printf '{"gateway": {"port": 4294968296}}\n' > "$work/hugeport.json"
printf '{"pollIntervalSeconds": 100000}\n' > "$work/hugepoll.json"
printf '{"mqtt": {"topicPrefix": "windows/#"}}\n' > "$work/wildcard.json"
for bad in unknown badtype badlevel boolport floatpoll hugeport hugepoll wildcard; do
    if andersen-mqtt --config "$work/$bad.json" 2> "$work/$bad.err"; then exit 1; fi
    grep -q "andersen-mqtt:" "$work/$bad.err"
done
grep -q "unknown key 'mqtt.hots'" "$work/unknown.err"
if andersen-mqtt --config /nonexistent.json 2>/dev/null; then exit 1; fi
if timeout 20 andersen-mqtt --config "$work/good.json" > "$work/good.out" 2>&1; then exit 1; fi
grep -q 'Welcome to Andersen to MQTT' "$work/good.out"

# Debian container images drop documentation on install, so inspect the archive.
dpkg-deb -x "$package" "$work/content"
for doc in copyright changelog.Debian.gz README.md; do
    [ -f "$work/content/usr/share/doc/andersen-mqtt/$doc" ]
done
gzip -t "$work/content/usr/share/doc/andersen-mqtt/changelog.Debian.gz"

# The manual is generated from --help, so it must document every long option.
# help2man escapes hyphens in the body, so strip the roff escapes first, and
# search only from OPTIONS on, since the SYNOPSIS would hide a gap.
manual="$work/content/usr/share/man/man8/andersen-mqtt.8.gz"
gzip -t "$manual"
gzip -dc "$manual" | sed -e 's/\\-/-/g' -e 's/\\f[BIRP]//g' -e 's/\\[,/]//g' > "$work/andersen-mqtt.8"
grep -q '^\.TH ANDERSEN-MQTT "8"' "$work/andersen-mqtt.8"
for heading in OPTIONS SERVICE CONFIGURATION TOPICS FILES EXAMPLES; do
    grep -q "^\.SH $heading" "$work/andersen-mqtt.8"
done
awk '/^\.SH OPTIONS/ { found = 1; next } found && /^\.SH / { exit } found' "$work/andersen-mqtt.8" \
    > "$work/options"
options=$(andersen-mqtt --help | grep -o -- '--[a-z-]*' | sort -u)
[ "$(echo "$options" | wc -l)" -ge 4 ]
for option in $options; do
    grep -qF -- "$option" "$work/options"
done

# How it behaves when its connections fail, against a local broker
apt-get install -y --no-install-recommends mosquitto mosquitto-clients netcat-openbsd
"$(dirname "$0")/check-connections.sh"

apt-get remove -y andersen-mqtt
[ ! -e /usr/bin/andersen-mqtt ]
[ ! -e /usr/lib/systemd/system/andersen-mqtt.service ]
apt-get purge -y andersen-mqtt
[ ! -e /etc/andersen-mqtt/andersen-mqtt.json ]
printf 'andersen-mqtt package checks passed.\n'
