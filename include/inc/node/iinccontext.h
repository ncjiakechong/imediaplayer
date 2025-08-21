/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iinccontext.h
/// @brief   context of INC(Inter Node Communication)
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IINCCONTEXT_H
#define IINCCONTEXT_H

#include <unordered_set>

#include <inc/kernel/iincengine.h>

namespace iShell {

class iINCStream;
class iINCOperation;

/** \section Context
 *
 * A context is the basic object for a connection to a server.
 * It multiplexes commands, data streams and events through a single
 * channel.
 *
 * There is no need for more than one context per application, unless
 * connections to multiple servers are needed.
 *
 * \subsection Operations
 *
 * All operations on the context are performed asynchronously. I.e. the
 * client will not wait for the server to complete the request. To keep
 * track of all these in-flight operations, the application is given a
 * iINCOperation object for each asynchronous operation.
 *
 * There are only two actions (besides reference counting) that can be
 * performed on a iINCOperation: querying its state with
 * getState() and aborting it with cancel().
 *
 * A iINCOperation object is reference counted, so an application must
 * make sure to unreference it, even if it has no intention of using it.
 *
 * \subsection Connecting
 *
 * A context must be connected to a server before any operation can be
 * issued. Calling connect() will initiate the connection
 * procedure. Unlike most asynchronous operations, connecting does not
 * result in a iINCOperation object. Instead, the application should
 * bind a signal using stateChanged().
 *
 * \subsection Disconnecting
 *
 * When the sound support is no longer needed, the connection needs to be
 * closed using disconnect(). This is an immediate function that
 * works synchronously.
 *
 * Since the context object has references to other objects it must be
 * disconnected after use or there is a high risk of memory leaks. If the
 * connection has terminated by itself, then there is no need to explicitly
 * disconnect the context using disconnect().
 */
class IX_INC_EXPORT iINCContext : public iObject
{
    IX_OBJECT(iINCContext)
public:
    /** The state of a connection context */
    enum State {
        STATE_UNCONNECTED,    /**< The context hasn't been connected yet */
        STATE_CONNECTING,     /**< A connection is being established */
        STATE_AUTHORIZING,    /**< The client is authorizing itself to the daemon */
        STATE_SETTING_NAME,   /**< The client is passing its application name to the daemon */
        STATE_READY,          /**< The connection is established, the context is ready to execute operations */
        STATE_FAILED,         /**< The connection failed or was disconnected */
        STATE_TERMINATED      /**< The connection was terminated cleanly */
    };

    iINCContext(const iStringView& name, iINCEngine *engine, iObject *parent = IX_NULLPTR);

    /** Return the current context status */
    State getState() const;

    /** Connect the context to the specified server. If server is NULL,
     * connect to the default server. This routine may but will not always
     * return synchronously on error. Use stateChanged() to
     * be notified when the connection is established.
     * Returns negative on certain errors such as invalid state
     * or parameters. */
    int connect(const iStringView& url);

    /** Terminate the context connection immediately */
    void disconnect();

    /** Enable event notification */
    iINCOperation* subscribe(uint32_t mask);

    /** Drain the context. If there is nothing to drain, the function returns NULL */
    iINCOperation* drain();

    /** Tell the daemon to exit. The returned operation is unlikely to
     * complete successfully, since the daemon probably died before
     * returning a success notification */
    iINCOperation* exitDaemon();

    /** Returns 1 when the connection is to a local daemon. Returns negative when no connection has been made yet. */
    int isLocal();

    /** Return the server name this context is connected to. */
    iStringView getServerName();

    /** Return the protocol version of the library. */
    xuint32 getProtocolVersion();

    /** Return the protocol version of the connected server.
     * Returns INVALID_INDEX on error. */
    xuint32 getServerProtocolVersion();

public: // signal
    void stateChanged(State st) ISIGNAL(stateChanged, st);
    void eventNotify(iStringView name) ISIGNAL(eventNotify, name);
    void subscribeNotify(uint32_t event, xuint32 idx) ISIGNAL(subscribeNotify, event, idx);

protected:
    virtual ~iINCContext();

private:
    iINCEngine* m_engine;
    std::unordered_set<iINCOperation*> m_operations;

    friend class iINCOperation;
    IX_DISABLE_COPY(iINCContext)
};

} // namespace iShell

#endif // IINCCONTEXT_H
