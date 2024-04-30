#include <iostream>
#include <algorithm>

#include <vector>
#include <map>
#include <queue>
#include <set>

#include <thread>
#include <chrono>


void sleep(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}


class Message {
public:
    int from;
    int number;
    int timestamp;
    Message(int from_, int number_, int timestamp_)
        : from(from_), number(number_), timestamp(timestamp_) {}
};


class Resource {
public:
    int number = 0;
    bool in_use = false;

    void increment() {
        /*if (in_use == true) {
            std::cout << "Race condition" << "\n";
        }*/

        in_use = true;
        sleep(200);
        ++number; // no mutex here
        in_use = false;
    }
};


class Process {
public:
    int my_id;
    Resource& resource;
    std::map<int, std::queue<Message>>& queue;
    int clock = 0;
    std::map<int, int> sent_count; // how many messages this process has sent
    std::map<int, int> processed_count; // how many messages this process has received in the right order

    struct cmp {
        bool operator() (const Message& a, const Message& b) const {
            return a.number < b.number;
        }
    };

    std::map<int, std::set<Message, cmp>> backlog; // messages that were received in the wrong order

    Process(int my_id_, Resource& resource_, std::map<int, std::queue<Message>>& queue_) 
        : my_id(my_id_), resource(resource_), queue(queue_) {}

    void loop() {
        request_resource();
        if (my_id == 0) {
            send_message(4);
        }
        receive_new_messages();
    }

    void send_message(int other_id) {
        if (sent_count.find(other_id) == sent_count.end()) {
            sent_count[other_id] = 1;
        } else {
            ++sent_count[other_id];
        }
        queue[other_id].push(Message(my_id, sent_count[other_id], clock));
        ++clock;
    }

    void process_message(Message message) {
        ++processed_count[message.from];
        clock = std::max(clock, message.timestamp + 1);
    }

    void receive_new_messages() {
        while (!queue[my_id].empty()) {
            Message message = queue[my_id].front();
            int sender = message.from;

            // if this is the first received message from sender
            if (processed_count.find(sender) == processed_count.end()) {
                processed_count[sender] = 0;
            }
            
            if (message.number > processed_count[sender] + 1) {
                // the previous messages from this sender weren't received yet
                backlog[sender].insert(message);
            } else {
                // the number of this message == (the number of the last processed message + 1)
                process_message(message);

                // lets go through the messages received in the wrong order and check if
                // any can be processed now
                for (auto it = backlog[sender].begin(); it != backlog[sender].end(); ) {
                    Message current = *it;
                    if (current.number == processed_count[sender] + 1) {
                        // found next consecutive message
                        process_message(current);
                        it = backlog[sender].erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            queue[my_id].pop();
        }
    }

    void request_resource() {
        resource.increment();
        ++clock;
    }

    void release_resource() {
        // ...
    }
};


void create_process(int id, Resource& resource, std::map<int, std::queue<Message>>& queue) {
    Process process(id, resource, queue);
    process.loop();
}


int main() {
    Resource resource;
    std::map<int, std::queue<Message>> queue;

    for (int i = 0; i < 5; ++i) {
        std::thread t(create_process, i, std::ref(resource), std::ref(queue));
        t.detach();
    }

    sleep(1000);

    std::cout << resource.number << "\n";
}
