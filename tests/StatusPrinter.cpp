/** @file */
// Test-only definition of rpc::operator<<(ostream&, Status const&).
//
// Errors.hpp declares this stream operator as a friend but leaves it undefined —
// consuming projects (Clio, xrpld) provide their own out-of-line definition in
// a .cpp (Clio's pulls in project-specific error-info tables). The standalone
// test build has no such .cpp, so gtest's value printer would fail to link when
// an EXPECT_EQ on a Status fails. This minimal version prints just enough to make
// assertion failures readable.

#include <rpcspec/Errors.hpp>

#include <ostream>
#include <variant>

namespace rpc {

std::ostream&
operator<<(std::ostream& stream, Status const& status)
{
    std::visit([&stream](auto code) { stream << "Code: " << static_cast<int>(code); }, status.code);
    if (!status.error.empty())
        stream << ", Error: " << status.error;
    if (!status.message.empty())
        stream << ", Message: " << status.message;
    return stream;
}

}  // namespace rpc
