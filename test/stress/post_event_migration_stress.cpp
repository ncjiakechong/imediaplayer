#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "core/kernel/icoreapplication.h"
#include "core/kernel/ievent.h"
#include "core/kernel/iobject.h"
#include "core/thread/ithread.h"

using namespace iShell;

namespace {

std::atomic<bool> stop(false);
iObject* target = IX_NULLPTR;

void postEvents()
{
    while (!stop.load(std::memory_order_acquire)) {
        iCoreApplication::postEvent(target, new iEvent(iEvent::User + 1));
        std::this_thread::sleep_for(std::chrono::microseconds(20));
    }
}

} // namespace

int main(int argc, char** argv)
{
    iCoreApplication app(argc, argv);
    iThread* mainThread = iThread::currentThread();
    iObject* receiver = new iObject;
    target = receiver;

    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i)
        producers.push_back(std::thread(postEvents));

    // Detaching creates thread data with no iThread reference. Repeatedly retiring it
    // exercises the postEvent() latch versus moveToThread() publication handshake.
    for (int round = 0; round < 4000; ++round) {
        if (!receiver->moveToThread(IX_NULLPTR))
            return 1;
        if (!receiver->moveToThread(mainThread))
            return 2;
        iCoreApplication::dispatchPostedEvents(IX_NULLPTR, 0);
    }

    stop.store(true, std::memory_order_release);
    for (size_t i = 0; i < producers.size(); ++i)
        producers[i].join();

    iCoreApplication::dispatchPostedEvents(IX_NULLPTR, 0);
    delete receiver;
    return 0;
}
