#include "plugin.h"

StdLogosResult Libp2pModuleImpl::mountProtocol(const std::string& proto) {
    if (!ctx) return {false, {}, "No libp2p context"};
    if (proto.empty()) return {false, {}, "Protocol string is empty"};
    // Incoming streams for this protocol are delivered via the on_incoming_stream
    // listener as protocolStream events; without emitEvent set no caller could
    // ever read, close, or release them, leaking the stream on the Nim side.
    if (!emitEvent) return {false, {}, "emitEvent must be set before mounting a protocol"};
    publishEmitEvent();

    return callSync("Failed to mount protocol", [&](SyncPromise* p) {
        return libp2p_ctx_mount_protocol(ctx, nimffi_str(proto.c_str()),
                                        &Libp2pModuleImpl::cbBool, p);
    });
}
