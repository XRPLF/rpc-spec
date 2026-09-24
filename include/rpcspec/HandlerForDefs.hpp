/** @file */
#pragma once
// Out-of-line definitions for HandlerFor. Included ONLY from the generated per-handler
// instantiation TU, which also includes that handler's <rpcspec/handlers/<name>/Spec.hpp>
// (so `specFor` is visible to ADL) and the consumer's backend header, then emits
//
//     template struct rpc::spec::HandlerFor<...::Input, <the consumer's JSON value type>>;
//
// That confines each handler's consteval spec instantiation to a translation unit of its
// own, so the TUs that merely dispatch (a handler registry, a request processor) see the
// declarations in HandlerFor.hpp and nothing more.
//
// Consumers do not write those TUs: cmake/RpcSpecInstantiations.cmake generates one per
// handler from cmake/Instantiate.cpp.in. See rpcspec_generate_instantiations().

#include <rpcspec/Concepts.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/RpcSpecView.hpp>
#include <rpcspec/VersionedSpec.hpp>

#include <cstdint>
#include <expected>
#include <utility>

namespace rpc::spec {

template <typename InputT, typename ValueT>
    requires HasObjectView<ValueT>
std::expected<InputT, rpc::Status>
HandlerFor<InputT, ValueT>::parseInput(ValueT jv, uint32_t apiVersion)
{
    // ADL: specFor(Input const*) is declared in the Input's namespace.
    return specFor(static_cast<InputT const*>(nullptr)).parse(std::move(jv), apiVersion);
}

template <typename InputT, typename ValueT>
    requires HasObjectView<ValueT>
RpcSpecView<typename HandlerFor<InputT, ValueT>::View>
HandlerFor<InputT, ValueT>::spec(uint32_t apiVersion)
{
    return specFor(static_cast<InputT const*>(nullptr)).template view<View>(apiVersion);
}

}  // namespace rpc::spec
