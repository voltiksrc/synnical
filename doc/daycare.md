# Daycare Configuration

Daycare is the name of Synnical's system configuration. It provides an optional declarative approach for users who want to reproduce their system setups.

Synnical supports both imperative and declarative configuration. Users familiar with distros such as Arch Linux and Ubuntu may prefer the imperative approach, while users who want to define their system configuration in a file can use Daycare.

## Getting Started

Synnical's configuration file is located at `/etc/daycare.toml`.

The file includes a default configuration with comments explaining the available settings and how to use them.

To apply your configuration, run:

```bash
synnical apply
```

**Warning:** Applying your configuration may modify important system settings. Review `/etc/daycare.toml` carefully before running this command.

## Configuration Reference

### System

The `[system]` section defines basic system-wide
settings such as the hostname and timezone.

#### Hostname

Example:

```bash
[system]
hostname = "crypt"
```

The `hostname` option sets the system's hostname to 
the value specified in the configuration. The value must be a string.

#### Timezone

Example: 
```bash
[system]
timezone = "America/Los_Angeles"
```

The `timezone` option sets your current timezone 
to the selected area, Synnical updates `/etc/localtime`
to point to the corresponding timezone file/setting.

