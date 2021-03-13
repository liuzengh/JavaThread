# pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>


/**
 * A bounded {@linkplain BlockingQueue blocking queue} backed by an
 * array.  This queue orders elements FIFO (first-in-first-out).  The
 * <em>head</em> of the queue is that element that has been on the
 * queue the longest time.  The <em>tail</em> of the queue is that
 * element that has been on the queue the shortest time. New elements
 * are inserted at the tail of the queue, and the queue retrieval
 * operations obtain elements at the head of the queue.
 *
 * <p>This is a classic &quot;bounded buffer&quot;, in which a
 * fixed-sized array holds elements inserted by producers and
 * extracted by consumers.  Once created, the capacity cannot be
 * changed.  Attempts to {@code put} an element into a full queue
 * will result in the operation blocking; attempts to {@code take} an
 * element from an empty queue will similarly block.
 */


template<typename T>
class ArrayBlockingQueue
{
    public:
        explicit ArrayBlockingQueue(int capacity);
        ~ArrayBlockingQueue() = default;
        ArrayBlockingQueue(const ArrayBlockingQueue& other) = delete;
        ArrayBlockingQueue& operator=(const ArrayBlockingQueue& other) = delete;
        std::shared_ptr<T> take();
        std::shared_ptr<T> poll();
        void put(const T &value);
        bool offer(const T &value);
        bool empty() const;
        bool full() const;
        int size() const;
        int capacity() const;

    private:
        void enqueue(const T &value);
        std::shared_ptr<T> dequeue();

    private:
    
        /** Capacity of the queue */
        const int capacity_;

        /** Number of elements in the queue */
        int count_; 

        /** items index for next take*/
        int takeIndex_;
        
        /** items index for next put, offer*/
        int putIndex_;

        /** The queued items */
        std::vector<T> items_;
        
        /** Main lock guarding all access */
        mutable std::mutex mutex_;

        /** Condition for waiting takes */
        std::condition_variable notEmpty_;

        /** Condition for waiting puts */
        std::condition_variable notFull_;


};
template<typename T>
ArrayBlockingQueue<T>::ArrayBlockingQueue(int capacity):
    capacity_(capacity), 
    count_(0),
    takeIndex_(0),
    putIndex_(0)
{
    items_.resize(capacity_);
}


/* Inserts the specified element into this queue, 
 * waiting if necessary for space to become available. 
 */
template<typename T>
void ArrayBlockingQueue<T>::put(const T &value)
{
    std::unique_lock<std::mutex> lk(mutex_);
    notFull_.wait(lk, [this]{ return count_ < capacity_; });
    enqueue(value);
}


/* Inserts the specified element into this queue if it is possible to do 
 * so immediately without violating capacity restrictions, 
 * returning true upon success and false if no space is currently available. 
 */

template<typename T>
bool ArrayBlockingQueue<T>::offer(const T &value)
{ 
    std::lock_guard<std::mutex> lk(mutex_);
    if(count_ = capacity_)
        return false;
    else
    {   
        enqueue(value);
        return true;
    }
}

/* Retrieves and removes the head of this queue, 
 * waiting if necessary until an element becomes available. 
 */
template<typename T>
std::shared_ptr<T> ArrayBlockingQueue<T>::take()
{
    std::unique_lock<std::mutex> lk(mutex_);
    notEmpty_.wait(lk, [this]{ return count_ > 0; });
    return dequeue();
}

/* Retrieves and removes the head of this queue,  
 * if it is possible to do  so immediately without violating capacity restrictions,  
 *  returning the element upon  success and nullpter if the queue is empty.
 */
template<typename T>
std::shared_ptr<T> ArrayBlockingQueue<T>::poll()
{
    std::lock_guard<std::mutex> lk(mutex_);
    if(count_ == 0)
        return std::shared_ptr<T>();
    return dequeue();
};


 /**
   * Inserts element at current put position, advances, and signals.
   * Call only when holding lock.
   */
template<typename T>
void ArrayBlockingQueue<T>::enqueue(const T &value)
{
    items_[putIndex_] = value; 
    if(++putIndex_ == capacity_)
        putIndex_ = 0;
    count_++;
    notEmpty_.notify_one();
}


/**
 * Extracts element at current take position, advances, and signals.
 * Call only when holding lock.
 */
template<typename T>
std::shared_ptr<T> ArrayBlockingQueue<T>::dequeue()
{
    std::shared_ptr<T> const res(
        std::make_shared<T>(std::move(items_[takeIndex_])));
    if(++takeIndex_ == capacity_)
        takeIndex_ = 0;
    count_--;
    notFull_.notify_one();
    return res;
}

template<typename T>
bool ArrayBlockingQueue<T>::empty() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return count_ == 0;
}

template<typename T>
bool ArrayBlockingQueue<T>::full() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return count_ == capacity_;
}

template<typename T>
int ArrayBlockingQueue<T>::size() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return count_;
}

template<typename T>
int ArrayBlockingQueue<T>::capacity() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return capacity_;
}