#include <atomic>
#include <thread>

#include "core/kernel/icoreapplication.h"
#include "core/kernel/ievent.h"
#include "core/kernel/iobject.h"
#include "core/thread/ithread.h"

using namespace iShell;

namespace {

std::atomic<iObject*> target((iObject*)IX_NULLPTR);
std::atomic<bool> migrated(false);
// not named `destroyed`: inside a thread subclass that would resolve to iObject::destroyed
std::atomic<bool> receiverGone(false);
std::atomic<bool> stop(false);
iThread* mainThread = IX_NULLPTR;

class Owner : public iThread
{
public:
    Owner() : receiver(IX_NULLPTR) {}
    iObject* receiver;

protected:
    void run() IX_OVERRIDE
    {
        receiver = new iObject;
        target.store(receiver, std::memory_order_release);

        // give the producers time to latch this thread's queue
        iThread::msleep(30);

        receiver->moveToThread(mainThread);
        migrated.store(true, std::memory_order_release);

        // Stall so anything posted around the flip stays parked here while the main
        // thread destroys the receiver; the loop below then drains those entries.
        while (!receiverGone.load(std::memory_order_acquire))
            iThread::msleep(1);

        exec();
    }
};

void postEvents()
{
    while (!stop.load(std::memory_order_acquire)) {
        iObject* receiver = target.load(std::memory_order_acquire);
        if (IX_NULLPTR != receiver)
            iCoreApplication::postEvent(receiver, new iEvent(iEvent::User + 1));
    }
}

} // namespace

int main(int argc, char** argv)
{
    iCoreApplication app(argc, argv);
    mainThread = iThread::currentThread();

    Owner owner;
    owner.start();

    std::thread producers[4];
    for (int i = 0; i < 4; ++i)
        producers[i] = std::thread(postEvents);

    while (!migrated.load(std::memory_order_acquire))
        iThread::msleep(1);

    stop.store(true, std::memory_order_release);
    for (int i = 0; i < 4; ++i)
        producers[i].join();

    iObject* receiver = owner.receiver;
    target.store(IX_NULLPTR, std::memory_order_release);
    delete receiver;
    receiverGone.store(true, std::memory_order_release);

    iThread::msleep(200);
    owner.exit();
    owner.wait();
    return 0;
}
