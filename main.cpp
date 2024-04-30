#include <map>
#include <queue>
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
    int my_id;
    Resource& resource;
    std::map<int, std::queue<int>>& queue;

    Process(int my_id_, Resource& resource_, std::map<int, std::queue<int>>& queue_) 
        : my_id(my_id_), resource(resource_), queue(queue_) {}

    void sleep() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void loop() {
        request_resource();
        if (my_id == 0) {
            send_message(4);
        }
        sleep();
        check_new_messages();
    }

    void send_message(int other_id) {
        queue[other_id].push(my_id);
        std::cout << "Thread " << my_id << " sent a message" << "\n";
    }

    void check_new_messages() {
        if (!queue[my_id].empty()) {
            std::cout << "Thread " << my_id << " received a message" << "\n";
        }
    }

    void request_resource() {
        resource.increment();
    }

    void release_resource() {
        // ...
    }
};


void create_process(int id, Resource& resource, std::map<int, std::queue<int>>& queue) {
    Process process(id, resource, queue);
    process.loop();
}


int main() {
    Resource resource;
    std::map<int, std::queue<int>> queue;

    for (int i = 0; i < 5; ++i) {
        std::thread t(create_process, i, std::ref(resource), std::ref(queue));
        t.detach();
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << resource.number << "\n";
}
