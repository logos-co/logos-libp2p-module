/// # Tutorial 0: Introduction and Common Patterns
///
/// Before creating nodes or connecting peers, it helps to understand the
/// conventions used by every tutorial in this series.
///
/// The C++ wrapper (`logos-libp2p-module`) exposes a single main class, `Libp2pModuleImpl`.
/// Most methods on that class return the Logos result type. In C++ module code that
/// type is named `StdLogosResult`.
///
/// `StdLogosResult` has three fields:
///
/// | Field | Meaning |
/// |-------|---------|
/// | `success` | `true` when the call completed successfully |
/// | `value` | a `nlohmann::json` value returned by the call |
/// | `error` | a diagnostic string when `success == false` |
///
/// ## Always Check `success` First
///
/// Every tutorial follows the same rule: check unsuccessful outcomes before
/// using returned data.
///
/// ```cpp
/// StdLogosResult info = node.peerInfo();
/// if (!info.success) {
///     fprintf(stderr, "Failed to get peer info: %s\n",
///             info.error.c_str());
///     return 1;
/// }
/// ```
///
/// This keeps the code predictable: errors are handled immediately, and the
/// rest of the step can assume the operation succeeded.
///
/// ## Tutorial Executables Fail Fast
///
/// These programs are examples, not long-running services. When a required
/// operation fails, they print a useful error message to `stderr` and return
/// `1`. Successful tutorials print progress to `stdout` and return `0`.
///
/// The `stdout` stream can be noisy because logs from the wrapped C binding
/// (`nim-libp2p`) library are also included. This is tracked in
/// [issue #79](https://github.com/logos-co/logos-libp2p-module/issues/79).
///
/// ## Convert JSON Values Explicitly
///
/// The `value` field is JSON. Convert it to the C++ type you need at the
/// point where you use it:
///
/// ```cpp
/// std::string peerId = info.value["peerId"].get<std::string>();
///
/// for (const nlohmann::json& addr : info.value["addrs"]) {
///     printf("  %s\n", addr.get<std::string>().c_str());
/// }
/// ```
///
/// Scalar values use `get<T>()`. Objects are accessed by key. Arrays are
/// iterated with range-for loops.
///
/// Tutorial code assumes returned JSON values have the documented type, so it
/// uses and converts values directly instead of checking the JSON type first.
///
/// ## Binary Data May Be Encoded
///
/// Some APIs carry arbitrary bytes, such as stream reads, DHT values, public
/// keys, or service discovery records. Those values may be returned as strings
/// encoded for JSON transport. The tutorials decode them before comparing or
/// printing human-readable payloads.
///
/// -----------

#include <cstdio>
#include <string>
#include "plugin.h"

int main()
{
    printf("=== Tutorial 0: Introduction and Common Patterns ===\n\n");

/// ## Step 1: Create and start a node
///
/// Even the first real operation returns a result. Check it before moving on.
    Libp2pModuleImpl node;

    StdLogosResult startRes = node.start();
    if (!startRes.success) {
        fprintf(stderr, "Failed to start node: %s\n",
                startRes.error.c_str());
        return 1;
    }
    printf("Node started\n");

/// ## Step 2: Use scalar JSON values
///
/// Some calls return a single string, number, or boolean in `value`.
    StdLogosResult version = node.getNodeInfo("Version");
    if (!version.success) {
        fprintf(stderr, "Failed to get module version: %s\n",
                version.error.c_str());
        return 1;
    }
    printf("Module version: %s\n",
           version.value.get<std::string>().c_str());

/// ## Step 3: Stop cleanly
///
/// For production code it should always be better to attempt to gracefully
/// close all resources first.
///
/// The tutorials do not always process every cleanup outcome, such as failures
/// while stopping a node or closing streams, in order to keep the example code
/// focused and easy to follow.
    StdLogosResult stopRes = node.stop();
    if (!stopRes.success) {
        fprintf(stderr, "Failed to stop node: %s\n",
                stopRes.error.c_str());
        return 1;
    }

    printf("\n=== Tutorial 0 Complete ===\n");

    return 0;
}

/// ## Summary
///
/// In this tutorial you learned the conventions used everywhere else:
///   - Most calls return `StdLogosResult`
///   - Check `success` before reading `value`
///   - Print `error` when a call fails
///   - Convert JSON values with `get<T>()`, object keys, or array iteration
///   - Return `1` from tutorial executables when required operations fail

/// ## Run tutorial
///
/// Run this tutorial now to check that your environment is set up and the
/// tutorial executable has already been built:
///
/// ```bash
/// ./build/tutorial/tutorial_0_introduction
/// ```
///
/// If that command fails because the tutorial has not been built yet, build the
/// tutorials with this command and then run it again:
///
/// ```bash
/// nix --extra-experimental-features 'nix-command flakes' develop --command ./tutorial/build_tutorials.sh
/// ```
