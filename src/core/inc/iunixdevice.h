/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iunixdevice.h
/// @brief   Unix domain socket transport
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IUNIXDEVICE_H
#define IUNIXDEVICE_H

#include <core/utils/istring.h>

#include "inc/iincdevice.h"

namespace iShell {

class iEventSource;

/// @brief Unix domain socket transport
/// @details Unified for both client and server modes.
class IX_CORE_EXPORT iUnixDevice : public iINCDevice
{
    IX_OBJECT(iUnixDevice)
public:
    /// @brief Create Unix domain socket device
    /// @param role Device role (client or server)
    /// @param parent Parent object
    explicit iUnixDevice(Role role, iObject *parent = IX_NULLPTR);
    virtual ~iUnixDevice();

    // --- Client Mode Methods ---
    
    /// Connect to Unix domain socket (client mode only)
    /// @param path Socket path (e.g., "/tmp/inc.sock")
    /// @return 0 on success, negative on error
    int connectToPath(const iString& path);

    // --- Server Mode Methods ---
    
    /// Start listening on Unix domain socket (server mode only)
    /// @param path Socket path
    /// @return 0 on success, negative on error
    int listenOn(const iString& path);

    /// Accept pending connection (server mode only)
    /// Emits newConnection(iUnixDevice*) signal with the client device
    void acceptConnection();

    // --- Common Methods ---
    
    /// Get peer address (returns socket path)
    iString peerAddress() const override { return m_socketPath; }
    
    /// Get socket path
    iString socketPath() const { return m_socketPath; }

    /// Get socket descriptor (for internal use by EventSource)
    int socketDescriptor() const { return m_sockfd; }

    /// Start EventSource monitoring
    /// Must be called AFTER connecting signals to avoid race conditions
    /// @param dispatcher The EventDispatcher to attach to. Must not be nullptr
    bool startEventMonitoring(iEventDispatcher* dispatcher) override;

    /// Configure event monitoring capabilities
    /// @param read Enable/disable read event monitoring
    /// @param write Enable/disable write event monitoring
    void configEventAbility(bool read, bool write) override;

    /// Set socket to non-blocking mode
    bool setNonBlocking(bool nonBlocking);

    /// Get socket error status
    int getSocketError();

    /// Handle async connection completion (internal use by EventSource)
    void handleConnectionComplete();

    // iIODevice interface
    bool isSequential() const override { return true; }
    xint64 bytesAvailable() const override;
    void close() override;

protected:
    iByteArray readData(xint64 maxlen, xint64* readErr) override;
    xint64 writeData(const iByteArray& data) override;

private:
    bool createSocket();
    void removeSocketFile();

    int                 m_sockfd;
    iString             m_socketPath;
    iEventSource*       m_eventSource;  ///< Internal EventSource (created in connectToPath/listenOn)
    
    IX_DISABLE_COPY(iUnixDevice)
};

} // namespace iShell

#endif // IUNIXDEVICE_H
