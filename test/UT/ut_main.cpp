/**
 * @file ut_main.cpp
 * @brief Main entry point for imediaplayer unit tests
 * @details Single main runner supporting module filtering
 */

#include <gtest/gtest.h>
#include <iostream>
#include <cstring>
#include <core/kernel/icoreapplication.h>

using namespace iShell;

// Global flags to control which modules to test
bool g_testKernel = false;
bool g_testThread = false;
bool g_testINC = false;
bool g_testUtils = false;
bool g_testIO = false;

// Parse custom module filtering arguments
void parseCustomArgs(int argc, char** argv) {
    std::cout << "[DEBUG] parseCustomArgs START: g_testINC=" << g_testINC << std::endl;
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
    std::cout << "[DEBUG] parseCustomArgs BEFORE if: g_testINC=" << g_testINC << std::endl;
    if (!g_testKernel && !g_testThread && !g_testINC && !g_testUtils && !g_testIO) {
        std::cout << "[DEBUG] Entering if block, setting all to true" << std::endl;
        g_testKernel = g_testThread = g_testINC = g_testUtils = g_testIO = true;
    }
    std::cout << "[DEBUG] parseCustomArgs END: g_testINC=" << g_testINC 
              << " address=" << (void*)&g_testINC << std::endl;
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

int main(int argc, char** argv) {
    // Create application instance for tests that need event loop
    iCoreApplication app(argc, argv);
    
    // Parse custom arguments first
    parseCustomArgs(argc, argv);
    
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // Add custom environment
    ::testing::AddGlobalTestEnvironment(new ModuleEnvironment());
    
    // Run tests (note: this blocks if any test runs event loop)
    return RUN_ALL_TESTS();
}
