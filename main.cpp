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
        in_use = true;
        sleep(200);
        ++number; // no mutex here
        in_use = false;
    }
};


template<typename T>
class ThreadsafeQueue {
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    bool empty() const {
        return queue_.empty();
    }

 public:
    ThreadsafeQueue() = default;
    ThreadsafeQueue(const ThreadsafeQueue<T> &) = delete ;
    ThreadsafeQueue& operator=(const ThreadsafeQueue<T> &) = delete ;
 
    ThreadsafeQueue(ThreadsafeQueue<T>&& other) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_ = std::move(other.queue_);
    }

    virtual ~ThreadsafeQueue() { }

    unsigned long size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    std::optional<T> pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return {};
        }
        T tmp = queue_.front();
        queue_.pop();
        return tmp;
    }

    void push(const T &item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
    }
};


class Process {
public:
    int my_id;
    bool asking_for_resource = false;
    int writes = NUM_WRITES; // how many times this process will write into the resource
    Resource& resource;
    std::map<int, ThreadsafeQueue<Message>>& queue;
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

    Process(int my_id_, Resource& resource_, std::map<int, ThreadsafeQueue<Message>>& queue_) 
        : my_id(my_id_), resource(resource_), queue(queue_) {}

    bool backlog_not_empty() {
        for (int i = 0; i < NUM_THREADS; ++i) {
            if (!backlog[i].empty()) {
                Message m = *backlog[i].begin();
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
        if (sent_count.find(other_id) == sent_count.end()) {
            sent_count[other_id] = 1;
        } else {
            ++sent_count[other_id];
        }
        int sz = queue[other_id].size();
        queue[other_id].push(Message(my_id, sent_count[other_id], clock, type));
        if (queue[other_id].size() <= sz) {
            std::cout << "WTF\n";
        }
        std::cout << "Thread " + std::to_string(my_id) + " sent message type " + std::to_string(type) + " to " + 
            std::to_string(other_id) + " at " + std::to_string(clock) + "\n";
        ++clock;
    }

    void process_message(Message message) {
        int sender = message.from;
        ++processed_count[sender];
        max_timestamp[sender] = std::max(max_timestamp[sender], message.timestamp);

        if (message.type == 1) { // processing a request resource message
            requests.insert(message);
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
        sleep(20);

        std::optional<Message> optional_message = queue[my_id].pop();
        while (optional_message) {
            Message message = *optional_message;

            int sender = message.from;
            clock = std::max(clock + 1, message.timestamp + 1);

            std::cout << "Thread " + std::to_string(my_id) + " received message type " + std::to_string(message.type) + " from " + 
                std::to_string(sender) + " at " + std::to_string(clock) + "\n";

            // if this is the first received message from sender
            if (processed_count.find(sender) == processed_count.end()) {
                processed_count[sender] = 0;
                max_timestamp[sender] = -1;
            }
            
            if (message.number > processed_count[sender] + 1) {
                // the previous messages from this sender weren't received yet
                backlog[sender].insert(message);
                std::cout << "Thread " + std::to_string(my_id) + " placed message from " + std::to_string(sender) + " at " + 
                    std::to_string(clock) + " into the backlog\n";
            } else {
                // the number of this message == (the number of the last processed message + 1)
                process_message(message);

                // lets go through the messages received in the wrong order and check if any can be processed now
                for (auto it = backlog[sender].begin(); it != backlog[sender].end(); ) {
                    Message current = *it;
                    if (current.number == processed_count[sender] + 1) {
                        // found next consecutive message
                        process_message(current);
                        std::cout << "Thread " + std::to_string(my_id) + " processed message from " + std::to_string(current.from) +  
                            " at " + std::to_string(current.timestamp) + " from the backlog\n";
                        it = backlog[sender].erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            optional_message = queue[my_id].pop();
            if (!optional_message) {
                sleep(50);
                optional_message = queue[my_id].pop();
            }
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
        std::cout << "The resource was granted to thread " + std::to_string(my_id) + " at " + std::to_string(clock) + "\n";
        resource.increment();
        --writes;
        ++clock;
        release_resource();
    }

    void release_resource() {
        std::cout << "The resource was released from thread " + std::to_string(my_id) + " at " + std::to_string(clock) + "\n";
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


void create_process(int id, Resource& resource, std::map<int, ThreadsafeQueue<Message>>& queue) {
    Process process(id, resource, queue);
    process.loop();
}


int main() {
    std::srand(std::time(nullptr));
    Resource resource;
    std::map<int, ThreadsafeQueue<Message>> queue;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.push_back(std::thread(create_process, i, std::ref(resource), std::ref(queue)));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << resource.number << "\n";
}
