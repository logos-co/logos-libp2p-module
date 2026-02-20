# logos-libp2p-module

`logos-libp2p-module` is a Logos module that integrates libp2p networking capabilities (via [nim-libp2p](https://github.com/vacp2p/nim-libp2p) C [bindings](https://github.com/vacp2p/nim-libp2p/tree/master/cbind)) into the Logos ecosystem.

It provides:
- Peer connectivity
- Stream management
- Kademlia DHT operations
- Sync and async APIs compatible with Qt

Check the examples/ directory for complete usage demonstrations.

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
