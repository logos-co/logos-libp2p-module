# Tutorials


Step-by-step tutorials for the `logos-libp2p-module` — from basic node lifecycle to NAT traversal with circuit relay.

## Build once, run many

The tutorials are compiled alongside the module. Build them **one time**, then run any tutorial immediately:

```bash
# Build once
nix develop
./tutorial/build_tutorials.sh

# or use following command if you are missing experimental Nix features:
nix --extra-experimental-features 'nix-command flakes' develop --command ./tutorial/build_tutorials.sh


# Run any tutorial:
./build/tutorial/tutorial_1_node_lifecycle
./build/tutorial/tutorial_3_connecting_peers
```

**Build prerequisites**
- Nix development shell (`nix develop`) or equivalent dependencies

No need to rebuild between runs unless the source code changes.

## Workflow

To get the most out of these tutorials, it is strongly recommended to follow this workflow:

1. **Read** a tutorial page (start with [Tutorial 1](docs/tutorial_1_node_lifecycle.md)).
2. **Run** the matching binary to see it in action.
   - Every tutorial page includes a command for running the tutorial's executable binary.
   - Observe the stdout output.
3. **Read the source** in the `.cpp` file for the full code.
   - Some tutorials include exercises – it is highly advised to try them.
   - After editing the code, rebuild the tutorials using the same command from the *Build once, run many* section before running the updated binary.


## Regenerate Markdown

The `docs/` folder is generated from `///` comments in the `.cpp` sources:

```bash
./tutorial/generate_markdown.sh
```

---

<div align="center">
<a href="docs/tutorial_1_node_lifecycle.md"><b>START HERE — Tutorial 1: Node Lifecycle</b></a>
</div>
