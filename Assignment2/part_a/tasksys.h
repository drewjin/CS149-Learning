#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"

#include <queue>
#include <mutex>
#include <future>
#include <functional>
#include <condition_variable>

template <typename Ty>
class TaskQueue {
    private:
        std::queue<Ty> m_queue;
        std::mutex m_mutex;
    public:
        TaskQueue();
        TaskQueue(TaskQueue &&another);
        ~TaskQueue();

        bool empty();
        size_t size();
        void push(Ty & item);
        bool pop(Ty & item);
};


class ThreadPool {
    private:
        class ThreadWorker;
        bool m_shutdown; // 线程池是否关闭
        TaskQueue<std::function<void()>> m_queue; // 执行函数安全队列，即任务队列
        std::vector<std::thread> m_threads; // 工作线程队列
        std::mutex m_conditional_mutex; // 线程休眠锁互斥变量
        std::condition_variable m_conditional_lock; // 线程环境锁，可以让线程处于休眠或者唤醒状态
    public:
        ThreadPool(const size_t num_threads);
        ThreadPool(const ThreadPool &) = delete;
        ThreadPool(ThreadPool &&) = delete;
        ThreadPool & operator=(const ThreadPool &) = delete;
        ThreadPool & operator=(ThreadPool &&) = delete;

        void init();
        void shutdown();
        template <typename F, typename... Args>
        auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))>;
};

class ThreadPool::ThreadWorker {
    private:
        int m_id;
        ThreadPool *m_pool;
    public:
        ThreadWorker(ThreadPool *pool, const int id);
        void operator()();
};

class ThreadPoolWithoutSleep {
    private:
        std::vector<std::thread> m_workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queue_mutex;
        std::atomic<bool> m_shutdown;
        void worker_loop(int id);
        bool pop_task_from_queue(std::function<void()>& task);
    public:
        ThreadPoolWithoutSleep(int num_threads);
        ~ThreadPoolWithoutSleep();
        template <typename F, typename... Args>
        auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;
        void shut_down();
};

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial: public ITaskSystem {
    public:
        TaskSystemSerial(int num_threads);
        ~TaskSystemSerial();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn: public ITaskSystem {
    public:
        TaskSystemParallelSpawn(int num_threads);
        ~TaskSystemParallelSpawn();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
*/
class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
    private:
        ThreadPoolWithoutSleep m_thread_pool;
    public:
        TaskSystemParallelThreadPoolSpinning(int num_threads);
        ~TaskSystemParallelThreadPoolSpinning();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

#endif
