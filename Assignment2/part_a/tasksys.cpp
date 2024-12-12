#include "tasksys.h"

/* [TaskQueue]
    Task Queue in the thread pool.
*/
template <typename Ty>
TaskQueue<Ty>::~TaskQueue() { }

template <typename Ty>
TaskQueue<Ty>::TaskQueue(TaskQueue &&another) { }

template <typename Ty>
TaskQueue<Ty>::TaskQueue() {}

template <typename Ty>
bool TaskQueue<Ty>::empty() { 
    std::unique_lock<std::mutex> lock(this->m_mutex);
    return this->m_queue.empty(); 
}

template <typename Ty>
size_t TaskQueue<Ty>::size() { 
    std::unique_lock<std::mutex> lock(this->mutex);
    return this->tasks.size(); 
}

template <typename Ty>
void TaskQueue<Ty>::push(Ty & item) {
    std::unique_lock<std::mutex> lock(this->mutex);
    this->m_queue.emplace(item);
}

template <typename Ty>
bool TaskQueue<Ty>::pop(Ty & item) {
    std::unique_lock<std::mutex> lock(this->m_mutex);
    if (this->m_queue.empty())
        return false;
    item = std::move(this->m_queue.front());
    this->m_queue.pop();
    return true;
}


/*[ThreadPool]
*/
ThreadPool::ThreadPool(const size_t num_threads = 4)
: m_threads(std::vector<std::thread>(num_threads)), m_shutdown(false) 
{ }

void ThreadPool::init() {
    for (size_t i = 0; i < this->m_threads.size(); ++i)
        // 分配线程
        this->m_threads.at(i) = std::thread(ThreadWorker(this, i));
}

void ThreadPool::shutdown() {
    this->m_shutdown = true;
    this->m_conditional_lock.notify_all(); // 通知，唤醒所有工作线程
    // for (int i = 0; i < m_threads.size(); ++i)
    //     if (this->m_threads.at(i).joinable()) 
    //         // 判断线程是否在等待
    //         this->m_threads.at(i).join(); // 将线程加入到等待队列
    for (auto &thread : this->m_threads)
        if (thread.joinable())
            // 判断线程是否在等待
            thread.join(); // 将线程加入到等待队列
}

// 将函数提交给线程池以异步执行
template <typename F, typename... Args>
auto ThreadPool::submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
    // 创建一个具有绑定参数的函数，准备执行。
    std::function<decltype(f(args...))()> func = std::bind(
        std::forward<F>(f), std::forward<Args>(args)...
    ); // 连接函数和参数定义，特殊函数类型，避免左右值错误

    // 将函数封装进一个共享指针中，以便能够进行复制构造。
    auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

    // 将包装好的任务包装成返回void的函数。
    std::function<void()> warpper_func = [task_ptr]() { (*task_ptr)(); };
    
    m_queue.push(warpper_func); // 队列通用安全封包函数，并压入安全队列
    m_conditional_lock.notify_one(); // 唤醒一个等待中的线程
    return task_ptr->get_future(); // 返回先前注册的任务指针 
}


/*[ThreadWorker]
*/
ThreadPool::ThreadWorker::ThreadWorker(ThreadPool *pool, const int id) 
: m_pool(pool), m_id(id) { }

void ThreadPool::ThreadWorker::operator()() {
    std::function<void()> fn; // 定义基础函数类fn
    bool is_dequeue; // 判断是否在取队列中的任务
    while (!this->m_pool->m_shutdown) {
        {   
            // 为线程环境加锁，互访问工作线程的休眠和唤醒
            std::unique_lock<std::mutex> lock( 
                this->m_pool->m_conditional_mutex
            );
            // 如果任务队列为空，阻塞当前线程
            if (this->m_pool->m_queue.empty()) 
                // 等待条件变量通知，开启线程
                this->m_pool->m_conditional_lock.wait(lock);
            // 取出任务队列中的元素
            is_dequeue = this->m_pool->m_queue.pop(fn);
        }
        // 如果成功取出，执行工作函数
        if (is_dequeue)
            fn();
    }
}

/*ThreadPoolWithoutSleep
*/
ThreadPoolWithoutSleep::ThreadPoolWithoutSleep(int num_threads)
: m_shutdown(false) {
    num_threads = std::min(num_threads, MAX_THREAD_NUM);
    for (size_t i = 0; i < num_threads; ++i) 
        m_workers.emplace_back([this, i] { this->worker_loop(i); });
}

ThreadPoolWithoutSleep::~ThreadPoolWithoutSleep() { shut_down(); }

void ThreadPoolWithoutSleep::worker_loop(int id) {
    while (!m_shutdown.load()) {
        std::function<void()> task;
        if (pop_task_from_queue(task))
            task();
    }
}

bool ThreadPoolWithoutSleep::pop_task_from_queue(std::function<void()>& task) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    if (!tasks.empty()) {
        task = std::move(tasks.front());
        tasks.pop();
        return true;
    }
    return false;
}

template <typename F, typename... Args>
auto ThreadPoolWithoutSleep::submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    using return_type = decltype(f(args...));
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        tasks.push([task]() { (*task)(); });
    }
    return task->get_future();
}

void ThreadPoolWithoutSleep::shut_down() {
    if (!m_shutdown.load()) {
        m_shutdown.store(true);
        for (auto& worker : m_workers) 
            if (worker.joinable()) 
                worker.join();
    }
}


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {
    this->num_threads = std::min(num_threads, MAX_THREAD_NUM);
    this->threads.reserve(num_threads);
}

ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    // nothing to do, changed the base class
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    int num_threads = this->num_threads;
    int tasks_per_thread = (num_total_tasks + num_threads - 1) / num_threads;
    for (int i = 0; i < tasks_per_thread; i++) {
        int start = i * tasks_per_thread;
        int end = std::min((i + 1) * tasks_per_thread, num_total_tasks);
        this->threads.emplace_back(
            std::thread([runnable, start, end, num_total_tasks] () {
                for (int j = start; j < end; j++)
                    runnable->runTask(j, num_total_tasks); 
            })
        );
    }
    for (int i = 0; i < tasks_per_thread; i++)
        this->threads[i].join();
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads)
: ITaskSystem(num_threads), m_thread_pool(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    std::vector<std::future<void>> futures;
    for (int i = 0; i < num_total_tasks; ++i) 
        futures.emplace_back(m_thread_pool.submit([runnable, i, num_total_tasks]() {
            runnable->runTask(i, num_total_tasks);
        }));

    // 等待所有任务完成
    for (auto& future : futures) 
        future.get(); // 可能会抛出异常
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
