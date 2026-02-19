# Examples

Enter the development shell and configure cmake
```bash
nix develop
cmake -B build -S .
```

Build module and examples
```bash
cmake --build build -j
```

Run examples
```bash
./build/examples/example_kademlia
```
