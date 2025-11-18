# iMediaPlayer

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++11](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Test Coverage](https://img.shields.io/badge/coverage-51.56%25-yellow.svg)](CORE_COVERAGE_REPORT.md)
[![Memory Safety](https://img.shields.io/badge/ASan-verified-success.svg)](ASAN_GENERIC_DISPATCHER_TEST.md)
[![Tests](https://img.shields.io/badge/tests-1776%2F1781%20passing-brightgreen.svg)](TEST_COVERAGE_PROGRESS.md)

A lightweight, high-performance C++ multimedia framework inspired by Qt, designed **without Qt dependencies**.
Features a complete object system with signals/slots, zero-copy IPC networking, and GStreamer-based media playback.

**Status**: Production Ready | **Test Coverage**: 61.07% (Core Module) | **Test Pass Rate**: 99.7% (1778/1781)

## 🌟 Key Features

### Core Object System (iObject)

- **🔌 Modern Signal/Slot Mechanism**
  - Compile-time type safety with automatic argument checking
  - Lambda expression and function pointer support
  - Zero-overhead signal forwarding
  - Thread-safe cross-thread signal delivery

- **🏷️ Dynamic Property System**
  - Runtime property access with type safety
  - Property change notifications
  - Observable pattern support
  - Metadata introspection

- **🧵 Threading Infrastructure**
  - POSIX thread wrapper with automatic lifetime management
  - **Generic event dispatcher** (eliminates GLib heap-use-after-free)
  - Lock-free event loop with priority-based scheduling
  - Thread-local storage and atomic operations
  - **ASan-verified**: No memory leaks, no use-after-free

- **📡 INC Protocol (Inter-Node Communication)**
  - **Zero-copy data transmission** via shared memory (10-100x faster)
  - **Async RPC** with type-safe method calls
  - **Event Pub/Sub** for efficient broadcasting
  - **Binary streams** for high-performance data transfer
  - **Auto-reconnect** with robust connection management
  - TCP/Unix domain socket support

### Media Playback

- **GStreamer Integration**
  - Hardware-accelerated decoding
  - Multiple format support
  - Streaming capabilities


## 📚 Usage Examples

### Signal/Slot Communication

```cpp
#include <core/kernel/iobject.h>

class Publisher : public iShell::iObject {
    IX_OBJECT(Publisher)
public:
    void dataReady(int value) ISIGNAL(dataReady, value);
};

class Subscriber : public iShell::iObject {
    IX_OBJECT(Subscriber)
public:
    void onDataReceived(int value) {
        printf("Received: %d\n", value);
    }
};

// Connect signal to slot
Publisher pub;
Subscriber sub;
iObject::connect(&pub, &Publisher::dataReady, 
                 &sub, &Subscriber::onDataReceived);

// Emit signal
IEMIT pub.dataReady(42);  // Prints: Received: 42

// Lambda support
iObject::connect(&pub, &Publisher::dataReady, 
                 [](int val) { printf("Lambda: %d\n", val); });
```

### Property System

```cpp
class Config : public iShell::iObject {
    IX_OBJECT(Config)
    
    IPROPERTY_BEGIN
        IPROPERTY_ITEM("timeout", IREAD timeout, IWRITE setTimeout, INOTIFY timeoutChanged)
    IPROPERTY_END

public:
    int timeout() const { return m_timeout; }
    void setTimeout(int ms) { 
        if (m_timeout != ms) {
            m_timeout = ms;
            IEMIT timeoutChanged(ms);
        }
    }
    void timeoutChanged(int ms) ISIGNAL(timeoutChanged, ms);

private:
    int m_timeout = 1000;
};

// Usage
Config cfg;
cfg.observeProperty("timeout", &cfg, &Config::onTimeoutChanged);
cfg.setProperty("timeout", iVariant(5000));
```

### Threading

```cpp
#include <core/thread/ithread.h>

class Worker : public iShell::iThread {
protected:
    void run() override {
        // Long-running task in separate thread
        for (int i = 0; i < 100; ++i) {
            processData(i);
            msleep(10);
        }
    }
};

Worker worker;
worker.start();
worker.wait();  // Block until finished
```

### INC Protocol

```cpp
#include <core/inc/iincserver.h>
#include <core/inc/iinccontext.h>

// Server
iShell::iINCServer server;
server.listenOn("tcp://0.0.0.0:8080");

// Client
iShell::iINCContext client;
client.connectTo("tcp://127.0.0.1:8080");

// Send data (zero-copy)
iShell::iByteArray data = "Hello, INC!";
client.send(data);
```


## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Inspired by the **Qt Framework**'s object model and signal/slot mechanism
- Uses **GStreamer** for multimedia capabilities
- Threading design influenced by modern C++ best practices
- Special thanks to all contributors and testers

