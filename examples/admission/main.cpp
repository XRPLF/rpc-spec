// Runs protobuf frames from three peers through a gossip node's receive path, and JSON requests
// from three clients through its RPC endpoint, and prints what became of each. See README.md for a
// walk through it.

#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/Rule.hpp>

#include <GossipHost.hpp>
#include <GossipSpecs.hpp>
#include <Receipt.hpp>
#include <RpcHost.hpp>
#include <gossip.pb.h>

#include <cstdint>
#include <format>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

[[nodiscard]] std::string_view
nameOf(admission::spec::Rule rule)
{
    switch (rule)
    {
        case admission::spec::Rule::None:
            return "-";
        case admission::spec::Rule::Sender:
            return "Sender";
        case admission::spec::Rule::UnknownType:
            return "UnknownType";
        case admission::spec::Rule::Privileged:
            return "Privileged";
        case admission::spec::Rule::Feature:
            return "Feature";
        case admission::spec::Rule::Compression:
            return "Compression";
        case admission::spec::Rule::Oversized:
            return "Oversized";
        case admission::spec::Rule::RepeatedCap:
            return "RepeatedCap";
        case admission::spec::Rule::EntrySize:
            return "EntrySize";
        case admission::spec::Rule::UnknownFields:
            return "UnknownFields";
        case admission::spec::Rule::Group:
            return "Group";
        case admission::spec::Rule::Depth:
            return "Depth";
        case admission::spec::Rule::Malformed:
            return "Malformed";
        case admission::spec::Rule::Check:
            return "Check";
    }
    return "?";
}

[[nodiscard]] std::string_view
nameOf(admission::spec::PenaltyClass penalty)
{
    switch (penalty)
    {
        case admission::spec::PenaltyClass::None:
            return "-";
        case admission::spec::PenaltyClass::Trivial:
            return "Trivial";
        case admission::spec::PenaltyClass::Useless:
            return "Useless";
        case admission::spec::PenaltyClass::Invalid:
            return "Invalid";
        case admission::spec::PenaltyClass::Malformed:
            return "Malformed";
    }
    return "?";
}

[[nodiscard]] std::string_view
nameOf(example::Receipt::Outcome outcome)
{
    switch (outcome)
    {
        case example::Receipt::Outcome::Handled:
            return "handled";
        case example::Receipt::Outcome::Refused:
            return "refused";
        case example::Receipt::Outcome::Closed:
            return "CLOSED";
    }
    return "?";
}

// Print a receipt, with the sender's balances after it.
void
print(std::string_view what, example::Receipt const& receipt, double reputation, double tokens)
{
    std::cout << std::format(
        "  {:<44} {:<8} {:<6} {:<13} {:<9} {:>7.0f} {:>6.1f}  {}\n",
        what,
        nameOf(receipt.outcome),
        receipt.stage,
        nameOf(receipt.rule),
        nameOf(receipt.penalty),
        reputation,
        tokens,
        receipt.reason);
}

// Receive `bytes` in 16-byte pieces, as a small receive buffer would deliver them.
void
send(example::GossipHost& host, std::string_view what, std::vector<uint8_t> const& bytes)
{
    auto const receipt = host.receive(bytes, 16);
    print(what, receipt, host.reputation(), host.tokens());
}

void
request(example::RpcHost& host, std::string_view path, std::string_view body)
{
    auto const receipt = host.handle(path, body);
    print(
        std::format("{} {}", path, body.substr(0, 30)), receipt, host.reputation(), host.tokens());
}

// A submit body listing `count` blobs of `hexBytes` characters each.
[[nodiscard]] std::string
submit(int32_t count, size_t hexBytes)
{
    auto body = std::string{R"({"tx_blobs":[)"};
    for (int32_t i{}; i < count; ++i)
    {
        body += std::format(R"({}"{}")", i == 0 ? "" : ",", std::string(hexBytes, 'A'));
    }
    return body + "]}";
}

void
heading(std::string_view peer)
{
    std::cout << std::format(
        "\n{}\n  {:<44} {:<8} {:<6} {:<13} {:<9} {:>7} {:>6}  {}\n",
        peer,
        "frame",
        "outcome",
        "stage",
        "rule",
        "penalty",
        "balance",
        "tokens",
        "reason");
}

[[nodiscard]] std::string
hash(uint8_t fill, size_t bytes = example::kHashBytes)
{
    return std::string(bytes, static_cast<char>(fill));
}

[[nodiscard]] gossip::Inventory
inventory(int32_t hashes, size_t hashBytes = example::kHashBytes)
{
    auto message = gossip::Inventory{};
    for (int32_t i{}; i < hashes; ++i)
    {
        message.add_hashes(hash(static_cast<uint8_t>(i), hashBytes));
    }
    return message;
}

[[nodiscard]] gossip::Fetch
fetch(uint32_t limit, size_t hashBytes = example::kHashBytes)
{
    auto message = gossip::Fetch{};
    message.set_kind(gossip::fkBODY);
    message.set_hash(hash(0xab, hashBytes));
    message.set_limit(limit);
    return message;
}

}  // namespace

int
main()
{
    auto ping = gossip::Ping{};
    ping.set_seq(1);

    // An ordinary peer: not trusted, and it did not negotiate fetching.
    {
        heading("Peer A: untrusted, no fetch feature");
        auto host = example::GossipHost{example::PeerConnection{}, 100.0, 1.0};

        send(host, "ping", example::frame(example::MessageType::Ping, ping));
        send(
            host,
            "inventory of 3 hashes",
            example::frame(example::MessageType::Inventory, inventory(3)));

        // Inventory is relayed as received by older releases, so each refusal below costs a
        // trivial penalty only: the sender may be an honest relay.
        send(
            host,
            "inventory with a 4-byte hash",
            example::frame(example::MessageType::Inventory, inventory(1, 4)));
        send(
            host,
            "inventory of 300 hashes (cap 256)",
            example::frame(example::MessageType::Inventory, inventory(300)));
        send(
            host,
            "inventory of 600 hashes (over 16 KiB)",
            example::frame(example::MessageType::Inventory, inventory(600)));

        auto report = gossip::AdminReport{};
        report.add_loads("cpu 0.4");
        send(host, "admin report", example::frame(example::MessageType::AdminReport, report));
        send(host, "fetch", example::frame(example::MessageType::Fetch, fetch(10)));

        // 17 fields the schema does not define, one over the limit for one message.
        auto unknown = ping.SerializeAsString();
        for (int32_t field{100}; field < 117; ++field)
        {
            unknown += static_cast<char>((field << 3) & 0x7f | 0x80);
            unknown += static_cast<char>(field >> 4);
            unknown += '\x01';
        }
        send(
            host,
            "ping with 17 unknown fields",
            example::frame(
                static_cast<uint16_t>(example::MessageType::Ping),
                std::span<uint8_t const>{
                    reinterpret_cast<uint8_t const*>(unknown.data()), unknown.size()}));

        send(host, "type 9, which has no spec", example::frame(9, std::span<uint8_t const>{}));

        // Bytes that do not decode: the connection is closed.
        auto const garbage = std::vector<uint8_t>{0x08, 0x96};
        send(
            host,
            "ping that does not decode",
            example::frame(static_cast<uint16_t>(example::MessageType::Ping), garbage));
        send(host, "ping after the close", example::frame(example::MessageType::Ping, ping));
    }

    // A trusted peer that negotiated fetching.
    {
        heading("Peer B: trusted, fetch feature");
        auto host = example::GossipHost{
            example::PeerConnection{.trusted = true, .features = {example::kFetchFeature}},
            100.0,
            1.0};

        auto report = gossip::AdminReport{};
        report.add_loads("cpu 0.4");
        send(host, "admin report", example::frame(example::MessageType::AdminReport, report));
        // The 32-byte hash spans two 16-byte pieces, so the check sees its size but not its bytes,
        // which is all it needs.
        send(host, "fetch, limit 10", example::frame(example::MessageType::Fetch, fetch(10)));
        send(
            host,
            "fetch, limit 500 (max 100)",
            example::frame(example::MessageType::Fetch, fetch(500)));
        send(
            host,
            "fetch of a 20-byte hash",
            example::frame(example::MessageType::Fetch, fetch(10, 20)));
    }

    // A peer that sends faster than its bucket refills.
    {
        heading("Peer C: a burst of 5 tokens, refilling at 1 a second");
        auto host = example::GossipHost{example::PeerConnection{}, 5.0, 1.0};
        for (int32_t i{1}; i <= 7; ++i)
        {
            send(host, std::format("ping {}", i), example::frame(example::MessageType::Ping, ping));
        }
    }
    std::cout << "\n--- The same stages, for JSON RPC requests ---\n";

    // An ordinary client.
    {
        heading("Client D: no admin role");
        auto host = example::RpcHost{example::RpcClient{}, 100.0, 1.0};

        request(host, "/v1/server_info", "{}");
        request(host, "/v1/submit", submit(2, 200));
        request(host, "/v1/submit", submit(9, 200));    // cap 8
        request(host, "/v1/submit", submit(1, 10));     // under 64 characters
        request(host, "/v1/submit", submit(400, 200));  // over 64 KiB
        request(host, "/v1/history", R"({"account":"r1","limit":50})");
        request(host, "/v1/history", R"({"account":"r1","limit":5000})");  // max 200
        request(host, "/v1/history", R"({"a":[[[[[[[[[1]]]]]]]]]})");      // deeper than 8
        request(host, "/v1/history", R"({"account":)");                    // cut off
        request(host, "/v1/stop", "{}");
        request(host, "/v1/teleport", "{}");
    }

    // An admin.
    {
        heading("Client E: admin role");
        auto host = example::RpcHost{example::RpcClient{.admin = true}, 100.0, 1.0};
        request(host, "/v1/stop", "{}");
    }

    // A client that asks faster than its bucket refills.
    {
        heading("Client F: a burst of 5 tokens, refilling at 1 a second");
        auto host = example::RpcHost{example::RpcClient{}, 5.0, 1.0};
        for (int32_t i{1}; i <= 4; ++i)
        {
            request(host, "/v1/history", R"({"account":"r1","limit":10})");
        }
    }
    return 0;
}
