#include "threadpool.h"

#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 4;

// 线程池构造
ThreadPool::ThreadPool()
    : initThreadSize_(0)
    , taskSize_(0)
    , taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
    , poolMode_(PoolMode::MODE_FIXED)
{}

// 线程池析构
ThreadPool::~ThreadPool()
{}

//设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode)
{
    poolMode_ = mode;
}

// 设置task任务队列上线阈值
void ThreadPool::setTaskQueMaxThresHold(int threshhold)
{
    taskQueMaxThreshHold_ = threshhold;
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
    return Result(sp);

}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
    // 记录初始线程个数
    initThreadSize_ = initThreadSize;

    // 创建线程对象
    for (int i = 0; i < initThreadSize_; i++)
    {
        // 创建thread线程对象的时候，把线程函数给到thread线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
        threads_.emplace_back(std::move(ptr));
    }

    // 启动所有线程 std::vector<Thread*> threads_;
    for (int i = 0; i < initThreadSize_; i++)
    {
        threads_[i]->start(); // 需要去执行一个线程函数
    }
  
}

// 定义线程函数  线程池的所有线程从任务队列里面消费任务
void ThreadPool::threadFunc()
{
    // std::cout << "begin threadFunc tid:" << std::this_thread::get_id()
    //     << std::endl;
    // std::cout << "end threadFunc tid:" << std::this_thread::get_id()
    //     << std::endl;
    for(;;)
    {
        std::shared_ptr<Task> task;
        {
            // 先获取锁
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            std::cout << "tid:" << std::this_thread::get_id()
                << "尝试获取任务..." << std::endl;

            // 等待notEmpty条件
            notEmpty_.wait(lock, [&]()->bool { return taskQue_.size() > 0;});

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
        

    }


}

//------------- 线程方法实现 ---------------//
// 线程构造
Thread::Thread(ThreadFunc func)
    : func_(func)
{}

// 线程析构
Thread::~Thread()
{}


// 启动线程
void Thread::start()
{
    // 创建一个线程来执行一个线程函数
    std::thread t(func_);
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

