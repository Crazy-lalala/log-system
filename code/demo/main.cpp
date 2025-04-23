#include "logger.hpp"

std::atomic<bool> g_stop(false);

void OutLoop(const int index) {
    int i = index * 100 + 1;
    while(!g_stop.load()) {
        if( 0 == i % 100) {
            i -= 100;
        }

        if(0 == index % 6){
            CDebug << "index is " << i << std::endl;
        }
        else if (1 == index % 6){
            CNeed << "index is " << i << std::endl;
        }
        else if(2 == index % 6){
            CInfo << "index is " << i << std::endl;
        }
        else if(3 == index % 6){
            CWarn << "index is " << i << std::endl;
        }
        else if(4 == index % 6){
            CError << "index is " << i << std::endl;
        }
        else if(5 == index % 6){
            CFatal << "index is " << i << std::endl;
        }
        ++i;
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main() {
    std::vector<std::thread> threads;
    for(int i = 0; i < 6000; ++i) {
        threads.emplace_back([i]() { OutLoop(i); });
    }

    std::this_thread::sleep_for(std::chrono::seconds(50));
    g_stop.store(true);
    for(auto &t : threads) {
        if(t.joinable()) {
            t.join();
        }
    }
    std::cout << "main thread exit" << std::endl;

    return 0;
}