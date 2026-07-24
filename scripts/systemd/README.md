# CEP app systemd services

These unit files let systemd manage each CEP app independently. If one app exits,
systemd restarts only that app.

## Install

Copy the service files to the device:

```sh
cp CEP-*.service /etc/systemd/system/
systemctl daemon-reload
```

Enable and start the services:

```sh
systemctl enable --now CEP-North_CEP.service
systemctl enable --now CEP-North_101.service
systemctl enable --now CEP-North_104.service
systemctl enable --now CEP-North_Mqtt.service
systemctl enable --now CEP-LogicCenter.service
systemctl enable --now CEP-Modbus.service
systemctl enable --now CEP-IEC104.service
systemctl enable --now CEP-DLT645.service
```

Disable the old monitor service after confirming these services work:

```sh
systemctl disable --now CEP_Init.service
```

## Common commands

```sh
systemctl status CEP-North_CEP.service
systemctl restart CEP-North_CEP.service
systemctl show CEP-North_CEP.service -p ActiveState -p SubState -p MainPID --no-page
journalctl -u CEP-North_CEP.service -n 100 --no-pager
```

## Notes

- These services use `Restart=always`, so each app is restarted independently.
- `KillMode=control-group` makes stop/restart clean up child processes owned by
  the service.
- The `ExecStart` commands include `-debug` to match the current
  `/home/cepgateway/app/enable_monitor.sh` style.

## Persistent network configuration

`cepb-network.service` is intentionally ordered after the factory
`cfg-apply.service`. The packaged `cfg-apply-oneshot.conf` drop-in changes the
factory service to `Type=oneshot` without editing its shipped unit, so systemd
waits for `/usr/bin/cfg-apply` to finish before applying the persistent user
configuration.

Install the network helper and unit:

```sh
install -m 0755 cepb-network-apply /usr/local/sbin/cepb-network-apply
install -m 0644 cepb-network.service /etc/systemd/system/cepb-network.service
mkdir -p /etc/systemd/system/cfg-apply.service.d
install -m 0644 cfg-apply-oneshot.conf \
  /etc/systemd/system/cfg-apply.service.d/10-cepb-network-ordering.conf
mkdir -p /etc/cepb
install -m 0644 network.conf.example /etc/cepb/network.conf
/usr/local/sbin/cepb-network-apply check /etc/cepb/network.conf
systemctl daemon-reload
test "$(systemctl show cfg-apply.service -p Type --value)" = oneshot
systemctl enable cepb-network.service
```

The release package also includes `device/install-cepb-network`. It performs the
same installation, preserves an existing `/etc/cepb/network.conf`, and does not
start the service automatically:

```sh
chmod +x device/install-cepb-network device/cepb-network-apply
device/install-cepb-network systemd/cepb-network.service \
  device/cepb-network-apply device/network.conf.example \
  systemd/cfg-apply-oneshot.conf
```

Do not run the apply command remotely until the target addresses and the current
management interface have been checked. Changing the SSH-facing address drops
the current connection. To apply immediately:

```sh
systemctl restart cepb-network.service
cat /run/cepb-network-apply.status
```

If `/etc/cepb/network.conf` does not exist, the service is skipped and the
factory addresses created by `cfg-apply` remain active.

The apply helper waits up to 30 seconds for every interface referenced by the
configuration. Its latest runtime result is written to
`/run/cepb-network-apply.status` as `STATE|running`, `STATE|success`, or
`STATE|failed`, followed by the command output.

ControlCenter verifies the uploaded helper, unit, drop-in, and configuration by
size and SHA-256 before replacing the installed files. Replacement uses
same-filesystem temporary files followed by `mv`, and `sync` completes before
the UI reports success so an immediate power cycle cannot leave zero-length
network files behind.
