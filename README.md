# logos-libp2p-module

`logos-libp2p-module` is a Logos module that integrates libp2p networking capabilities (via [nim-libp2p](https://github.com/vacp2p/nim-libp2p) C [bindings](https://github.com/vacp2p/nim-libp2p/tree/master/cbind)) into the Logos ecosystem.

It provides:
- Peer connectivity
- Stream management
- Kademlia DHT operations
- Gossipsub operations
- Sync and async APIs compatible with Qt

Check the examples/ directory for complete usage demonstrations.

---

# Configuration

When the module is loaded by `logoscore`, it is constructed without arguments, so
options are supplied through the `LIBP2P_MODULE_CONFIG` environment variable — set
to either inline JSON or a path to a JSON file. The config is overlaid onto the
defaults at load time; every key is optional, and an omitted key keeps its default.
If the config is unset, unreadable, not valid JSON, or has a wrong-typed field, the
module logs a warning and falls back to the full default options.

```bash
# inline
export LIBP2P_MODULE_CONFIG='{
  "addrs": ["/ip4/0.0.0.0/tcp/9000"],
  "bootstrapNodes": [
    { "peerId": "16Uiu2...", "addrs": ["/ip4/1.2.3.4/tcp/9000"] }
  ],
  "transport": "tcp",
  "privKey": "08021220..."
}'

# or a file
export LIBP2P_MODULE_CONFIG=/etc/logos/libp2p.json
```

See [`config.example.json`](./config.example.json) for a ready-to-edit sample and
the `config` section of [`metadata.json`](./metadata.json) for the full schema.
Code that constructs `Libp2pModuleImpl` directly (examples, tests) passes
`Libp2pModuleOptions` to the constructor and bypasses this path.

---

# Running a node via logoscore

The module can be driven directly from `logoscore` without any other module. A
default node is created when the module is loaded; `createNode` then rebuilds it
from a call-time config (same schema as `LIBP2P_MODULE_CONFIG`, so `@config.json`
expands to the file's contents), and `getNodeInfo` reads back node details.

First build the module's `.lgx` bundle, install it into a modules directory, and
start the daemon against that directory:

```bash
nix build '.#lgx'                       # result/ holds the .lgx bundle
lgpm --modules-dir ./modules --allow-unsigned install --file result/*.lgx
lgpm --modules-dir ./modules list       # confirm "libp2p_module" is listed

logoscore -D -m ./modules &             # start the daemon against ./modules
```

The daemon binds its socket asynchronously, so the first `load-module` can race
it — retry once if it reports an RPC failure. Then drive the node:

```bash
logoscore load-module libp2p_module
logoscore call libp2p_module createNode @config.example.json   # or inline JSON
logoscore call libp2p_module start
logoscore call libp2p_module getNodeInfo Version        # module version
logoscore call libp2p_module getNodeInfo MyBoundPorts   # bound ports, e.g. [9000]
logoscore call libp2p_module getNodeInfo PeerId         # this node's peer id
logoscore call libp2p_module getNodeInfo Multiaddrs     # full bound multiaddrs
logoscore stop
```

`createNode` is optional: if you set `LIBP2P_MODULE_CONFIG` before loading, the
node is already configured and you can `start` straight away. Calling
`createNode` tears down the existing node and builds a fresh one from the supplied
config, so issue it before `start`. It accepts inline JSON or `@config.json`
(the file's contents); wrap inline JSON in single quotes so the shell doesn't
mangle it.

`logoscore` only relays a generic "call failed" to the CLI; the specific reason
(`createNode: invalid config: …`, `libp2p_new failed: …`) is written to the
daemon's stderr, so check the daemon output (or its redirected log) when a call
fails.

A scripted version of this flow runs in CI and locally via
`nix run .#standalone-e2e` (see [`tests/README.md`](./tests/README.md)).

---

# Building
Currently the recommended and supported building way is using Nix

## Build everything (default)
```bash
nix build
```
Or explicitly
```bash
nix build '.#default'
```

The result will include:

- `/lib/libp2p_module_plugin.so` (or `.dylib` on macOS) — The Logos libp2p module plugin
- `/include/libp2p_module_api.h` — Generated module API header
- `/include/libp2p_module_api.cpp` — Generated module API implementation

## Build Individual Components

Build only the library (plugin)
```bash
nix build '.#lib'
```

Build only the generated headers
```bash
nix build '.#include'
```

See [examples](./examples) for how to build examples


# Development Shell

Enter development environment
```bash
nix develop
```

This provides:
- CMake
- Ninja
- Qt6
- Logos SDK dependencies
- Proper environment variables

If flakes are not enabled globally:

```bash
nix build --extra-experimental-features 'nix-command flakes'
```

To enable globally, add flake as a `experimental-feature` to `~/.config/nix/nix.conf`:
```bash
echo 'experimental-features = nix-command flakes' >> ~/.config/nix/nix.conf
```
