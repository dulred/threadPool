#include <iostream>
#include <chrono>
#include <thread>
#include <chrono>
#include "threadPool/threadpool.h"

using uLong  = unsigned long long;

class MyTask : public Task
{
public:
    MyTask(int begin, int end)
        : begin_(begin)
        , end_(end)
    {}
    Any run()
    {
        std::cout << "tid:" << std::this_thread::get_id()
            << "begin" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        uLong sum = 0;
        for (uLong i = begin_; i < end_; i++)
        {
            sum += i;
        }
        
        std::cout << "tid:" << std::this_thread::get_id()
            << "end" << std::endl;
        return sum;
    }
private:
    int begin_;
    int end_;
};

int main(int argc, char const *argv[])
{
    {
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_CACHED);
        pool.start(1);
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        uLong sum1 = res1.get().cast_<uLong>();
        std::cout << sum1 << std::endl;


    }

    std::cout << "main over" << std::endl;

#if 0
    {
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_CACHED);
        pool.start(4);

        // auto start = std::chrono::steady_clock::now();

        Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        // Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        // Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        // Result res4 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        // Result res5 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        // Result res6 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        uLong sum1 = res1.get().cast_<uLong>();
        // uLong sum2 = res2.get().cast_<uLong>();
        // uLong sum3 = res3.get().cast_<uLong>();
    }


    // // Master - Slave 线程模型
    // auto end = std::chrono::steady_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    // std::cout << "Execution time1 :" << duration.count() << "microseconds" << std::endl;

    // std::cout << (sum1 + sum2 + sum3) << std::endl;

    // uLong sum = 0;

    // auto start2 = std::chrono::steady_clock::now();

    // for (uLong i = 1; i < 300000000; i++)
    // {
    //     sum += i;
    // }
    // auto end2 = std::chrono::steady_clock::now();
    // auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    // std::cout << "Execution time2 :" << duration2.count() << "microseconds" << std::endl;
    // std::cout << sum << std::endl;
    
#endif

    getchar();

    return 0;
}
