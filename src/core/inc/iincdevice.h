/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincdevice.h
/// @brief   Base class for INC transport devices
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCDEVICE_H
#define IINCDEVICE_H

#include <core/io/iiodevice.h>

namespace iShell {

class iEventDispatcher;

/// @brief Base class for INC transport devices
/// @details Provides common interface for TCP and Pipe devices
class IX_CORE_EXPORT iINCDevice : public iIODevice
{
    IX_OBJECT(iINCDevice)
public:
    /// @brief Role of the device
    enum Role {
        ROLE_CLIENT,    ///< Client connection (connect to remote)
        ROLE_SERVER     ///< Server socket (accept connections)
    };

    /// @brief Constructor
    /// @param role Device role (client or server)
    /// @param parent Parent object
    explicit iINCDevice(Role role, iObject *parent = IX_NULLPTR);
    virtual ~iINCDevice();

    /// Get device role
    Role role() const { return m_role; }

    /// Get peer address (for logging/debugging)
    /// @return Peer address string (format depends on transport: "IP:port" for TCP, "path" for Unix socket)
    virtual iString peerAddress() const = 0;

    /// Start async event monitoring (attach EventSource to dispatcher)
    /// @details Must be called AFTER connecting signals to ensure no events are missed.
    ///          This separates device creation from event monitoring activation.
    /// @param dispatcher The EventDispatcher to attach to. Must not be nullptr
    /// @return true on success, false if dispatcher is nullptr or attachment fails
    virtual bool startEventMonitoring(iEventDispatcher* dispatcher) = 0;

    /// Configure event monitoring capabilities
    /// @details Controls which I/O events should be monitored by the EventSource.
    ///          This hides the internal EventSource implementation from external code.
    /// @param read Enable/disable read event monitoring
    /// @param write Enable/disable write event monitoring
    virtual void configEventAbility(bool read, bool write) = 0;

// signals:
    /// @brief New connection signal (server mode)
    /// @param client The newly connected client device
    /// @details Emitted when a new client connection is accepted.
    ///          The client device is a child of this server device.
    void newConnection(iINCDevice* client) ISIGNAL(newConnection, client);

    void connected() ISIGNAL(connected);
    void disconnected() ISIGNAL(disconnected);
    void errorOccurred(int errorCode) ISIGNAL(errorOccurred, errorCode);

private:
    Role m_role;  ///< Device role (client or server)
    
    IX_DISABLE_COPY(iINCDevice)
};

} // namespace iShell

#endif // IINCDEVICE_H
