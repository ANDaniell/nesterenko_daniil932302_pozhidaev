#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <string>

struct Event {
    int id;
    std::string payload;
};

class Monitor {
public:
    void provide(Event* ev) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return !ready; });
        data = ev;
        ready = true;
        std::cout << "Provider: sent event id=" << ev->id
                  << ", payload=" << ev->payload << std::endl;
        cv.notify_one();
    }

    bool consume(Event*& out) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return ready || !running; });
        if (!running && !ready) return false;
        out = data;
        ready = false;
        std::cout << "Consumer: received event id=" << out->id
                  << ", payload=" << out->payload << std::endl;
        cv.notify_one();
        return true;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mtx);
        running = false;
        cv.notify_all();
    }

private:
    std::mutex mtx;
    std::condition_variable cv;

    bool ready = false;
    bool running = true;

    Event* data = nullptr;
};

int main() {
    Monitor m;
    std::thread provider([&]() {
        for (int i = 1; i <= 5; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            Event* ev = new Event{ i, "Payload_" + std::to_string(i) };
            m.provide(ev);
        }
        m.stop();
    });

    std::thread consumer([&]() {
        Event* ev = nullptr;
        while (m.consume(ev)) {
            delete ev;
        }
    });

    provider.join();
    consumer.join();

    std::cout << "Finished." << std::endl;
    return 0;
}
