#include <thread>
#include <chrono>
#include <vector>
#include <iostream>


class Resource {
public:
    int number = 0;
    bool in_use = false;

    void increment() {
        if (in_use == true) {
            std::cout << "Race condition" << "\n";
        }

        in_use = true;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++number;
        in_use = false;
    }
};


class Process {
public:
    Resource& resource;
    Process(Resource& resource_) : resource(resource_) {}

    void sleep() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void loop() {
        // request to use resource once in a while
        resource.increment();
    }

    void send_message() {
        // send a message to another process
    }

    void request_resource() {
        // ...
    }

    void release_resource() {
        // ...
    }
};


void create_process(Resource& resource) {
    Process process(resource);
    process.loop();
}


int main() {
    Resource resource;

    for (int i = 0; i < 5; ++i) {
        std::thread t(create_process, std::ref(resource));
        t.detach();
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << resource.number << "\n";
}
