/** @file */
#pragma once
// Out-of-line definitions for HandlerFor<Input>. Include ONLY from a handler's .cpp (which also
// includes that handler's <rpcspec/handlers/<name>/Spec.hpp>, so `specFor` is visible to ADL), and
// emit the single explicit instantiation:
//
//     template struct rpc::spec::HandlerFor<rpc::spec::handlers::<name>::Input>;
//
// That confines the handler's consteval spec instantiation to its own translation unit.

#include <boost/json/value.hpp>

#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/VersionedSpec.hpp>

#include <cstdint>
#include <expected>
#include <utility>

namespace rpc::spec {

template <typename InputT>
std::expected<InputT, rpc::Status>
HandlerFor<InputT>::parseInput(boost::json::value jv, uint32_t apiVersion)
{
    // ADL: specFor(Input const*) is declared in the Input's namespace.
    return specFor(static_cast<InputT const*>(nullptr)).parse(std::move(jv), apiVersion);
}

template <typename InputT>
RpcSpecView
HandlerFor<InputT>::spec(uint32_t apiVersion)
{
    return specFor(static_cast<InputT const*>(nullptr)).view(apiVersion);
}

}  // namespace rpc::spec
