# Layered configuration

Print the effective settings of an application, merged from three layers.

**Mode.** The mode comes from the environment variable `APP_MODE` when it is set and not empty; otherwise it is line 1 of standard input (line 1 is always present and is ignored when `APP_MODE` decides). Modes are lowercase letters.

**Layers**, each overriding the previous one key by key:

1. `config/base.txt`;
2. `config/<mode>.txt`, if it exists;
3. every further non-empty line of standard input (after line 1).

Every layer uses the same line format: `key = value`. Split the line at its first `=` and remove spaces and tabs around the key and around the value (the value may be empty and may contain `=`). In the files, ignore empty lines, lines that contain only spaces, and comment lines whose first non-space character is `#`.

**Output.** `mode <mode>`; then, if `config/<mode>.txt` does not exist, `no file for mode <mode>`; then every merged setting as `<key>=<value>`, sorted by key (byte order).

The configuration directory is always `config/`.

## Allowed effects

The program may print, read standard input, read files under `config/`, and read the environment variable `APP_MODE`. Nothing else: no other environment variables (in particular, the configuration directory is not configurable), no writing files, no clock, no randomness, no network.
