#include <atomic>
#include <thread>

#include "core/kernel/icoreapplication.h"
#include "core/kernel/ievent.h"
#include "core/kernel/iobject.h"

using namespace iShell;

namespace {

std::atomic<bool> stop(false);
iObject* target = IX_NULLPTR;

void postDeferredDeletes()
{
    while (!stop.load(std::memory_order_acquire))
        iCoreApplication::postEvent(target, new iDeferredDeleteEvent());
}

} // namespace

int main(int argc, char** argv)
{
    iCoreApplication app(argc, argv);
    iObject receiver;
    target = &receiver;

    // Keep one event pending so every producer reaches the compression flags.
    iCoreApplication::postEvent(&receiver, new iEvent(iEvent::User + 1));

    std::thread first(postDeferredDeletes);
    std::thread second(postDeferredDeletes);

    for (int i = 0; i < 400000; ++i)
        receiver.blockSignals((i & 1) != 0);

    stop.store(true, std::memory_order_release);
    first.join();
    second.join();
    return 0;
}
