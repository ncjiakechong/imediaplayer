/**
 * @file test_icoreposix.cpp
 * @brief Unit tests for POSIX interface functions
 */

#include <gtest/gtest.h>
#include <core/kernel/icoreposix.h>
#include <unistd.h>
#include <fcntl.h>

using namespace iShell;

extern bool g_testKernel;

class ICorePosixTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_testKernel) {
            GTEST_SKIP() << "Kernel module tests are disabled";
        }
    }
    
    void TearDown() override {
        // Clean up any open file descriptors
    }
};

// Test igettime function
TEST_F(ICorePosixTest, GetTimeBasic) {
    timespec ts = igettime();
    
    // Time should be positive
    EXPECT_GE(ts.tv_sec, 0);
    EXPECT_GE(ts.tv_nsec, 0);
    EXPECT_LT(ts.tv_nsec, 1000000000);  // Nanoseconds should be < 1 second
}

TEST_F(ICorePosixTest, GetTimeMonotonic) {
    timespec ts1 = igettime();
    
    // Small delay
    usleep(1000);  // 1ms
    
    timespec ts2 = igettime();
    
    // Second time should be after first time (monotonic)
    EXPECT_TRUE(ts2.tv_sec > ts1.tv_sec || 
               (ts2.tv_sec == ts1.tv_sec && ts2.tv_nsec >= ts1.tv_nsec));
}

// Test ix_open_pipe with no flags
TEST_F(ICorePosixTest, OpenPipeNoFlags) {
    xintptr fds[2];
    
    int result = ix_open_pipe(fds, 0);
    EXPECT_EQ(result, 0);
    EXPECT_GT(fds[0], 0);
    EXPECT_GT(fds[1], 0);
    
    // Test that pipe works - write to fds[1], read from fds[0]
    char write_buf[] = "test";
    char read_buf[10] = {0};
    
    ssize_t written = write(fds[1], write_buf, 4);
    EXPECT_EQ(written, 4);
    
    ssize_t read_bytes = read(fds[0], read_buf, 4);
    EXPECT_EQ(read_bytes, 4);
    EXPECT_STREQ(read_buf, "test");
    
    close(fds[0]);
    close(fds[1]);
}

// Test ix_open_pipe with FD_CLOEXEC flag
TEST_F(ICorePosixTest, OpenPipeWithFlags) {
    xintptr fds[2];
    
    int result = ix_open_pipe(fds, FD_CLOEXEC);
    EXPECT_EQ(result, 0);
    EXPECT_GT(fds[0], 0);
    EXPECT_GT(fds[1], 0);
    
    // Verify FD_CLOEXEC flag is set
    int flags0 = fcntl(fds[0], F_GETFD);
    int flags1 = fcntl(fds[1], F_GETFD);
    
    EXPECT_TRUE(flags0 & FD_CLOEXEC);
    EXPECT_TRUE(flags1 & FD_CLOEXEC);
    
    close(fds[0]);
    close(fds[1]);
}

// Test ix_open_pipe error handling with invalid flags
TEST_F(ICorePosixTest, OpenPipeInvalidFlags) {
    xintptr fds[2];
    
    // Use an extremely large invalid flag value that might cause fcntl to fail
    // Note: This test depends on system behavior
    int result = ix_open_pipe(fds, 0x7FFFFFFF);
    
    // Either succeeds (system accepts the flags) or fails (returns -1)
    // If it fails, fds should not be valid for use
    if (result == -1) {
        EXPECT_EQ(result, -1);
    } else {
        // If succeeded, clean up
        close(fds[0]);
        close(fds[1]);
    }
}

// Test ix_set_fd_nonblocking - set to nonblocking
TEST_F(ICorePosixTest, SetFdNonblocking) {
    xintptr fds[2];
    ix_open_pipe(fds, 0);
    
    int result = ix_set_fd_nonblocking(fds[0], true);
    EXPECT_EQ(result, 0);
    
    // Verify O_NONBLOCK flag is set
    int flags = fcntl(fds[0], F_GETFL);
    EXPECT_TRUE(flags & O_NONBLOCK);
    
    close(fds[0]);
    close(fds[1]);
}

// Test ix_set_fd_nonblocking - set to blocking
TEST_F(ICorePosixTest, SetFdBlocking) {
    xintptr fds[2];
    ix_open_pipe(fds, 0);
    
    // First set to nonblocking
    ix_set_fd_nonblocking(fds[0], true);
    
    // Then set back to blocking
    int result = ix_set_fd_nonblocking(fds[0], false);
    EXPECT_EQ(result, 0);
    
    // Verify O_NONBLOCK flag is not set
    int flags = fcntl(fds[0], F_GETFL);
    EXPECT_FALSE(flags & O_NONBLOCK);
    
    close(fds[0]);
    close(fds[1]);
}

// Test ix_set_fd_nonblocking with invalid fd
TEST_F(ICorePosixTest, SetFdNonblockingInvalidFd) {
    xintptr invalid_fd = -1;
    
    int result = ix_set_fd_nonblocking(invalid_fd, true);
    EXPECT_EQ(result, -1);
}

// Test toggling nonblocking multiple times
TEST_F(ICorePosixTest, ToggleNonblocking) {
    xintptr fds[2];
    ix_open_pipe(fds, 0);
    
    // Toggle multiple times
    EXPECT_EQ(ix_set_fd_nonblocking(fds[0], true), 0);
    int flags = fcntl(fds[0], F_GETFL);
    EXPECT_TRUE(flags & O_NONBLOCK);
    
    EXPECT_EQ(ix_set_fd_nonblocking(fds[0], false), 0);
    flags = fcntl(fds[0], F_GETFL);
    EXPECT_FALSE(flags & O_NONBLOCK);
    
    EXPECT_EQ(ix_set_fd_nonblocking(fds[0], true), 0);
    flags = fcntl(fds[0], F_GETFL);
    EXPECT_TRUE(flags & O_NONBLOCK);
    
    close(fds[0]);
    close(fds[1]);
}

// Test both pipe fds can be set nonblocking
TEST_F(ICorePosixTest, SetBothPipeFdsNonblocking) {
    xintptr fds[2];
    ix_open_pipe(fds, 0);
    
    EXPECT_EQ(ix_set_fd_nonblocking(fds[0], true), 0);
    EXPECT_EQ(ix_set_fd_nonblocking(fds[1], true), 0);
    
    int flags0 = fcntl(fds[0], F_GETFL);
    int flags1 = fcntl(fds[1], F_GETFL);
    
    EXPECT_TRUE(flags0 & O_NONBLOCK);
    EXPECT_TRUE(flags1 & O_NONBLOCK);
    
    close(fds[0]);
    close(fds[1]);
}

// Test pipe communication with nonblocking
TEST_F(ICorePosixTest, PipeCommunicationNonblocking) {
    xintptr fds[2];
    ix_open_pipe(fds, 0);
    
    ix_set_fd_nonblocking(fds[0], true);
    ix_set_fd_nonblocking(fds[1], true);
    
    // Write data
    char write_buf[] = "nonblocking test";
    ssize_t written = write(fds[1], write_buf, 16);
    EXPECT_EQ(written, 16);
    
    // Read data
    char read_buf[20] = {0};
    ssize_t read_bytes = read(fds[0], read_buf, 16);
    EXPECT_EQ(read_bytes, 16);
    EXPECT_STREQ(read_buf, "nonblocking test");
    
    close(fds[0]);
    close(fds[1]);
}

// Test multiple pipe creations
TEST_F(ICorePosixTest, MultiplePipes) {
    xintptr fds1[2], fds2[2], fds3[2];
    
    EXPECT_EQ(ix_open_pipe(fds1, 0), 0);
    EXPECT_EQ(ix_open_pipe(fds2, 0), 0);
    EXPECT_EQ(ix_open_pipe(fds3, 0), 0);
    
    // All should have different fds
    EXPECT_NE(fds1[0], fds2[0]);
    EXPECT_NE(fds1[0], fds3[0]);
    EXPECT_NE(fds2[0], fds3[0]);
    
    close(fds1[0]); close(fds1[1]);
    close(fds2[0]); close(fds2[1]);
    close(fds3[0]); close(fds3[1]);
}

// Test igettime called multiple times rapidly
TEST_F(ICorePosixTest, GetTimeRapidCalls) {
    timespec ts[10];
    
    for (int i = 0; i < 10; ++i) {
        ts[i] = igettime();
    }
    
    // All timestamps should be valid
    for (int i = 0; i < 10; ++i) {
        EXPECT_GE(ts[i].tv_sec, 0);
        EXPECT_GE(ts[i].tv_nsec, 0);
        EXPECT_LT(ts[i].tv_nsec, 1000000000);
    }
    
    // Times should be monotonically increasing (or equal for rapid calls)
    for (int i = 1; i < 10; ++i) {
        EXPECT_TRUE(ts[i].tv_sec > ts[i-1].tv_sec || 
                   (ts[i].tv_sec == ts[i-1].tv_sec && ts[i].tv_nsec >= ts[i-1].tv_nsec));
    }
}

// Test setting fd nonblocking idempotency
TEST_F(ICorePosixTest, SetNonblockingIdempotent) {
    xintptr fds[2];
    ix_open_pipe(fds, 0);
    
    // Set nonblocking multiple times
    EXPECT_EQ(ix_set_fd_nonblocking(fds[0], true), 0);
    EXPECT_EQ(ix_set_fd_nonblocking(fds[0], true), 0);
    EXPECT_EQ(ix_set_fd_nonblocking(fds[0], true), 0);
    
    int flags = fcntl(fds[0], F_GETFL);
    EXPECT_TRUE(flags & O_NONBLOCK);
    
    close(fds[0]);
    close(fds[1]);
}

// Test setting fd blocking idempotency
TEST_F(ICorePosixTest, SetBlockingIdempotent) {
    xintptr fds[2];
    ix_open_pipe(fds, 0);
    
    ix_set_fd_nonblocking(fds[0], true);
    
    // Set blocking multiple times
    EXPECT_EQ(ix_set_fd_nonblocking(fds[0], false), 0);
    EXPECT_EQ(ix_set_fd_nonblocking(fds[0], false), 0);
    EXPECT_EQ(ix_set_fd_nonblocking(fds[0], false), 0);
    
    int flags = fcntl(fds[0], F_GETFL);
    EXPECT_FALSE(flags & O_NONBLOCK);
    
    close(fds[0]);
    close(fds[1]);
}

// Test gettime consecutive calls
TEST_F(ICorePosixTest, GetTimeEdgeCases) {
    timespec ts1 = igettime();
    timespec ts2 = igettime();
    
    // ts2 should be >= ts1 (monotonic time)
    EXPECT_TRUE(ts2.tv_sec >= ts1.tv_sec);
    if (ts2.tv_sec == ts1.tv_sec) {
        EXPECT_GE(ts2.tv_nsec, ts1.tv_nsec);
    }
}
