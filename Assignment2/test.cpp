#include <iostream>
#include <thread>
#include <mutex>

std::mutex mtx; // 定义一个互斥锁

void print_block(int n, char c) {
    // 使用lock_guard来自动管理互斥锁的生命周期
    std::lock_guard<std::mutex> guard(mtx);   //获取锁
    for (int i = 0; i < n; ++i) { std::cout << c; }
    std::cout << '\n';
} // guard 对象在这里被销毁，互斥锁会被自动释放

int main() {
    std::thread t1(print_block, 50, '*');
    std::thread t2(print_block, 50, '$');

    t1.join();
    t2.join();

    return 0;
}