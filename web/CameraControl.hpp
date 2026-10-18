#pragma once

#include <string>
#include <vector>

#include "model/Profile.hpp"
#include "model/SystemStatus.hpp"

/// Everything the control plane's HTTP API (web layer) is allowed to do to the
/// server. This is the seam between web/ and server/: ApiController is the
/// customer on one side; Application is the concrete adapter behind it. Tests
/// cross the same seam with their fake (tests/web/test_apicontroller.cpp).
///
/// Design notes (docs/ARCHITECTURE-SERVER-CLIENT.md sections 3 and 6):
///   - Capture is always on in the profile design; there is deliberately no
///     camera start/stop or global resolution endpoint. Profiles switch by
///     valve (setProfileEnabled), not by pipeline state, because GStreamer
///     pipeline state is global (section 3.1).
///   - profiles() returns the catalog WITH live state, in a caller-agnostic
///     order. setProfileEnabled() records the desired state and is idempotent:
///     true whether or not anything actually changed (only unknown names
///     return false). Callers must treat false as data, not as an HTTP error.
///   - Threading: from Phase 3 on, implementations must be callable from the
///     web thread, synchronizing whatever they share with the main thread
///     internally (Application: m_stateMutex, see server/Application.hpp).
class CameraControl
{
public:
    virtual ~CameraControl() = default;

    /// Host + supervisor health snapshot.
    virtual SystemStatus systemStatus() const = 0;

    /// The live catalog: every profile with its current state, url, and stats.
    virtual std::vector<ProfileInfo> profiles() const = 0;

    /// Open the profile's valve when enabled == true, close it otherwise.
    /// Idempotent; returns false only for an unknown profile (no change).
    virtual bool setProfileEnabled(const std::string &name, bool enabled) = 0;
};
