/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iincengine.cpp
/// @brief   Internal engine implementation with transport creation
/// @version 1.0
/////////////////////////////////////////////////////////////////

#include <core/inc/iincerror.h>
#include <core/io/iurl.h>
#include <core/io/ilog.h>

#include "inc/iincengine.h"
#include "inc/itcpdevice.h"
#include "inc/iunixdevice.h"
#include "inc/iudpdevice.h"

#define ILOG_TAG "ix_inc"

namespace iShell {

iINCEngine::iINCEngine(iObject *parent)
    : iObject(parent)
    , m_initialized(false)
{
}

iINCEngine::~iINCEngine()
{
    shutdown();
}

bool iINCEngine::initialize()
{
    if (m_initialized) {
        return true;
    }

    // Engine is ready - no special initialization needed
    // Transport EventSources will automatically attach to current thread's EventDispatcher
    m_initialized = true;
    return true;
}

void iINCEngine::shutdown()
{
    if (!m_initialized) {
        return;
    }

    // Engine shutdown - no cleanup needed
    // Transport devices and their EventSources are managed by their owners
    m_initialized = false;
}

iINCDevice* iINCEngine::createClientTransport(const iStringView& url)
{
    ParsedUrl parsed = parseUrl(url);
    if (!parsed.valid) {
        ilog_error("Invalid URL:", url);
        return IX_NULLPTR;
    }

    if (parsed.scheme == "tcp") {
        return createTcpClient(parsed);
    }
    else if (parsed.scheme == "udp") {
        return createUdpClient(parsed);
    }
    else if (parsed.scheme == "pipe" || parsed.scheme == "unix") {
        return createUnixClient(parsed);
    }

    ilog_error("Unsupported scheme:", parsed.scheme);
    return IX_NULLPTR;
}

iINCDevice* iINCEngine::createServerTransport(const iStringView& url)
{
    ParsedUrl parsed = parseUrl(url);
    if (!parsed.valid) {
        ilog_error("Invalid URL:", url);
        return IX_NULLPTR;
    }

    if (parsed.scheme == "tcp") {
        return createTcpServer(parsed);
    }
    else if (parsed.scheme == "udp") {
        return createUdpServer(parsed);
    }
    else if (parsed.scheme == "pipe" || parsed.scheme == "unix") {
        return createUnixServer(parsed);
    }

    ilog_error("Unsupported scheme:", parsed.scheme);
    return IX_NULLPTR;
}

iINCEngine::ParsedUrl iINCEngine::parseUrl(const iStringView& url)
{
    ParsedUrl result;
    result.valid = false;
    result.port = 0;

    // Parse using iUrl (convert iStringView to iString)
    iUrl parsedUrl(url.toString());
    result.scheme = parsedUrl.scheme().toLower();

    if (result.scheme.isEmpty()) {
        ilog_error("Missing scheme in URL:", url);
        return result;
    }

    if (result.scheme == "tcp" || result.scheme == "udp") {
        // tcp://host:port or udp://host:port
        result.host = parsedUrl.host();
        int p = parsedUrl.port();

        if (result.host.isEmpty()) {
            result.host = "127.0.0.1";  // Default to localhost
        }

        if (p <= 0) {
            ilog_error("Missing port in TCP/UDP URL:", url);
            return result;
        }
        result.port = (xuint16)p;

        result.valid = true;
    }
    else if (result.scheme == "pipe" || result.scheme == "unix") {
        // pipe:///path/to/socket or unix:///path/to/socket
        result.path = parsedUrl.path();

        if (result.path.isEmpty()) {
            ilog_error("Missing path in pipe/unix URL:", url);
            return result;
        }

        result.valid = true;
    }

    return result;
}

iTcpDevice* iINCEngine::createTcpClient(const ParsedUrl& url)
{
    iTcpDevice* device = new iTcpDevice(iINCDevice::ROLE_CLIENT);

    if (device->connectToHost(url.host, url.port) != INC_OK) {
        delete device;
        return IX_NULLPTR;
    }

    ilog_info("Created TCP client to", url.host, ":", url.port);
    return device;
}

iTcpDevice* iINCEngine::createTcpServer(const ParsedUrl& url)
{
    iTcpDevice* device = new iTcpDevice(iINCDevice::ROLE_SERVER);

    iString bindAddr = url.host.isEmpty() ? "0.0.0.0" : url.host;
    if (device->listenOn(bindAddr, url.port) != INC_OK) {
        delete device;
        return IX_NULLPTR;
    }

    ilog_info("Created TCP server on", bindAddr, ":", url.port);
    return device;
}

iUnixDevice* iINCEngine::createUnixClient(const ParsedUrl& url)
{
    iUnixDevice* device = new iUnixDevice(iINCDevice::ROLE_CLIENT);

    if (device->connectToPath(url.path) != INC_OK) {
        delete device;
        return IX_NULLPTR;
    }

    ilog_info("Created unix socket client to", url.path);
    return device;
}

iUnixDevice* iINCEngine::createUnixServer(const ParsedUrl& url)
{
    iUnixDevice* device = new iUnixDevice(iINCDevice::ROLE_SERVER);

    if (device->listenOn(url.path) != INC_OK) {
        delete device;
        return IX_NULLPTR;
    }

    ilog_info("Created unix socket server on", url.path);
    return device;
}

iUDPDevice* iINCEngine::createUdpClient(const ParsedUrl& url)
{
    iUDPDevice* device = new iUDPDevice(iINCDevice::ROLE_CLIENT);

    if (device->connectToHost(url.host, url.port) != INC_OK) {
        delete device;
        return IX_NULLPTR;
    }

    ilog_info("Created UDP client to", url.host, ":", url.port);
    return device;
}

iUDPDevice* iINCEngine::createUdpServer(const ParsedUrl& url)
{
    iUDPDevice* device = new iUDPDevice(iINCDevice::ROLE_SERVER);

    iString bindAddr = url.host.isEmpty() ? "0.0.0.0" : url.host;
    if (device->bindOn(bindAddr, url.port) != INC_OK) {
        delete device;
        return IX_NULLPTR;
    }

    ilog_info("Created UDP server on", bindAddr, ":", url.port);
    return device;
}

} // namespace iShell
