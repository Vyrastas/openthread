# OpenThread CLI - Joiner

## Quick Start

See [README_COMMISSIONING.md](README_COMMISSIONING.md).

## Command List

Usage : `joiner [command] ...`

- [help](#help)
- [discerner](#discerner)
- [id](#id)
- [start](#start)
- [state](#state)
- [stop](#stop)

## Command Details

### help

Usage: `joiner help`

Print joiner help menu.

```bash
> joiner help
help
id
start
state
stop
Done
```

### discerner

Usage: `joiner discerner`

Print the Joiner Discerner. Note this value takes the place of the place of EUI-64 during the joiner session of Thread commissioning.

```bash
> joiner discerner
0xabc/12
Done
```

Usage: `joiner discerner <number/length>`

Set the Joiner Discerner.

```bash
> joiner discerner 0xabc/12
Done
```

Usage: `joiner discerner clear`

Clear the Joiner Discerner.

```bash
> joiner discerner clear
Done
```

### id

Usage: `joiner id`

Print the Joiner ID.

```bash
> joiner id
d65e64fa83f81cf7
Done
```

### start

Usage: `joiner start <pskd> [provisioning-url]`

Start the Joiner role.

- pskd: Pre-Shared Key for the Joiner.
- provisioning-url: Provisioning URL for the Joiner (optional).

This command will cause the device to start the Joiner process.

```bash
> joiner start J01NM3
Done
```

### state

Usage: `joiner state`

Print the Joiner state.

- Idle
- Discover
- Connecting
- Connected
- Entrust
- Joined

```bash
> joiner state
Idle
Done
```

### stop

Usage: `joiner stop`

Stop the Joiner role.

```bash
> joiner stop
Done
```
