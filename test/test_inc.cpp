/*
 * @file test_inc.cpp
 * @brief Demo: 1 Server sending to Multiple Clients (TCP + Shared Memory Broadcast)
 * @details Tests shared memory usage when 1 server sends to multiple clients (3+)
 *          Note: Uses broadcastEvent instead of iINCStream because iINCStream is client-side only.
 */

#include <core/inc/iincserver.h>
#include <core/inc/iincserverconfig.h>
#include <core/inc/iincconnection.h>
#include <core/inc/iinccontext.h>
#include <core/inc/iincstream.h>
#include <core/inc/iincerror.h>
#include <core/inc/iincoperation.h>
#include <core/kernel/icoreapplication.h>
#include <core/kernel/itimer.h>
#include <core/kernel/ideadlinetimer.h>
#include <core/thread/iatomiccounter.h>
#include <core/io/ilog.h>
#include <core/io/imemblock.h>
#include <cstdlib>
#include <vector>
#include <list>
#include <map>
#include <utility>

#define ILOG_TAG "test"

using namespace iShell;

struct PerfOptions {
    PerfOptions()
        : payloadBytes(63 * 1024)
        , inflightPerClient(3)
        , logIntervalMs(10000)
        , opTimeoutMs(50)
        , enableChecksum(true)
    {}
    xint32 payloadBytes;
    xint32 inflightPerClient;
    xint32 logIntervalMs;
    xint32 opTimeoutMs;
    bool enableChecksum;
};


static xint64 calculateChecksum(const char* data, size_t size) {
    xint64 checksum = 0;
    if (size >= 24) {
        // Head 8 bytes
        for (int i = 0; i < 8; ++i) checksum ^= (xint64)(unsigned char)data[i] << (i * 8);
        
        // Middle 8 bytes
        size_t midStart = (size / 2) - 4;
        for (int i = 0; i < 8; ++i) checksum ^= (xint64)(unsigned char)data[midStart + i] << (i * 8);

        // Tail 8 bytes
        size_t tailStart = size - 8;
        for (int i = 0; i < 8; ++i) checksum ^= (xint64)(unsigned char)data[tailStart + i] << (i * 8);
    } else {
        // Fallback for small buffers
        for (size_t i = 0; i < size; ++i) {
             checksum ^= (xint64)(unsigned char)data[i] << ((i % 8) * 8);
        }
    }
    // XOR with size to ensure length changes are detected
    checksum ^= size;
    return checksum;
}

static inline xint64 currentTimeMs() {
    return iDeadlineTimer::current().deadline();
}

// Server: Sends data to all connected clients via sendBinaryData (simulating stream)
class StreamServer : public iINCServer
{
    IX_OBJECT(StreamServer)
public:
    struct ClientInfo {
        ClientInfo(iINCConnection* c = IX_NULLPTR, xuint32 id = 0, xint32 p = 0) : conn(c), channelId(id), pendingOps(p) {}
        iINCConnection* conn;
        xuint32 channelId;
        xint32 pendingOps;  // Track pending operations
    };

    struct SharedPacket {
        iByteArray data;
        xint64 checksum;
        iAtomicCounter<int> pending;
        SharedPacket(const iByteArray& d, xint64 c)
            : data(d)
            , checksum(c)
            , pending(0)
        {
        }
        SharedPacket(const SharedPacket& other)
            : data(other.data)
            , checksum(other.checksum)
            , pending(other.pending.value())
        {
        }
        SharedPacket& operator=(const SharedPacket& other) {
            if (this != &other) {
                data = other.data;
                checksum = other.checksum;
                pending = other.pending.value();
            }
            return *this;
        }
#if __cplusplus >= 201103L
        SharedPacket(SharedPacket&& other) noexcept
            : data(std::move(other.data))
            , checksum(other.checksum)
            , pending(other.pending.value())
        {
        }
        SharedPacket& operator=(SharedPacket&& other) noexcept {
            if (this != &other) {
                data = std::move(other.data);
                checksum = other.checksum;
                pending = other.pending.value();
            }
            return *this;
        }
#endif
    };

    struct CallbackContext {
        CallbackContext(StreamServer* s, iINCConnection* c, xuint32 id, SharedPacket* p) : server(s), conn(c), channelId(id), packet(p) {}
        StreamServer* server;
        iINCConnection* conn;
        xuint32 channelId;
        SharedPacket* packet;
    };

    StreamServer(int numClients, const PerfOptions& options, iObject* parent = IX_NULLPTR)
        : iINCServer(iString("Server"), parent)
        , m_numClients(numClients)
        , m_options(options)
        , m_totalBytesSent(0)
        , m_startTime(0)
        , m_lastLogTime(0)
        , m_closing(false)
        , m_bytesAtLastLog(0)
        , m_inflightPackets(0)
    {
        
        m_startTime = currentTimeMs();
        
        connect(this, &iINCServer::clientConnected, this, &StreamServer::onClientConnected);
        connect(this, &iINCServer::clientDisconnected, this, &StreamServer::onClientDisconnected);
        connect(this, &iINCServer::streamOpened, this, &StreamServer::onStreamOpened);
        connect(this, &iINCServer::streamClosed, this, &StreamServer::onStreamClosed);
    }

    ~StreamServer() {}

    void beginShutdown() {
        m_closing = true;
    }

    void close() {
        m_closing = true;
        
        // Wait for all pending operations to complete (max 5000ms)
        int waitCount = 0;
        while (waitCount < 500) {
            xint32 totalPending = 0;
            for (std::list<ClientInfo>::const_iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
                totalPending += it->pendingOps;
            }
            if (totalPending == 0) {
                break;
            }
            if (waitCount % 50 == 0) {
                 ilog_info("[Server] Waiting for ", totalPending, " pending operations to complete...");
            }
            iThread::msleep(10);
            waitCount++;
        }
        
        iINCServer::close();
    }

    bool start(const iString& url) {
        int result = listenOn(url);
        if (result != 0) {
            ilog_error("[Server] Failed to listen: ", result);
            return false;
        }
        ilog_info("[Server] Listening on ", url.toUtf8().constData());
        return true;
    }

    void printStats() {
        xint64 now = currentTimeMs();
        xint64 diff = now - m_startTime;
        if (diff > 0) {
            double speed = (double)m_totalBytesSent / diff * 1000.0 / 1024.0 / 1024.0;
            ilog_info("[Server] Final Throughput: ", speed, " MB/s (Total: ", m_totalBytesSent, " bytes in ", diff, " ms)");
        }
    }

protected:
    void handleMethod(iINCConnection* conn, xuint32 seqNum, const iString&, xuint16, const iByteArray&) override {
        sendMethodReply(conn, seqNum, INC_OK, iByteArray());
    }

    void handleBinaryData(iINCConnection*, xuint32, xuint32, xint64, const iByteArray&) override {
        // Not used
    }

private:
    volatile bool m_closing;

    void onClientConnected(iINCConnection* connection) {
        ilog_info("[Server] Client connected: ", connection->peerAddress().toUtf8().constData());
    }

    void onClientDisconnected(iINCConnection* connection) {
        ilog_info("[Server] Client disconnected");
        
        // Remove all streams for this connection
        for (std::list<ClientInfo>::iterator it = m_clients.begin(); it != m_clients.end(); ) {
            if (it->conn == connection) {
                it = m_clients.erase(it);
            } else {
                ++it;
            }
        }
    }

    void onStreamOpened(iINCConnection* conn, xuint32 channelId, xuint32 mode) {
        if (mode & iINCChannel::MODE_READ) { // Client wants to read
            ilog_info("[Server] Stream opened on channel ", channelId);
            m_clients.push_back(ClientInfo(conn, channelId, 0));
            
            ilog_info("[Server] Client connected. Total clients: %d/%d", m_clients.size(), m_numClients);

            if (m_clients.size() == m_numClients) {
                ilog_info("[Server] All clients connected. Starting transmission...");
                startSending();
            }
        }
    }

    void onStreamClosed(iINCConnection* conn, xuint32 channelId) {
        ilog_info("[Server] Stream closed on channel ", channelId);
        for (std::list<ClientInfo>::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (it->conn == conn && it->channelId == channelId) {
                m_clients.erase(it);
                break;
            }
        }
    }

    void startSending() {
        ilog_info("[Server] Starting data transmission...");
        tryFillWindow();
    }

    void tryFillWindow() {
        if (m_closing) return;
        while (m_inflightPackets.value() < m_options.inflightPerClient) {
            if (!sendBroadcastPacket()) {
                break;
            }
        }
    }

    bool sendBroadcastPacket() {
        if (m_closing) return false;
        if (m_clients.empty()) return false;
        xint32 chunkSize = m_options.payloadBytes;
        iByteArray data;

        // Try to acquire from SHM pool (retry a few times without fallback allocation)
        iMemBlock* block = IX_NULLPTR;
        for (int i = 0; i < 5 && !block; ++i) {
            block = acquireBuffer(chunkSize);
            if (!block) {
                iThread::msleep(1);
            }
        }
        if (block) {
            if (block->pool()) {
                static bool printed = false;
                if (!printed) {
                    ilog_info("[Server] Pool blockSizeMax: ", block->pool()->blockSizeMax());
                    printed = true;
                }
            }
            // Fill data
            memset(block->data().value(), 'X', chunkSize);
            
            // Wrap in iByteArray (zero-copy)
            // block starts with Ref=0. dp will increment to 1. data will increment to 2.
            // dp destruction decrements to 1. data holds the sole reference.
            iByteArray::DataPointer dp(static_cast<iTypedArrayData<char>*>(block), (char*)block->data().value(), chunkSize);
            data = iByteArray(dp);
        } else {
            ilog_warn("[Server] acquireBuffer returned nullptr after retries, skipping send");
            return false;
        }

        // Calculate checksum for verification
        xint64 checksum = m_options.enableChecksum ? calculateChecksum(data.constData(), data.size()) : 0;

        // Send to all clients with checksum in pos parameter
        SharedPacket* packet = new SharedPacket(data, checksum);
        int successfulSends = 0;
        for (std::list<ClientInfo>::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
            ClientInfo& client = *it;
            iSharedDataPointer<iINCOperation> op = sendBinaryData(client.conn, client.channelId, checksum, data);
            if (op) {
                op->setTimeout(m_options.opTimeoutMs);
                CallbackContext* ctx = new CallbackContext(this, client.conn, client.channelId, packet);
                op->setFinishedCallback(&StreamServer::onPacketSent, ctx);
                client.pendingOps++;
                packet->pending++;
                successfulSends++;
            } else {
                ilog_warn("[Server] sendBinaryData returned nullptr");
            }
        }

        if (packet->pending == 0) {
            delete packet;
            return false;
        }

        m_inflightPackets++;
        
        m_totalBytesSent += static_cast<xint64>(chunkSize) * successfulSends;

        xint64 now = currentTimeMs();
        if (now - m_lastLogTime > m_options.logIntervalMs) {
            xint64 intervalTime = now - m_lastLogTime;
            m_lastLogTime = now;
            xint64 diff = now - m_startTime;
            
            xint64 intervalBytes = m_totalBytesSent - m_bytesAtLastLog;
            m_bytesAtLastLog = m_totalBytesSent;

            if (diff > 0 && intervalTime > 0) {
                double avgSpeed = (double)m_totalBytesSent / diff * 1000.0 / 1024.0 / 1024.0;
                double intervalSpeed = (double)intervalBytes / intervalTime * 1000.0 / 1024.0 / 1024.0;
                ilog_info("[Server] Throughput Interval(", m_options.logIntervalMs / 1000, "s): ", intervalSpeed, " MB/s | Avg: ", avgSpeed, " MB/s (Total: ", m_totalBytesSent, " bytes)");
            }
        }

        return true;
    }

    static void onPacketSent(iINCOperation* op, void* userData) {
        CallbackContext* ctx = static_cast<CallbackContext*>(userData);
        if (!ctx) {
            return;
        }
        
        // Check for errors or timeout
        if (op) {
            iINCOperation::State state = op->getState();
            if (state == iINCOperation::STATE_FAILED) {
                ilog_warn("[Server] Send operation failed, error code: ", op->errorCode());
            } else if (state == iINCOperation::STATE_TIMEOUT) {
                ilog_warn("[Server] Send operation timeout");
            }
            
            // Always release the operation to prevent memory leak and performance degradation
            // (The connection implementation likely keeps track of operations until released)
            if (ctx->conn) {
                ctx->conn->releaseOperation(op);
            }
        }
        
        if (ctx->server) {
            ctx->server->handlePacketSent(ctx->conn->connectionId(), ctx->channelId);
        }

        if (ctx->packet) {
            if (--ctx->packet->pending == 0) {
                if (ctx->server) {
                    ctx->server->onPacketCompleted();
                }
                delete ctx->packet;
            }
        }
        delete ctx;
    }

    void handlePacketSent(xuint64 connId, xuint32 channelId) {
        // Find client
        for (std::list<ClientInfo>::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (it->conn->connectionId() == connId && it->channelId == channelId) {
                it->pendingOps--;
                return;
            }
        }
    }

    void onPacketCompleted() {
        if (m_inflightPackets > 0) {
            m_inflightPackets--;
        }
        tryFillWindow();
    }

    int m_numClients;
    PerfOptions m_options;
    xint64 m_totalBytesSent;
    xint64 m_startTime;
    xint64 m_lastLogTime;
    std::list<ClientInfo> m_clients;
    xint64 m_bytesAtLastLog;
    iAtomicCounter<int> m_inflightPackets;
};

// Client: Receives data from server
class StreamClient : public iINCContext
{
    IX_OBJECT(StreamClient)
public:
    StreamClient(int id, const PerfOptions& options, iObject* parent = IX_NULLPTR)
        : iINCContext(iString("Client"), parent)
        , m_id(id)
        , m_options(options)
        , m_totalBytes(0)
        , m_startTime(0)
        , m_stream(IX_NULLPTR)
        , m_lastLogTime(0)
        , m_bytesAtLastLog(0)
    {
        connect(this, &iINCContext::stateChanged, this, &StreamClient::onStateChanged);
    }

    ~StreamClient() {}

    void printStats() {
        xint64 now = currentTimeMs();
        xint64 diff = now - m_startTime;
        if (diff > 0 && m_startTime > 0) {
            double speed = (double)m_totalBytes / diff * 1000.0 / 1024.0 / 1024.0;
            ilog_info("[Client ", m_id, "] Final Throughput: ", speed, " MB/s (Total: ", m_totalBytes, " bytes in ", diff, " ms)");
        }
    }

private:
    void onStateChanged(iINCContext::State prev, iINCContext::State curr) {
        if (curr == iINCContext::STATE_CONNECTED) {
            ilog_info("[Client ", m_id, "] Connected! Attaching stream...");
            
            m_stream = new iINCStream(iString("ClientStream"), this, this);
            connect(m_stream, &iINCStream::dataReceived, this, &StreamClient::onDataReceived);
            m_stream->attach(iINCChannel::MODE_READ);
            
            m_startTime = currentTimeMs();
        }
    }

    void onDataReceived(xuint32 seqNum, xint64 pos, iByteArray data) {
        if (data.size() > 1024*1024) {
             ilog_warn("[Client ", m_id, "] Received huge data: ", data.size());
        }

        if (m_options.enableChecksum) {
            // Verify checksum (pos contains the checksum)
            xint64 calculatedChecksum = calculateChecksum(data.constData(), data.size());
            if (calculatedChecksum != pos) {
                ilog_error("[Client ", m_id, "] Checksum mismatch! Expected: ", pos, ", Calculated: ", calculatedChecksum, ", Size: ", data.size());
            }
        }

        m_totalBytes += data.size();

        // ACK the received data to free up SHM slots on server
        if (m_stream) {
            m_stream->ackDataReceived(seqNum, data.size());
        }
        
        xint64 now = currentTimeMs();
        if (m_lastLogTime == 0) m_lastLogTime = now; // Init on first data

        if (now - m_lastLogTime > m_options.logIntervalMs) {
            xint64 intervalTime = now - m_lastLogTime;
            m_lastLogTime = now;
            xint64 diff = now - m_startTime;
            
            xint64 intervalBytes = m_totalBytes - m_bytesAtLastLog;
            m_bytesAtLastLog = m_totalBytes;

            if (diff > 0 && intervalTime > 0) {
                double avgSpeed = (double)m_totalBytes / diff * 1000.0 / 1024.0 / 1024.0;
                double intervalSpeed = (double)intervalBytes / intervalTime * 1000.0 / 1024.0 / 1024.0;
                ilog_info("[Client ", m_id, "] Throughput Interval(", m_options.logIntervalMs / 1000, "s): ", intervalSpeed, " MB/s | Avg: ", avgSpeed, " MB/s");
            }
        }
    }

    int m_id;
    PerfOptions m_options;
    xint64 m_totalBytes;
    xint64 m_startTime;
    iINCStream* m_stream;
    xint64 m_lastLogTime;
    xint64 m_bytesAtLastLog;
};

class IncTestController : public iObject
{
    IX_OBJECT(IncTestController)
public:
    IncTestController(StreamServer* server, std::vector<StreamClient*> clients, void (*callback)(), iObject* parent = IX_NULLPTR)
        : iObject(parent)
        , m_server(server)
        , m_clients(clients)
        , m_callback(callback)
        , m_finished(false)
    {
    }

    void finish() {
        if (m_finished) {
            return;
        }
        m_finished = true;

        ilog_info("====================================================");
        ilog_info("Application terminated. Final Statistics:");
        ilog_info("====================================================");

        if (m_server) {
            m_server->beginShutdown();
            m_server->printStats();
            m_server = IX_NULLPTR;
        }

        for (size_t i = 0; i < m_clients.size(); ++i) {
            StreamClient* client = m_clients[i];
            if (!client) {
                continue;
            }
            client->printStats();
            client->deleteLater();
        }
        m_clients.clear();

        ilog_info("====================================================");

        if (m_callback) {
            m_callback();
        }
    }

    void onTimeout() {
        finish();
    }

private:
    StreamServer* m_server;
    std::vector<StreamClient*> m_clients;
    void (*m_callback)();
    bool m_finished;
};

static void printIncUsage() {
    ilog_info("Usage: imediaplayertest --inc [options]\n"
              "  -t <sec>        : timeout (seconds), 0 = no timeout\n"
              "  -u <url>        : server url (default: unix:///tmp/imediaplayer_inc.sock)\n"
              "  -n <num>        : number of clients\n"
              "  -s <mb>         : shared memory size (MB)\n"
              "  -p <kb>         : payload size per packet (KB, default 63)\n"
              "  -i <num>        : inflight packets per client (default 3)\n"
              "  -l <sec>        : log interval (seconds, default 10)\n"
              "  -o <ms>         : send operation timeout (ms, default 50)\n"
              "  --no-checksum   : disable checksum verification\n"
              "  --server        : server-only\n"
              "  --client        : client-only\n"
              "  -h, --help      : show help");
}

int test_inc_pref(void (*callback)())
{

    std::list<iShell::iString> argsList = iShell::iCoreApplication::arguments();
    std::vector<iShell::iString> args(argsList.begin(), argsList.end());

    bool enableInc = false;

    // Parse arguments
    int timeoutSec = 5;
    int numClients = 3;
    int shmSizeMB = 32; // Default 32MB
    #ifdef __ANDROID__
    iString url = "unix:///data/local/tmp/imediaplayer_inc.sock";
    #else
    iString url = "unix:///tmp/imediaplayer_inc.sock";
    #endif
    bool isServer = false;
    bool isClient = false;
    PerfOptions options;

    for (size_t i = 1; i < args.size(); ++i) {
        iString arg = args[i];
        if (arg == "--inc") {
            enableInc = true;
        } else if (arg == "-t" && i + 1 < args.size()) {
            timeoutSec = atoi(args[++i].toUtf8().constData());
        } else if (arg == "-u" && i + 1 < args.size()) {
            url = args[++i];
        } else if (arg == "-n" && i + 1 < args.size()) {
            numClients = atoi(args[++i].toUtf8().constData());
        } else if (arg == "-s" && i + 1 < args.size()) {
            shmSizeMB = atoi(args[++i].toUtf8().constData());
        } else if (arg == "-p" && i + 1 < args.size()) {
            options.payloadBytes = atoi(args[++i].toUtf8().constData()) * 1024;
        } else if (arg == "-i" && i + 1 < args.size()) {
            options.inflightPerClient = atoi(args[++i].toUtf8().constData());
        } else if (arg == "-l" && i + 1 < args.size()) {
            options.logIntervalMs = atoi(args[++i].toUtf8().constData()) * 1000;
        } else if (arg == "-o" && i + 1 < args.size()) {
            options.opTimeoutMs = atoi(args[++i].toUtf8().constData());
        } else if (arg == "--no-checksum") {
            options.enableChecksum = false;
        } else if (arg == "--server") {
            isServer = true;
        } else if (arg == "--client") {
            isClient = true;
        } else if (arg == "-h" || arg == "--help") {
            printIncUsage();
            return -1;
        }
    }

    if (!enableInc) {
        return -1;
    }

    // Default to both if neither specified (backward compatibility)
    if (!isServer && !isClient) {
        isServer = true;
        isClient = true;
    }

    if (numClients <= 0) numClients = 1;
    if (shmSizeMB <= 0) shmSizeMB = 32;
    if (options.payloadBytes <= 0) options.payloadBytes = 63 * 1024;
    if (options.inflightPerClient <= 0) options.inflightPerClient = 1;
    if (options.logIntervalMs <= 0) options.logIntervalMs = 10000;
    if (options.opTimeoutMs <= 0) options.opTimeoutMs = 50;

    ilog_info("====================================================");
    ilog_info("Demo: Stream Shared Memory Test");
    ilog_info("====================================================");
    ilog_info("Configuration:");
    ilog_info("  Mode: ", (isServer && isClient) ? "Combined" : (isServer ? "Server Only" : "Client Only"));
    ilog_info("  URL: ", url.toUtf8().constData());
    ilog_info("  Timeout: ", timeoutSec, " seconds");
    if (isServer) ilog_info("  Num Clients (Expected): ", numClients);
    ilog_info("  SHM Size: ", shmSizeMB, " MB");
    ilog_info("  Payload: ", options.payloadBytes / 1024, " KB");
    ilog_info("  Inflight/Client: ", options.inflightPerClient);
    ilog_info("  Log Interval: ", options.logIntervalMs / 1000, " seconds");
    ilog_info("  Op Timeout: ", options.opTimeoutMs, " ms");
    ilog_info("  Checksum: ", options.enableChecksum ? "ON" : "OFF");
    ilog_info("====================================================");

    iCoreApplication* app = iCoreApplication::instance();
    StreamServer* server = IX_NULLPTR;
    std::vector<StreamClient*> clients;

    if (isServer) {
        // Create server
        server = new StreamServer(numClients, options, app);
        
        // Configure shared memory
        iINCServerConfig config;
        config.setSharedMemorySize(shmSizeMB * 1024 * 1024);
        server->setConfig(config);

        if (!server->start(url)) {
            ilog_error("Failed to start server!");
            delete server;
            return -1;
        }
    }

    if (isClient) {
        // Create multiple clients
        // If running in client-only mode, numClients determines how many client instances to spawn in this process
        for (int i = 0; i < numClients; ++i) {
            StreamClient* client = new StreamClient(i + 1, options, app);
            if (client->connectTo(url) < 0) {
                ilog_error("Client ", i+1, " failed to connect");
                delete client;
            } else {
                clients.push_back(client);
            }
        }
    }

    IncTestController* controller = new IncTestController(server, std::move(clients), callback, app);

    if (timeoutSec > 0) {
        iTimer* quitTimer = new iTimer(controller);
        quitTimer->setSingleShot(true);
        quitTimer->start(timeoutSec * 1000);
        iObject::connect(quitTimer, &iTimer::timeout, controller, &IncTestController::onTimeout);
    }

    return 0;
}
