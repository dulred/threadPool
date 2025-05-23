#include <iostream>
#include <chrono>
#include <thread>
#include <chrono>
#include "threadPool2/threadpool.h"
#include <future>

int sum1(int a, int b)
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return a + b;
}

int sum2(int a, int b, int c)
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return a + b + c;
}

int main()
{
    ThreadPool pool;
    // pool.setMode(PoolMode::MODE_CACHED);
    pool.start(2);

    future<int> r1 = pool.submitTask(sum1, 1, 2);
    future<int> r2 = pool.submitTask(sum2, 1, 2, 3);
    future<int> r3 = pool.submitTask([](int a, int b)-> int{
        int sum = 0;
        for (int i = a; i <= b; i++)
        {
            sum += i;
        }
        
        return sum;
    }, 1, 100);
    future<int> r4 = pool.submitTask(sum1, 1, 2);
    future<int> r5 = pool.submitTask([](int a, int b)-> int{
    int sum = 0;
    for (int i = a; i <= b; i++)
    {
        sum += i;
    }
    
    return sum;
    }, 1, 100);
    
    cout << r1.get() << endl;
    cout << r2.get() << endl;
    cout << r3.get() << endl;
    cout << r4.get() << endl;
    cout << r5.get() << endl;

    return 0;
}
