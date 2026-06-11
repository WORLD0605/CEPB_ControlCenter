# APP upgrade binaries

Put the latest device-side APP executable files in this directory before packaging.
The release script copies these files into `build-release/app_binaries`, and the
program upgrade button uploads them to the corresponding device path.

Expected file names:

- `ServiceChannel`
- `IEC101ServiceChannel`
- `cepLogicCenter`
- `cepmodbus`
- `cepiec104`
- `cepdlt645`

Current source location for manual updates:
`\\wsl.localhost\Ubuntu\home\world\project\cepAPPNew\bin`
