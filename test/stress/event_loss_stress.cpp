// Counts posted events against delivered ones. The queue hands an event between two
// tiers, two threads and, on migration, two queues; every one of those handoffs is a
// chance to drop it, and nothing else in the suite would notice a dropped event.
//
// Phase A drives the wakeUp()/canWait handshake against a sleeping consumer.
// Phase B drives the drain() forwarding path, where a producer latches a queue the
// receiver is about to leave.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "core/kernel/icoreapplication.h"
#include "core/kernel/ievent.h"
#include "core/kernel/iobject.h"
#include "core/thread/ithread.h"

using namespace iShell;

namespace {

const int kType = iEvent::User + 51;
const int kProducers = 4;
const int kPerProducer = 50000;

std::atomic<int> g_delivered(0);
std::atomic<bool> g_stop(false);

class Sink : public iObject
{
public:
    virtual bool event(iEvent* e) IX_OVERRIDE
    {
        if (e->type() == kType) {
            g_delivered.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        return iObject::event(e);
    }
};

class Worker : public iThread
{
protected:
    virtual void run() IX_OVERRIDE { exec(); }
};

void postFixed(iObject* target)
{
    for (int i = 0; i < kPerProducer; ++i)
        iCoreApplication::postEvent(target, new iEvent(kType));
}

void postUntilStop(iObject* target, std::atomic<int>* posted)
{
    while (!g_stop.load(std::memory_order_acquire)) {
        // The consumer is one thread that is also driving the migrations, so an
        // unthrottled producer just exhausts memory before proving anything.
        if ((posted->load(std::memory_order_relaxed) - g_delivered.load(std::memory_order_relaxed)) > 20000) {
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            continue;
        }
        iCoreApplication::postEvent(target, new iEvent(kType));
        posted->fetch_add(1, std::memory_order_relaxed);
    }
}

// Waits for the consumer to catch up. Returns what was actually delivered, so the caller
// reports the shortfall rather than just timing out.
int settle(int expected, int seconds)
{
    const int spins = seconds * 200;
    for (int i = 0; i < spins; ++i) {
        if (g_delivered.load(std::memory_order_relaxed) >= expected)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return g_delivered.load(std::memory_order_relaxed);
}

} // namespace

int main(int argc, char** argv)
{
    iCoreApplication app(argc, argv);
    iThread* mainThread = iThread::currentThread();
    int failures = 0;

    // Phase A: consumer asleep in its own event loop, producers racing the wakeup.
    {
        Worker worker;
        worker.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        Sink* sink = new Sink;
        if (!sink->moveToThread(&worker))
            return 90;

        g_delivered.store(0);
        std::vector<std::thread> producers;
        for (int i = 0; i < kProducers; ++i)
            producers.push_back(std::thread(postFixed, sink));
        for (size_t i = 0; i < producers.size(); ++i)
            producers[i].join();

        const int expected = kProducers * kPerProducer;
        const int got = settle(expected, 30);
        if (got != expected) {
            fprintf(stderr, "PHASE_A posted=%d delivered=%d lost=%d\n", expected, got, expected - got);
            ++failures;
        }

        worker.exit();
        worker.wait();
        if (!sink->moveToThread(mainThread))
            return 91;
        iCoreApplication::dispatchPostedEvents(IX_NULLPTR, 0);
        delete sink;
    }

    // Phase B: receiver migrating out from under producers that already latched a queue.
    {
        Sink* sink = new Sink;
        g_delivered.store(0);
        g_stop.store(false);
        std::atomic<int> posted(0);

        std::vector<std::thread> producers;
        for (int i = 0; i < kProducers; ++i)
            producers.push_back(std::thread(postUntilStop, sink, &posted));

        for (int round = 0; round < 3000; ++round) {
            if (!sink->moveToThread(IX_NULLPTR))
                return 92;
            if (!sink->moveToThread(mainThread))
                return 93;
            iCoreApplication::dispatchPostedEvents(IX_NULLPTR, 0);
        }

        g_stop.store(true, std::memory_order_release);
        for (size_t i = 0; i < producers.size(); ++i)
            producers[i].join();

        // producers are done, so the count is stable and every remaining event is ours
        const int expected = posted.load();
        for (int i = 0; i < 2000 && g_delivered.load() < expected; ++i) {
            iCoreApplication::dispatchPostedEvents(IX_NULLPTR, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const int got = g_delivered.load();
        if (got != expected) {
            fprintf(stderr, "PHASE_B posted=%d delivered=%d lost=%d\n", expected, got, expected - got);
            ++failures;
        }

        delete sink;
    }

    return failures;
}
