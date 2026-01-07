/**
 * @file ut_main.cpp
 * @brief Main entry point for imediaplayer unit tests
 * @details Single main runner supporting module filtering
 */

#include <gtest/gtest.h>
#include <iostream>
#include <cstring>
#include <core/kernel/icoreapplication.h>
#include <core/thread/ieventdispatcher_generic.h>
#ifdef IBUILD_HAVE_GLIB
#include <core/thread/ieventdispatcher_glib.h>
#endif

using namespace iShell;

// Global flags to control which modules to test
bool g_testKernel = false;
bool g_testThread = false;
bool g_testINC = false;
bool g_testUtils = false;
bool g_testIO = false;

// Parse custom module filtering arguments
void parseCustomArgs(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--module=", 9) == 0) {
            const char* module = argv[i] + 9;
            if (strcmp(module, "kernel") == 0) {
                g_testKernel = true;
            } else if (strcmp(module, "thread") == 0) {
                g_testThread = true;
            } else if (strcmp(module, "inc") == 0) {
                g_testINC = true;
            } else if (strcmp(module, "utils") == 0) {
                g_testUtils = true;
            } else if (strcmp(module, "io") == 0) {
                g_testIO = true;
            } else if (strcmp(module, "all") == 0) {
                g_testKernel = g_testThread = g_testINC = g_testUtils = g_testIO = true;
            }
        } else if (strcmp(argv[i], "--help-modules") == 0) {
            std::cout << "Available modules:\n"
                      << "  --module=kernel   : Test EventLoop, EventDispatcher, EventSource\n"
                      << "  --module=thread   : Test Mutex, Condition, Atomic\n"
                      << "  --module=inc      : Test INC Protocol, TCP Device\n"
                      << "  --module=utils    : Test String, ByteArray\n"
                      << "  --module=io       : Test IODevice, Log\n"
                      << "  --module=all      : Test all modules (default)\n"
                      << "\n"
                      << "You can also use standard gtest filters:\n"
                      << "  --gtest_filter=TestSuite.TestCase\n"
                      << "  --gtest_filter=EventLoop*\n"
                      << "  --gtest_list_tests\n";
            exit(0);
        }
    }

    // If no module specified, test all
    if (!g_testKernel && !g_testThread && !g_testINC && !g_testUtils && !g_testIO) {
        g_testKernel = g_testThread = g_testINC = g_testUtils = g_testIO = true;
    }
}

// Test environment setup
class ModuleEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        std::cout << "==================================================" << std::endl;
        std::cout << "  imediaplayer Unit Test Suite" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << "Enabled Modules:" << std::endl;
        if (g_testKernel) std::cout << "  - Kernel (EventLoop, EventDispatcher, EventSource)" << std::endl;
        if (g_testThread) std::cout << "  - Thread (Mutex, Condition, Atomic)" << std::endl;
        if (g_testINC) std::cout << "  - INC (Protocol, TCP Device)" << std::endl;
        if (g_testUtils) std::cout << "  - Utils (String, ByteArray)" << std::endl;
        if (g_testIO) std::cout << "  - IO (IODevice, Log)" << std::endl;
        std::cout << "==================================================" << std::endl;
    }
};

class TestCoreApplication : public iCoreApplication {
    bool m_useGlib;
public:
    TestCoreApplication(int argc, char** argv) 
        : iCoreApplication(argc, argv), m_useGlib(false) {}

    void setUseGlib(bool use) {
        m_useGlib = use;
    }

    iEventDispatcher* doCreateEventDispatcher() const override {
        if (m_useGlib) {
            #ifdef IBUILD_HAVE_GLIB
            std::cout << "Creating iEventDispatcher_Glib" << std::endl;
            return new iEventDispatcher_Glib();
            #else
            std::cout << "GLib not available, falling back to generic" << std::endl;
            return new iEventDispatcher_generic();
            #endif
        } else {
            std::cout << "Creating iEventDispatcher_generic" << std::endl;
            return new iEventDispatcher_generic();
        }
    }
};

// Global function to allow dynamic switching from tests
void setUseGlibDispatcher(bool use) {
    iCoreApplication* app = iCoreApplication::instance();
    if (app) {
        static_cast<TestCoreApplication*>(app)->setUseGlib(use);
    }
}

int main(int argc, char** argv) {
    // Parse custom arguments first
    parseCustomArgs(argc, argv);

    // Create application instance for tests that need event loop
    TestCoreApplication app(argc, argv);

    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Add custom environment
    ::testing::AddGlobalTestEnvironment(new ModuleEnvironment());

    // Run tests (note: this blocks if any test runs event loop)
    return RUN_ALL_TESTS();
}
