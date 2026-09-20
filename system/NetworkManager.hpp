#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "model/SystemStatus.hpp"

/// One network interface as reported by the kernel.
struct NetworkInterface
{
    std::string name;    ///< e.g. "eth0", "wlan0"
    std::string address; ///< IPv4 address when one is assigned
    bool wireless{false};
    bool up{false};
};

/// Reads host networking state. Deliberately does NOT talk to NetworkManager
/// over D-Bus yet — that is an explicit non-goal in README (line 536).
///
/// Implementation notes for NetworkManager.cpp:
///   - listInterfaces(): iterate /sys/class/net, then getifaddrs() for addresses
///   - wireless: true when /sys/class/net/<name>/wireless exists
///   - up: read the operstate file or IFF_UP from getifaddrs flags
///   - primaryAddress(): first UP non-loopback interface with an IPv4 address
///   - setWifiEnabled()/setApMode(): Phase 10 stubs, return false for now
class NetworkManager
{
public:
    NetworkManager() = default;
    ~NetworkManager() = default;

    std::vector<NetworkInterface> listInterfaces() const;
    bool primaryAddress(std::string &outAddress) const;
    bool isConnected() const;
    NetworkState state() const;

    // Phase 10 (README lines 407-436) — declared so the shape is fixed now.
    bool setWifiEnabled(bool enabled);
    bool setApMode(bool enabled);

private:
    NetworkManager(const NetworkManager &) = delete;
    NetworkManager &operator=(const NetworkManager &) = delete;
    NetworkManager(NetworkManager &&) = delete;
    NetworkManager &operator=(NetworkManager &&) = delete;
};
