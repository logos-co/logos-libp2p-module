#pragma once

// The nim-ffi generated binding carries enum-typed config/params as plain
// integers, so the ordinals the Nim side expects are no longer emitted as C
// macros. They are mirrored here (values match the Nim `TransportType`,
// `MuxerType`, `Direction` and `PKScheme` enums) for the config layer, the
// wrapper and the tests to use.

enum {
    LIBP2P_TRANSPORT_QUIC = 0,
    LIBP2P_TRANSPORT_TCP = 1,
};

enum {
    LIBP2P_MUXER_MPLEX = 0,
    LIBP2P_MUXER_YAMUX = 1,
};

enum {
    Direction_In = 0,
    Direction_Out = 1,
};

enum {
    LIBP2P_PK_RSA = 0,
    LIBP2P_PK_ED25519 = 1,
    LIBP2P_PK_SECP256K1 = 2,
    LIBP2P_PK_ECDSA = 3,
};
