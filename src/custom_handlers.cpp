#include "plugin.h"

StdLogosResult Libp2pModuleImpl::mountProtocol(const std::string& proto) {
    if (!hasCtx()) return {false, {}, "No libp2p context"};
    if (proto.empty()) return {false, {}, "Protocol string is empty"};
    publishEmitEvent();

    return callSync("Failed to mount protocol", [&](SyncPromise* p) {
        return libp2p_ctx_mount_protocol(ctx, nimffi_str(proto.c_str()),
                                        &Libp2pModuleImpl::cbBool, p);
    });
}
