/** @file */
#pragma once
// The JSON backend these tests run against.
//
// The spec library names no JSON type, so a consumer picks one and aliases it once. This
// is the test suite's copy of what Clio and xrpld each do in their own tree.

#include <rpcspec/backends/BoostJson.hpp>

namespace rpc::spec {

/**
 * @brief The field view the tests use.
 */
using FieldView = BoostJsonFieldView;

/**
 * @brief The object view the tests use.
 */
using ObjectView = BoostJsonObjectView;

}  // namespace rpc::spec
