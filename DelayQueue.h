# pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <chrono>
template<typename T>
class DelayQueue
{
    
    public:
        explicit DelayQueue():hasLeader_(false){}
        DelayQueue(const DelayQueue&) = delete;
        DelayQueue& operator=(const DelayQueue&) = delete;
        ~DelayQueue() = default;
        void put(const T &value);
        bool offer(const T &value);
        const T& peek();
        std::shared_ptr<T> poll();
        std::shared_ptr<T> take();
        int size();
    private:
        std::priority_queue<T> queue_;
        mutable std::mutex mutex_;

    /**
     * Condition signalled when a newer element becomes available
     * at the head of the queue or a new thread may need to
     * become leader.
     */
    std::condition_variable available_;

     /**
     * Thread designated to wait for the element at the head of
     * the queue.  This variant of the Leader-Follower pattern
     * (http://www.cs.wustl.edu/~schmidt/POSA/POSA2/) serves to
     * minimize unnecessary timed waiting.  When a thread becomes
     * the leader, it waits only for the next delay to elapse, but
     * other threads await indefinitely.  The leader thread must
     * signal some other thread before returning from take() or
     * poll(...), unless some other thread becomes leader in the
     * interim.  Whenever the head of the queue is replaced with
     * an element with an earlier expiration time, the leader
     * field is invalidated by being reset to null, and some
     * waiting thread, but not necessarily the current leader, is
     * signalled.  So waiting threads must be prepared to acquire
     * and lose leadership while waiting.
     */
    std::thread::id leader_;
    
    bool hasLeader_;
    
};

/*
 * Inserts the specified element into this delay queue. As the queue is
 * unbounded this method will never block.
 */

template<typename T>
void  DelayQueue<T>::put(const T &value)
{
    offer(value);
}

/*
 * Inserts the specified element into this delay queue. As the queue is
 * unbounded this method will never block.
 */

template<typename T>
bool DelayQueue<T>::offer(const T &value)
{

    std::lock_guard<std::mutex> lock(mutex_);
    bool  resetLeader_ = queue_.size() == 0 ||  value < queue_.top()
                        ? true
                        : false;
    queue_.push(value);
    /* Whenever the head of the queue is replaced with
     * an element with an earlier expiration time, the leader
     * field is invalidated by being reset to null, and some
     * waiting thread, but not necessarily the current leader, is
     * signalled.
     */
    if(true)
    {
        hasLeader_ = false;
        available_.notify_one();
    }
    return true;
}


/*
 * Retrieves and removes the head of this queue, or returns {@code null}
 * if this queue has no elements with an expired delay.
 */

template<typename T>
std::shared_ptr<T> DelayQueue<T>::poll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(queue_.size() == 0 || (queue_.top()).getDelay() > std::chrono::steady_clock::now())
        return std::shared_ptr<T>();
    
    std::shared_ptr<T> const res(std::make_shared<T>(std::move(queue_.top())));
    queue_.pop();
    return res;
    
}


/**
 * Retrieves and removes the head of this queue, waiting if necessary
 * until an element with an expired delay is available on this queue.
 */
template<typename T>
std::shared_ptr<T> DelayQueue<T>::take()
{
    std::unique_lock<std::mutex> lock(mutex_);
    for(;;)
    {
        if(queue_.size() == 0)
            available_.wait(lock);
        else
        {
            std::chrono::steady_clock::time_point timeout =  (queue_.top()).getDelay(); //FIXME
            if(timeout <= std::chrono::steady_clock::now())
                break;
            if(hasLeader_)
                available_.wait(lock);
            else
            {
                std::thread::id thisThread =  std::this_thread::get_id();
                leader_ =  thisThread;
                //while(available_.wait_until(lock, timeout) != std::cv_status::timeout);
                available_.wait_until(lock, timeout);
                if(leader_ == thisThread)
                    hasLeader_ = false;
            }

        }
    }
    std::shared_ptr<T> const res(std::make_shared<T>(std::move(queue_.top())));
    queue_.pop(); 

    if(!hasLeader_ && queue_.size() > 0)
        available_.notify_one();

    return res;
}

/* Retrieves, but does not remove, the head of this queue*/
template<typename T>
const T& DelayQueue<T>::peek()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.top();
}

template<typename T>
int  DelayQueue<T>::size()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}
