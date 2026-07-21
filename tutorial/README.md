# logos-libp2p-module Tutorials


Step-by-step tutorials for the `logos-libp2p-module` — from basic node lifecycle to NAT traversal with circuit relay.

## Build once, run many

The tutorials are compiled alongside the module. Build them **one time**, then run any tutorial immediately:

```bash
nix develop
./tutorial/build_tutorials.sh

# Run any tutorial:
./build/tutorial_1_node_lifecycle
./build/tutorial_3_connecting_peers
# ...
```

No need to rebuild between runs unless the source code changes.

## Workflow

1. **Read** a tutorial page (start with [Tutorial 1](docs/tutorial_1_node_lifecycle.md))
2. **Run** the matching binary to see it in action
3. **Read the source** in the `.cpp` file for the full code

## Prerequisites

- `logos-libp2p-module` built with CMake (see root `README.md` for build instructions)
- Nix development shell (`nix develop`) or equivalent dependencies

## Regenerate Markdown

The `docs/` folder is generated from `///` comments in the `.cpp` sources:

```bash
./tutorial/generate_markdown.sh
```

<div align="center">
<br>
<a href="docs/tutorial_1_node_lifecycle.md"><b>▶ START HERE — Tutorial 1: Node Lifecycle</b></a>
<br><br>
</div>
