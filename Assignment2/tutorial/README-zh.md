# C++ 同步原语概述 #

在你的编程作业2中，解决方案肯定需要创建线程，并且可能需要使用两种类型的同步原语：互斥量（mutex）和条件变量（condition variables）。以下是这两种同步原语的解释。

我们在提供的起始代码文件 `tutorial/tutorial.cpp` 中提供了创建 C++ 线程、锁定/解锁互斥量和使用条件变量的基本示例。

## 创建 C++ 线程 ##

在 C++ 中创建新线程非常简单。要创建线程，应用程序通过构造 `std::thread` 对象的新实例。例如，在以下代码中，主线程创建了两个运行 `my_func` 函数的线程。（请注意，`my_func` 函数作为参数传递给 `std::thread` 构造函数。）主线程通过调用 `join()` 来确定何时完成所生成线程的执行。

```cpp
#include <thread>
#include <stdio.h>

void my_func(int thread_id, int num_threads) {
    printf("Hello from spawned thread %d of %d\n", thread_id, num_threads);
}

int main(int argc, char** argv) {

  std::thread t0 = std::thread(my_func, 0, 2);
  std::thread t1 = std::thread(my_func, 1, 2);

  printf("The main thread is running concurrently with spawned threads.\n");

  t0.join();
  t1.join();

  printf("Spawned threads have terminated at this point.\n");

  return 0;
}
```

有关 `std::thread` 的完整文档，请访问：<https://en.cppreference.com/w/cpp/thread/thread>。

有关 C++ 11 中创建线程的有用教程：

* <https://www.geeksforgeeks.org/multithreading-in-cpp/>
* <https://thispointer.com/c-11-multithreading-part-1-three-different-ways-to-create-threads/>

## 互斥量（Mutex） ##

C++ 标准库提供了一个互斥量同步原语 `std::mutex`，用于保护共享数据，以防止多个应用程序线程同时访问同一数据。（注意：mutex 是 "mutual exclusion"（互斥）的缩写。）

<https://en.cppreference.com/w/cpp/thread/mutex>

你在之前的课程中可能已经遇到过互斥量。在 C++ 中，一个线程使用 `mutex::lock()` 来锁定互斥量。调用线程将阻塞，直到能够获得锁。当 `lock()` 返回时，调用线程保证已经获得锁。线程使用 `mutex::unlock()` 来解锁互斥量。

对于那些有兴趣的人，C++ 提供了许多封装类，用来减少在使用锁时出现的 bug（例如忘记解锁互斥量）。你可能希望查看 [`std::unique_lock`](https://en.cppreference.com/w/cpp/thread/unique_lock) 和 [`std::lock_guard`](https://en.cppreference.com/w/cpp/thread/lock_guard) 的定义。例如，`lock_guard` 会在构造时自动锁定指定的互斥量，并在超出作用域时自动解锁该互斥量。

我们建议你查看 `tutorial/tutorial.cpp` 中的 `mutex_example()` 函数，它展示了如何使用互斥量来保护对共享计数器的更新。在此示例中，互斥量确保计数器的读-修改-写操作是原子的。

## 条件变量（Condition Variables） ##

条件变量用于管理等待某个条件成立的线程列表，并允许其他线程通知等待的线程该事件已发生。条件变量在与互斥量一起使用时，提供了一种简便的方式来在线程间发送通知。

条件变量有两个主要操作：`wait()` 和 `notify()`。

线程调用 `wait(lock)` 来表示它希望等待直到收到其他线程的通知。请注意，`wait()` 调用需要传递一个互斥量（通常使用 `std::unique_lock` 封装）。当线程被通知时，条件变量将获得锁。这意味着当 `wait()` 返回时，调用线程是当前的锁持有者。通常，锁会用来保护共享变量，线程此时需要检查这些变量，以确保其等待的条件为真。

例如，`tutorial/tutorial.cpp` 中的代码创建了 N 个线程。N-1 个线程等待来自线程 0 的通知，然后在被通知时原子地递增由共享互斥量保护的计数器。

线程可以调用 `notify()` 来通知一个等待条件变量的线程，或者调用 `notify_all()` 来通知所有等待的线程。请注意，在 `tutorial/tutorial.cpp` 中，线程 0 在通知所有等待线程之前，释放了保护计数器的锁。

在你的任务执行系统实现中，如何使用 `notify_all()`？可以考虑一种情况：所有工作线程当前都在等待新的批量任务启动，而应用程序调用 `run()` 来提供新的任务执行。

额外参考资料：

* <https://thispointer.com/c11-multithreading-part-7-condition-variables-explained>
* <https://www.modernescpp.com/index.php/condition-variables>

## C++ 原子操作（Atomics） ##

C++ 还提供了一种简单的方法来使变量上的操作具有原子性——只需创建一个 `std::atomic<T>` 类型的变量。例如，要创建一个支持原子递增的整数，只需创建一个如下的变量：

```cpp
std::atomic<int> my_counter;
```

现在，`my_counter` 上的操作（如 `my_counter++`）将被保证是原子的。有关更多细节，请参阅：<https://en.cppreference.com/w/cpp/atomic/atomic>。