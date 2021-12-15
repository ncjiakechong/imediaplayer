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

#include <core/kernel/iobject.h>
#include <inc/iincglobal.h>

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
    /** Generic notification callback prototype */
    typedef void (*notify_cb_t)(iINCContext* c, void* userdata);

    /** A generic callback for operation completion */
    typedef void (*success_cb_t) (iINCContext* c, int success, void* userdata);

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

    /** Some special flags for contexts. */
    enum Flag {
        FLAG_NOFLAGS = 0x0000U, /**< Flag to pass when no specific options are needed (used to avoid casting) */
        FLAG__NOFAIL = 0x0002U  
        /**< Don't fail if the daemon is not available when connect() is called, 
          * instead enter STATE_CONNECTING state and wait for the daemon to appear. */
    };
    typedef uint Flags;

    /** Subscription event mask, as used by pa_context_subscribe() */
    enum SubscriptionMask {
        SUBSCRIPTION_MASK_NULL = 0x0000U,           /**< No events */
        SUBSCRIPTION_MASK_SINK = 0x0001U,           /**< Sink events */
        SUBSCRIPTION_MASK_SOURCE = 0x0002U,         /**< Source events */
        SUBSCRIPTION_MASK_SINK_INPUT = 0x0004U,     /**< Sink input events */
        SUBSCRIPTION_MASK_SOURCE_OUTPUT = 0x0008U,  /**< Source output events */
        SUBSCRIPTION_MASK_MODULE = 0x0010U,         /**< Module events */
        SUBSCRIPTION_MASK_CLIENT = 0x0020U,         /**< Client events */
        SUBSCRIPTION_MASK_SAMPLE_CACHE = 0x0040U,   /**< Sample cache events */
        SUBSCRIPTION_MASK_SERVER = 0x0080U,         /**< Other global server changes. */
        SUBSCRIPTION_MASK_ALL = SUBSCRIPTION_MASK_SINK | SUBSCRIPTION_MASK_SOURCE | SUBSCRIPTION_MASK_SINK_INPUT | SUBSCRIPTION_MASK_SOURCE_OUTPUT | \
                                SUBSCRIPTION_MASK_MODULE | SUBSCRIPTION_MASK_CLIENT | SUBSCRIPTION_MASK_SAMPLE_CACHE | SUBSCRIPTION_MASK_SERVER
        /**< Catch all events */
    };
    typedef uint SubscriptionMasks;

    /** Subscription event types*/
    enum SubscriptionEventType {
        SUBSCRIPTION_EVENT_SINK = 0x0000U,          /**< Event type: Sink */
        SUBSCRIPTION_EVENT_SOURCE = 0x0001U,        /**< Event type: Source */
        SUBSCRIPTION_EVENT_SINK_INPUT = 0x0002U,    /**< Event type: Sink input */
        SUBSCRIPTION_EVENT_SOURCE_OUTPUT = 0x0003U, /**< Event type: Source output */
        SUBSCRIPTION_EVENT_MODULE = 0x0004U,        /**< Event type: Module */
        SUBSCRIPTION_EVENT_CLIENT = 0x0005U,        /**< Event type: Client */
        SUBSCRIPTION_EVENT_SAMPLE_CACHE = 0x0006U,  /**< Event type: Sample cache item */
        SUBSCRIPTION_EVENT_SERVER = 0x0007U,        /**< Event type: Global server change, only occurring with SUBSCRIPTION_EVENT_CHANGE. */
        SUBSCRIPTION_EVENT_CARD = 0x0009U,          /**< Event type: Card \since 0.9.15 */
        SUBSCRIPTION_EVENT_FACILITY_MASK = 0x000FU, /**< A mask to extract the event type from an event value */
        SUBSCRIPTION_EVENT_NEW = 0x0000U,           /**< A new object was created */
        SUBSCRIPTION_EVENT_CHANGE = 0x0010U,        /**< A property of the object was modified */
        SUBSCRIPTION_EVENT_REMOVE = 0x0020U,        /**< An object was removed */
        SUBSCRIPTION_EVENT_TYPE_MASK = 0x0030U      /**< A mask to extract the event operation from an event value */
    };

    iINCContext(iStringView name, iObject *parent = IX_NULLPTR);

    /** Increase the reference count by one */
    void ref();

    /** Decrease the reference count by one */
    void deref();

    /** Return the current context status */
    State getState() const;

    /** Connect the context to the specified server. If server is NULL,
     * connect to the default server. This routine may but will not always
     * return synchronously on error. Use stateChanged() to
     * be notified when the connection is established. 
     * Returns negative on certain errors such as invalid state
     * or parameters. */
    int connect(iStringView server, Flags flags);

    /** Terminate the context connection immediately */
    void disconnect();

    /** Enable event notification */
    iINCOperation* subscribe(SubscriptionMasks m, success_cb_t cb, void *userdata);

    /** Drain the context. If there is nothing to drain, the function returns NULL */
    iINCOperation* drain(notify_cb_t cb, void *userdata);

    /** Tell the daemon to exit. The returned operation is unlikely to
     * complete successfully, since the daemon probably died before
     * returning a success notification */
    iINCOperation* exitDaemon(success_cb_t cb, void *userdata);

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
    void stateChanged(State st) ISIGNAL(stateChanged, st)
    void eventNotify(iStringView name) ISIGNAL(eventNotify, name)
    void subscribeNotify(SubscriptionEventType t, xuint32 idx) ISIGNAL(subscribeNotify, t, idx)

protected:
    virtual ~iINCContext();

private:
    std::unordered_set<iINCOperation*> m_operations;

    friend class iINCOperation;
    IX_DISABLE_COPY(iINCContext)
};

} // namespace iShell

#endif // IINCCONTEXT_H
