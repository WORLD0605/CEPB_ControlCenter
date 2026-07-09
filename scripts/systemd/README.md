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
