#include "libp2p_module_plugin.h"

Connection::Connection(Libp2pModuleInterface *owner,
                       libp2p_stream_t *stream)
    : owner_(owner),
      stream_(stream)
{
}

Connection::~Connection()
{
    release();
}

Connection::Connection(Connection &&other) noexcept
    : owner_(other.owner_),
      stream_(other.stream_)
{
    other.owner_ = nullptr;
    other.stream_ = nullptr;
}

Connection &Connection::operator=(Connection &&other) noexcept
{
    if (this != &other) {
        release();

        owner_ = other.owner_;
        stream_ = other.stream_;

        other.owner_ = nullptr;
        other.stream_ = nullptr;
    }

    return *this;
}

libp2p_stream_t *Connection::get() const
{
    return stream_;
}

bool Connection::isValid() const
{
    return owner_ && stream_;
}

void Connection::release()
{
    if (!owner_ || !stream_)
        return;

    auto *owner = owner_;
    auto *stream = stream_;

    owner_ = nullptr;
    stream_ = nullptr;

    // IMPORTANT:
    // Construct wrapper WITHOUT triggering destructor recursion.
    Connection tmp;
    tmp.owner_ = owner;
    tmp.stream_ = stream;

    owner->streamRelease(tmp);

    // prevent tmp destructor from re-releasing
    tmp.owner_ = nullptr;
    tmp.stream_ = nullptr;
}

