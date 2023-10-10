#ifndef __SEMAPHORE_HPP
#define __SEMAPHORE_HPP

#include <mutex>
#include <condition_variable>

class Semaphore
{
public:
    Semaphore(int value = 0) :count(value){}

    void Wait()
    {
        std::unique_lock<std::mutex> lck(mtk);
        if(--count < 0)
        {
            cv.wait(lck);
        }
    }

    void Signal()
    {
        std::unique_lock<std::mutex> lck(mtk);
        if(++count <= 0)
        {
            cv.notify_one();
        }
    }
private:
    int count;
    std::mutex mtk;
    std::condition_variable cv;
};

#endif //__SEMAPHORE_HPP