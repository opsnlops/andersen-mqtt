#!/bin/sh
# Check that andersen-mqtt exits the right way when its connections go away, so
# systemd (Restart=always) can bring it back. Everything runs on 127.0.0.1 with
# a local mosquitto and a fake gateway that never answers; never point this at
# the real broker. Needs mosquitto, mosquitto-clients and netcat-openbsd.
set -eu

work=$(mktemp -d)
broker_port=18830
gateway_port=16000

cat > "$work/config.json" <<JSON
{
  "mqtt": {"host": "127.0.0.1", "port": $broker_port},
  "gateway": {"host": "127.0.0.1", "port": $gateway_port},
  "pollIntervalSeconds": 1,
  "logLevel": "debug"
}
JSON

nc -lk 127.0.0.1 "$gateway_port" > /dev/null &
gateway=$!

start_broker() {
    mosquitto -p "$broker_port" > "$work/mosquitto.log" 2>&1 &
    broker=$!
    sleep 1
}

# Start the bridge under a hard time limit, so a hang fails the check rather
# than the build. timeout passes TERM on to it.
start_bridge() {
    : > "$work/bridge.log"
    timeout -s KILL "$1" andersen-mqtt --config "$work/config.json" > "$work/bridge.log" 2>&1 &
    bridge=$!
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        grep -q 'subscribe acknowledged' "$work/bridge.log" && return 0
        sleep 1
    done
    cat "$work/bridge.log"
    echo "the bridge never subscribed" >&2
    exit 1
}

expect_exit() {
    status=0
    wait "$bridge" || status=$?
    if [ "$status" -ne "$1" ] || ! grep -q "$2" "$work/bridge.log"; then
        cat "$work/bridge.log"
        echo "expected exit $1 and '$2', got exit $status" >&2
        exit 1
    fi
}

echo "SIGTERM stops it cleanly"
start_broker
start_bridge 20
kill -TERM "$bridge"
expect_exit 0 'stopped$'

echo "Losing the broker is a failure"
start_bridge 20
kill "$broker"
wait "$broker" || true
expect_exit 1 'MQTT.*shutting down'

echo "Another client with the same ID is a failure"
start_broker
start_bridge 20
timeout 3 mosquitto_sub -h 127.0.0.1 -p "$broker_port" -i andersen-mqtt -t unused || true
expect_exit 1 'MQTT.*shutting down'

echo "A gateway that never answers is a failure"
start_bridge 60
expect_exit 1 'no status from the window controller'

kill "$broker" "$gateway" 2>/dev/null || true
echo "Connection checks passed."
