#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT HUP INT TERM

mkdir "$TEST_DIR/bin"
cat > "$TEST_DIR/bin/ip" <<'EOF'
#!/bin/sh
case "$*" in
    "link show eth0"|"link show eth5") exit 0 ;;
esac
printf '%s\n' "$*" >> "$CEPB_NETWORK_TEST_IP_LOG"
EOF
chmod +x "$TEST_DIR/bin/ip"

cat > "$TEST_DIR/network.conf" <<'EOF'
VERSION|1
INTERFACE|eth0|192.168.0.10|24
INTERFACE|eth5|14.14.14.30|24
ROUTE|10.20.0.0/16|192.168.0.1|eth0|100
ROUTE|14.14.14.21/32||eth5||14.14.14.30
EOF

export CEPB_NETWORK_TEST_IP_LOG="$TEST_DIR/ip.log"
export CEPB_NETWORK_STATUS_FILE="$TEST_DIR/status"
PATH="$TEST_DIR/bin:$PATH" /bin/sh "$SCRIPT_DIR/cepb-network-apply" check "$TEST_DIR/network.conf"
PATH="$TEST_DIR/bin:$PATH" /bin/sh "$SCRIPT_DIR/cepb-network-apply" apply "$TEST_DIR/network.conf"

grep -Fx "route replace 10.20.0.0/16 via 192.168.0.1 dev eth0 metric 100 proto 186" \
    "$CEPB_NETWORK_TEST_IP_LOG"
grep -Fx "route replace 14.14.14.21/32 dev eth5 src 14.14.14.30 proto 186" \
    "$CEPB_NETWORK_TEST_IP_LOG"
grep -Fx "STATE|success" "$CEPB_NETWORK_STATUS_FILE"

cat > "$TEST_DIR/legacy.conf" <<'EOF'
VERSION|1
INTERFACE|eth0|192.168.0.10|24
ROUTE|10.30.0.0/16|192.168.0.1|eth0|200
EOF
PATH="$TEST_DIR/bin:$PATH" /bin/sh "$SCRIPT_DIR/cepb-network-apply" check "$TEST_DIR/legacy.conf"

echo "cepb-network-apply tests passed"
