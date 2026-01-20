/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iudpclientdevice.h
/// @brief   UDP client virtual device - represents a single UDP client connection
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IUDPCLIENTDEVICE_H
#define IUDPCLIENTDEVICE_H

#include <netinet/in.h>
#include "iincdevice.h"

namespace iShell {

class iUDPDevice;

/// @brief Virtual device representing a single UDP client connection
/// @details Lightweight wrapper that references parent UDP server device and a specific channel.
///          Does not own the socket - all I/O is delegated to parent device.
///          Lifetime managed by iINCConnection.
class IX_CORE_EXPORT iUDPClientDevice : public iINCDevice
{
    IX_OBJECT(iUDPClientDevice)
public:
    /// @brief Constructor for creating empty client device (address will be set later)
    /// @param serverDevice Parent UDP server device (not owned, just referenced)
    /// @param parent iObject parent (typically iINCConnection)
    explicit iUDPClientDevice(iUDPDevice* serverDevice, iObject* parent = IX_NULLPTR);
    
    /// @brief Constructor with full client information
    /// @param serverDevice Parent UDP server device (not owned, just referenced)
    /// @param clientAddr Client address (sockaddr_in)
    /// @param parent iObject parent (typically iINCConnection)
    iUDPClientDevice(iUDPDevice* serverDevice, const struct sockaddr_in& clientAddr, iObject* parent = IX_NULLPTR);

    virtual ~iUDPClientDevice();

    // --- iINCDevice interface ---
    iString peerAddress() const IX_OVERRIDE;
    bool isLocal() const IX_OVERRIDE;
    xint64 bytesAvailable() const IX_OVERRIDE;
    void close() IX_OVERRIDE;
    
    bool startEventMonitoring(iEventDispatcher* dispatcher = IX_NULLPTR) IX_OVERRIDE;
    void configEventAbility(bool read, bool write) IX_OVERRIDE;
    int  eventAbility() const { return m_monitorEvents; }

    int socketDescriptor() const { return -1; }  // Virtual device has no real socket
    
    /// Get client address key (ip in high 32 bits, port in low 32 bits)
    xuint64 addrKey() const { return m_addrKey; }
    
    /// Update client address and channel info (called after first packet received)
    void updateClientInfo(const struct sockaddr_in& clientAddr);
    struct sockaddr_in clientAddr() const { return m_clientAddr; }

    virtual xint64 writeMessage(const iINCMessage& msg, xint64 offset) IX_OVERRIDE;
    void receivedData(const iByteArray& data);

protected:
    iByteArray readData(xint64 maxlen, xint64* readErr) IX_OVERRIDE;
    xint64 writeData(const iByteArray& data) IX_OVERRIDE;

private:
    iUDPDevice*         m_serverDevice;  ///< Reference to parent UDP server device (not owned)
    struct sockaddr_in  m_clientAddr;    ///< Client address
    xuint64             m_addrKey;       ///< Address key (ip:port packed into uint64)
    int                 m_monitorEvents;
    iByteArray          m_recvBuffer;

    IX_DISABLE_COPY(iUDPClientDevice)
};

} // namespace iShell

#endif // IUDPCLIENTDEVICE_H
