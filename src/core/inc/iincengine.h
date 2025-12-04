/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincengine.h
/// @brief   Internal engine managing transport creation and thread pool
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCENGINE_H
#define IINCENGINE_H

#include <core/kernel/iobject.h>
#include <core/utils/istring.h>
#include <core/utils/istringview.h>

namespace iShell {

class iINCDevice;
class iTcpDevice;
class iUnixDevice;
class iUDPDevice;
class iEventSource;
class iTcpEventSource;
class iUnixEventSource;

/// @brief Internal engine managing transport creation
/// @details Each iINCContext or iINCServer owns its own iINCEngine instance.
///          NOT a global singleton - created on demand by owner.
///          Provides: transport creation factory
///          EventSource is automatically attached to current thread's EventDispatcher
///          Thread management is handled externally by the application layer
class IX_CORE_EXPORT iINCEngine : public iObject
{
    IX_OBJECT(iINCEngine)
public:
    /// @brief Constructor
    /// @param parent Parent object (typically iINCContext or iINCServer)
    explicit iINCEngine(iObject *parent = IX_NULLPTR);
    virtual ~iINCEngine();

    /// Initialize engine resources
    /// @return true on success, false on error
    bool initialize();

    /// Shutdown engine and cleanup resources
    void shutdown();

    /// Check if engine is initialized and ready
    /// @return true if initialized
    bool isReady() const { return m_initialized; }

    // --- Transport Creation Methods (replacing iINCTransportFactory) ---

    /// Create client transport from URL
    /// @param url Connection URL (e.g., "tcp://127.0.0.1:8080" or "pipe:///tmp/inc.sock")
    /// @return Created transport device, or nullptr on error
    iINCDevice* createClientTransport(const iStringView& url);

    /// Create server transport from URL
    /// @param url Bind URL (e.g., "tcp://0.0.0.0:8080" or "pipe:///tmp/inc.sock")
    /// @return Created transport device, or nullptr on error
    iINCDevice* createServerTransport(const iStringView& url);

private:
    struct ParsedUrl {
        iString scheme;     ///< tcp, pipe, unix
        iString host;       ///< hostname for TCP
        xuint16 port;       ///< port for TCP
        iString path;       ///< path for pipe/unix
        bool    valid;      ///< parsing success
    };

    ParsedUrl parseUrl(const iStringView& url);

    iTcpDevice* createTcpClient(const ParsedUrl& url);
    iTcpDevice* createTcpServer(const ParsedUrl& url);
    iUnixDevice* createUnixClient(const ParsedUrl& url);
    iUnixDevice* createUnixServer(const ParsedUrl& url);
    iUDPDevice* createUdpClient(const ParsedUrl& url);
    iUDPDevice* createUdpServer(const ParsedUrl& url);

    bool                m_initialized;  ///< Initialization state

    IX_DISABLE_COPY(iINCEngine)
};

} // namespace iShell

#endif // IINCENGINE_H
