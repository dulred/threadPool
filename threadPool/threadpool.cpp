#include "threadpool.h"

#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 10; // 单位: 秒

// 线程池构造
ThreadPool::ThreadPool()
    : initThreadSize_(0)
    , curThreadSize_(0)
    , idleThreadSize_(0)
    , threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
    , taskSize_(0)
    , taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
    , poolMode_(PoolMode::MODE_FIXED)
    , isPoolRunning_(false)
{}

// 线程池析构
ThreadPool::~ThreadPool()
{
    isPoolRunning_ = false;

    // 等待线程池里面所有的线程返回 有三种状态: 阻塞 & 正在执行任务中 & 阻塞状态抢锁中
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lock, [&](){return threads_.size() == 0;});
}

//设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode)
{
    if (checkRunningState())
        return;
    
    poolMode_ = mode;
}

// 设置task任务队列上线阈值
void ThreadPool::setTaskQueMaxThresHold(int threshhold)
{
    if (checkRunningState())
        return;
    taskQueMaxThreshHold_ = threshhold;
}


// 设置task任务队列上线阈值
void ThreadPool::setThreadSizeThresHold(int threshhold){
    if (checkRunningState())
        return;
    if (poolMode_ == PoolMode::MODE_CACHED)
    {
        threadSizeThreshHold_ = threshhold;
    }
}

// 给线程池提交任务  用户调用该接口, 传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    // 获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);

    // 线程的通信 等待任务队列有空余
    // 用户提交任务, 最长不能阻塞超过1s, 否则判断提交任务失败，返回
    if (!notFull_.wait_for(lock, std::chrono::seconds(1),
        [&]()->bool { return taskQue_.size() < taskQueMaxThreshHold_;}))
    {
        // 表示notFull_等待1s钟， 条件依然没有满足
        std::cerr << "task queue is full, submit task fail. " << std::endl;
        return Result(sp, false);
    }

    // 如果有空余，把任务放入任务队列中
    taskQue_.emplace(sp);
    taskSize_++;

    // 因为新放了任务， 任务队列肯定不空了，在notEmpty_上进行通知
    notEmpty_.notify_all();
    
    // cached模式 任务处理比较紧急 场景: 小而快的任务 （如果耗时还是fixed模式慢慢处理）  需要根据任务数量和空闲线程的数量，判断是否需要创建新线程

    if (poolMode_ == PoolMode::MODE_CACHED
        && taskSize_ > idleThreadSize_
        && curThreadSize_ < threadSizeThreshHold_)
    {
        std::cout << ">>>>> create new thread..." << std::endl;
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
        // 启动线程
        threads_[threadId]->start();
        // 修改线程个数相关的变量
        curThreadSize_++;
        idleThreadSize_++;
    }
    
    // 返回任务的result对象
    return Result(sp);
    // return task

}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
    // 设置线程池的运行状态
    isPoolRunning_ = true;

    // 记录初始线程个数
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;

    // 创建线程对象
    for (int i = 0; i < initThreadSize_; i++)
    {
        // 创建thread线程对象的时候，把线程函数给到thread线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
    }

    // 启动所有线程 std::vector<Thread*> threads_;
    for (int i = 0; i < initThreadSize_; i++)
    {
        threads_[i]->start(); // 需要去执行一个线程函数
        idleThreadSize_++;
    }
  
}

// 定义线程函数  线程池的所有线程从任务队列里面消费任务
void ThreadPool::threadFunc(int threadid)
{
    auto lastTime = std::chrono::high_resolution_clock().now();

    for(;;)
    {
        std::shared_ptr<Task> task;
        {
            // 先获取锁
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            std::cout << "tid:" << std::this_thread::get_id()
                << "尝试获取任务..." << std::endl;

            // cached 模式下， 有可能已经创建了很多的线程， 但是空闲时间超过60s，应该把多余的线程
            // 结束回收掉 (超过initThreadSize_数量的线程要进行回收)
            // 当前时间 - 上一次线程执行的时间 > 60s
           
                // 每一秒中返回一次  怎么区分: 超时返回？ 还是有任务待执行返回
                while (taskQue_.size() == 0)
                { 
                    // 线程池要结束，
                    if (!isPoolRunning_)
                    {
                        threads_.erase(threadid);
                        std::cout << "threadid" << std::this_thread::get_id() << "delete" << std::endl;
                        exitCond_.notify_all();
                        return;
                    }

                    // 超时回收线程资源
                    if (poolMode_ == PoolMode::MODE_CACHED)
                    {
                        // 条件变量， 超时返回了
                        if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
                        {
                            auto now = std::chrono::high_resolution_clock().now();
                            auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                            if (dur.count() >= THREAD_MAX_IDLE_TIME
                                && curThreadSize_ > initThreadSize_)
                            {
                                // 开始回收当前线程
                                // 记录线程数量相关变量的值修改
                                // 把线程对象从线程列表容器中删除
                                threads_.erase(threadid);
                                curThreadSize_--;
                                idleThreadSize_--;
                                std::cout << "threadid" << std::this_thread::get_id() << "exit" << std::endl;
                                return;
                            }
                            
                        }
                    
                    }
                    else
                    {
                        // 等待notEmpty条件
                        notEmpty_.wait(lock);
                    }

                    // // 线程池
                    // if (!isPoolRunning_)
                    // {
                    //     threads_.erase(threadid);
                    //     std::cout << "threadid" << std::this_thread::get_id() << "delete" << std::endl;
                    //     exitCond_.notify_all();
                    //     return;
                    // }
                    
                }
           

            idleThreadSize_--;

            std::cout << "tid:" << std::this_thread::get_id()
                << "获取任务成功..." << std::endl;


            // 从任务队列中取出一个任务出来
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;

            // 如果依然有剩余任务， 继续通知其他的线程执行任务
            if (taskQue_.size() > 0)
            {
                notEmpty_.notify_all();
            }

            // 取出一个任务，进行通知生产任务线程
            notFull_.notify_all();
            
        } // 锁的颗粒度缩小

        // 当前线程负责执行这个任务
        if (task != nullptr)
        {
            // task->run(); // 执行任务; 把任务的返回值setVal方法给到Result
            task->exec();
        }

        idleThreadSize_++;

        lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
        

    }

}


bool ThreadPool::checkRunningState() const
{
    return isPoolRunning_;
}

//------------- 线程方法实现 ---------------//

int Thread::generateId_ = 0;

// 线程构造
Thread::Thread(ThreadFunc func)
    : func_(func)
    , threadId_(generateId_++)
{}

// 线程析构
Thread::~Thread()
{}

int Thread::getId() const
{
    return threadId_;
}
// 启动线程
void Thread::start()
{
    // 创建一个线程来执行一个线程函数
    std::thread t(func_, threadId_);
    t.detach();
}

//------------- Task 方法实现 ---------------//
Task::Task()
    : result_(nullptr)
{}

void Task::exec()
{
    if (result_ != nullptr)
    {
        result_->setVal(run()); // 这里发生多态调用
    }
    
}
void Task::setResult(Result* res)
{
    result_ = res;
}

//------------- Result 方法实现 ---------------//
Result::Result(std::shared_ptr<Task> task, bool isValid)
    : isValid_(isValid)
    , task_(task)
{
    task_->setResult(this);
}

// 用户调用这个方法获取task的返回值
Any Result::get()
{
    if (!isValid_)
    {
        return "";
    }
    sem_.wait(); // task任务如果没有执行完， 这里会阻塞用户的线程
    return std::move(any_);
    
}

// 获取任务执行完的返回值
void Result::setVal(Any any)
{
    // 存储task的返回值
    this->any_ = std::move(any);
    sem_.post(); //已经获取的任务的返回值，增加信号量资源
}

