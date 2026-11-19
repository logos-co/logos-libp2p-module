#include <logos_test.h>
#include <plugin.h>

#include <string>

using StartAdvertising = StdLogosResult (Libp2pModuleImpl::*)(const std::string&, const std::string&);
using Lookup = StdLogosResult (Libp2pModuleImpl::*)(const std::string&);

// Compile-time check: overload resolution fails here if a signature goes away.
[[maybe_unused]] constexpr StartAdvertising kStart = &Libp2pModuleImpl::discoStartAdvertising;
[[maybe_unused]] constexpr Lookup kLookup = &Libp2pModuleImpl::discoLookup;

LOGOS_TEST(disco_spec_signatures_exist) {
    LOGOS_ASSERT_TRUE(true);
}
