#include <mutex>
#include <condition_variable>
class CountDownLatch
{
    private:
        int count_;
        mutable std::mutex mutex_; 
        std::condition_variable cond_;
    public:
        explicit  CountDownLatch(int count);
        void await();
        void countDown();
        int getCount() const;
};
CountDownLatch::CountDownLatch(int count):
    count_(count)
{

}

void CountDownLatch::await()
{
    std::unique_lock<std::mutex> lk(mutex_);
    cond_.wait(lk, [this]{ return count_ == 0; });
}

void CountDownLatch::countDown()
{
    std::unique_lock<std::mutex> lk(mutex_);
    --count_;
    if(count_ == 0)
        cond_.notify_all();
}

int CountDownLatch::getCount() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return count_;
}