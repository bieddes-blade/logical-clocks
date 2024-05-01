#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <string>

#include <vector>
#include <map>
#include <queue>
#include <set>

#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "safe_queue.hpp"


int NUM_THREADS = 3;
int NUM_WRITES = 1;


std::map<int, std::string> WORDS = {
    { 1, "REQUEST" },
    { 2, "RELEASE" },
    { 3, "ACK" }
};


void sleep(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}


class Message {
public:
    int from;
    int number;
    int timestamp;
    int type; // 1 == request resource, 2 == release resource, 3 == acknowledgement
    Message() : from(-1), number(-1), timestamp(-1), type(-1) {}
    Message(int from_, int number_, int timestamp_, int type_)
        : from(from_), number(number_), timestamp(timestamp_), type(type_) {}
};


class Resource {
public:
    int number = 0;
    bool in_use = false;

    void increment(int my_id) {
        if (in_use) {
            std::cout << "RACE CONDITION, thread " + std::to_string(my_id) + " tring to access resource\n";
        }
        in_use = true;
        sleep(200);
        ++number; // no mutex here
        in_use = false;
    }
};


class Process {
public:
    int my_id;
    bool asking_for_resource = false;
    int writes = NUM_WRITES; // how many times this process will write into the resource
    Resource& resource;
    std::map<int, SafeQueue<Message>>& queue;
    int clock = 0;

    std::map<int, int> sent_count; // how many messages this process has sent
    std::map<int, int> processed_count; // how many messages this process has received in the right order
    std::map<int, int> max_timestamp; // max timestamp of a received message

    struct cmp_timestamp {
        bool operator() (const Message& a, const Message& b) const {
            if (a.timestamp == b.timestamp) {
                return a.from < b.from;
            }
            return a.timestamp < b.timestamp;
        }
    };
    std::set<Message, cmp_timestamp> requests;

    struct cmp_number {
        bool operator() (const Message& a, const Message& b) const {
            return a.number < b.number;
        }
    };
    std::map<int, std::set<Message, cmp_number>> backlog; // messages that were received in the wrong order

    Process(int my_id_, Resource& resource_, std::map<int, SafeQueue<Message>>& queue_) 
        : my_id(my_id_), resource(resource_), queue(queue_) {}

    bool backlog_not_empty() {
        for (int i = 0; i < NUM_THREADS; ++i) {
            if (!backlog[i].empty()) {
                return true;
            }
        }
        return false;
    }

    void loop() {
        while ((writes > 0) || (backlog_not_empty())) {
            // if there are messages in the backlog, not every request has been processed
            int random_value = std::rand() % 10;
            if ((writes > 0) && (random_value == 0) && (!asking_for_resource)) {
                asking_for_resource = true;
                request_resource();
            }
            if (asking_for_resource) {
                check_if_access_granted();
            }
            receive_new_messages();
        }
    }

    void send_message(int other_id, int type) {
        sleep(20);

        if (sent_count.find(other_id) == sent_count.end()) {
            sent_count[other_id] = 1;
        } else {
            ++sent_count[other_id];
        }

        Message new_message = Message(my_id, sent_count[other_id], clock, type);
        queue[other_id].Produce(std::move(new_message));

        std::cout << "Thread " + std::to_string(my_id) + " sent " + WORDS[type] + " to " + 
            std::to_string(other_id) + " at " + std::to_string(clock) + "\n";
        ++clock;
    }

    void process_message(Message message) {
        int sender = message.from;
        ++processed_count[sender];
        max_timestamp[sender] = std::max(max_timestamp[sender], message.timestamp);

        if (message.type == 1) { // processing a request resource message
            Message new_message = message;
            requests.insert(new_message);
            send_message(sender, 3); // send acknowledgement message to sender
        } else if (message.type == 2) {
            // erase one previous request resource message from the sender
            for (auto it = requests.begin(); it != requests.end(); ++it) {
                Message current = *it;
                if ((current.from == sender) && (current.timestamp <= message.timestamp) && (current.type == 1)) {
                    requests.erase(it);
                    break;
                }
            }
        }
    }

    void receive_new_messages() {
        Message message;
        while (queue[my_id].Consume(message)) {
            int sender = message.from;
            clock = std::max(clock + 1, message.timestamp + 1);

            std::cout << "Thread " + std::to_string(my_id) + " received " + WORDS[message.type] +
                " from " + std::to_string(sender) + " at " + std::to_string(clock) + "\n";

            // if this is the first received message from sender
            if (processed_count.find(sender) == processed_count.end()) {
                processed_count[sender] = 0;
                max_timestamp[sender] = -1;
            }
            process_message(message);
        }
    }

    void request_resource() {
        requests.insert(Message(my_id, -1, clock, 1));
        for (int i = 0; i < NUM_THREADS; ++i) {
            if (i != my_id) {
                send_message(i, 1);
            }
        }
        ++clock;
    }

    void check_if_access_granted() {
        if ((requests.size() == 0) || (max_timestamp.size() == 0)) { // change for NUM_THREADS == 1
            return;
        }
        Message min_message = *requests.begin();
        if (min_message.from != my_id) {
            return;
        }
        for (auto it = max_timestamp.begin(); it != max_timestamp.end(); ++it) {
            if ((it->first != my_id) && (it->second <= min_message.timestamp)) {
                return;
            }
        }
        access_granted();
    }

    void access_granted() {
        std::cout << "  Resource was granted to thread " + std::to_string(my_id) + " at " + std::to_string(clock) + "\n";
        resource.increment(my_id);
        --writes;
        ++clock;
        release_resource();
    }

    void release_resource() {
        std::cout << "  Resource was released from thread " + std::to_string(my_id) + " at " + std::to_string(clock) + "\n";
        // erase one previous resource request message from this process
        for (auto it = requests.begin(); it != requests.end(); ++it) {
            Message current = *it;
            if ((current.from == my_id) && (current.timestamp <= clock) && (current.type == 1)) {
                requests.erase(it);
                break;
            }
        }
        // send all other processes a release resource message
        for (int i = 0; i < NUM_THREADS; ++i) {
            if (i != my_id) {
                send_message(i, 2);
            }
        }
        ++clock;
        asking_for_resource = false;
    }
};


void create_process(int id, Resource& resource, std::map<int, SafeQueue<Message>>& queue) {
    Process process(id, resource, queue);
    process.loop();
}


int main() {
    std::srand(std::time(nullptr));
    Resource resource;
    std::map<int, SafeQueue<Message>> queue;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.push_back(std::thread(create_process, i, std::ref(resource), std::ref(queue)));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << resource.number << "\n";
}
