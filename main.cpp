#include <iostream>
#include <algorithm>
#include <cstdlib>

#include <vector>
#include <map>
#include <queue>
#include <set>

#include <thread>
#include <chrono>


int NUM_THREADS = 2;
int NUM_WRITES = 1;


void sleep(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}


class Message {
public:
    int from;
    int number;
    int timestamp;
    int type; // 1 == request resource, 2 == release resource, 3 == acknowledgement
    Message(int from_, int number_, int timestamp_, int type_)
        : from(from_), number(number_), timestamp(timestamp_), type(type_) {}
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
    bool asking_for_resource = false;
    int writes = NUM_WRITES; // how many times this process will write into the resource
    Resource& resource;
    std::map<int, std::queue<Message>>& queue;
    int clock = 0;
    std::map<int, int> sent_count; // how many messages this process has sent
    std::map<int, int> processed_count; // how many messages this process has received in the right order
    std::map<int, int> max_timestamp; // max timestamp of a received message
    struct cmp_timestamp {
        bool operator() (const Message& a, const Message& b) const {
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

    Process(int my_id_, Resource& resource_, std::map<int, std::queue<Message>>& queue_) 
        : my_id(my_id_), resource(resource_), queue(queue_) {}

    void loop() {
        while ((writes > 0) || (backlog.size() > 0)) {
            int random_value = std::rand() % 10;
            if ((random_value == 0) && (!asking_for_resource)) {
                ++clock;
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
        if (sent_count.find(other_id) == sent_count.end()) {
            sent_count[other_id] = 1;
        } else {
            ++sent_count[other_id];
        }
        queue[other_id].push(Message(my_id, sent_count[other_id], clock, type));
    }

    void process_message(Message message) {
        int sender = message.from;
        ++processed_count[sender];
        max_timestamp[sender] = std::max(max_timestamp[sender], message.timestamp);
        clock = std::max(clock, message.timestamp + 1);

        if (message.type == 1) {
            requests.insert(message);
            send_message(sender, 3); // send acknowledgement message to sender
        } else if (message.type == 2) {
            for (auto it = requests.begin(); it != requests.end(); ) {
                Message current = *it;
                // erase all previous resource requests from the sender
                if ((current.from == sender) && (current.timestamp <= message.timestamp) && (current.type == 1)) {
                    it = requests.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void receive_new_messages() {
        while (!queue[my_id].empty()) {
            Message message = queue[my_id].front();
            int sender = message.from;

            // if this is the first received message from sender
            if (processed_count.find(sender) == processed_count.end()) {
                processed_count[sender] = 0;
                max_timestamp[sender] = -1;
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
        for (int i = 0; i < NUM_THREADS; ++i) {
            if (i != my_id) {
                send_message(i, 1);
            }
        }
        requests.insert(Message(my_id, -1, clock, 1));
        ++clock;
    }

    void check_if_access_granted() {
        if (requests.size() != 0) {
            Message min_message = *requests.begin();
            if (min_message.from != my_id) {
                return;
            }
        } else {
            return;
        }
        for (auto it = max_timestamp.begin(); it != max_timestamp.end(); ++it) {
            if (it->second < clock) {
                return;
            }
        }
        access_granted();
    }

    void access_granted() {
        resource.increment();
        --writes;
        release_resource();
    }

    void release_resource() {
        for (auto it = requests.begin(); it != requests.end(); ) {
            Message current = *it;
            // erase all previous resource requests from this process
            if ((current.from == my_id) && (current.timestamp <= clock) && (current.type == 1)) {
                it = requests.erase(it);
            } else {
                ++it;
            }
        }
        for (int i = 0; i < NUM_THREADS; ++i) {
            if (i != my_id) {
                send_message(i, 2);
            }
        }
        ++clock;
        asking_for_resource = false;
    }
};


void create_process(int id, Resource& resource, std::map<int, std::queue<Message>>& queue) {
    Process process(id, resource, queue);
    process.loop();
}


int main() {
    std::srand(std::time(nullptr));
    Resource resource;
    std::map<int, std::queue<Message>> queue;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.push_back(std::thread(create_process, i, std::ref(resource), std::ref(queue)));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << resource.number << "\n";
}
