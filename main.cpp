#include <thread>
#include <chrono>
#include <iostream>

class Resourse {
public:
    int number = 0;
    bool in_use = false;

    void increment() {
        if (in_use == true) {
            std::cout << "Race condition" << "\n";
        }
        in_use = true;
        //std::this_thread::sleep_for(std::chrono::seconds(1));
        ++number;
        in_use = false;
    }
};

void use_resourse(Resourse& resourse) {
    resourse.increment();
}

int main() {
    Resourse resourse;

    for (int i = 0; i < 5; ++i) {
        std::thread t(use_resourse, std::ref(resourse));
        t.detach();
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << resourse.number << "\n";
}
