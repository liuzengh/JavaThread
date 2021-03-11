#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>
template<typename T>
class ArrayBlockingQueue
{
    public:
        explicit ArrayBlockingQueue(int capacity);
        ~ArrayBlockingQueue();
        ArrayBlockingQueue(const ArrayBlockingQueue& other);
        ArrayBlockingQueue& operator=(const ArrayBlockingQueue& other);
        std::shared_ptr<T> take();
        std::shared_ptr<T> poll();
        void put(T new_value);
        bool offer(T new_value);
        bool empty() const;
        bool full() const;
        int size() const;
        int capacity() const;

    private:
        void enqueue(T &new_value);
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
        vector<T> items_;
        
        /** Main lock guarding all access */
        mutable std::mutex mutex_;

        /** Condition for waiting takes */
        std::condition_variable notEmpty;

        /** Condition for waiting puts */
        std::condition_variable notFull;



};
template<typename T>
ArrayBlockingQueue<T>::ArrayBlockingQueue(int capacity):
    capacity_(capacity), 
    count_(0),
    takeIndex_(0),
    putIndex_(0)
{
    items_.reserve(capacity_);
}


/* Inserts the specified element into this queue, 
 * waiting if necessary for space to become available. 
 */
template<typename T>
void ArrayBlockingQueue<T>::put(T new_value)
{
    std::unique_lock<std::mutex> lk(mutex_);
    notFull.wait(lk, [this]{ return count_ < capacity_; });
    enqueue(new_value);
}


/* Inserts the specified element into this queue if it is possible to do 
 * so immediately without violating capacity restrictions, 
 * returning true upon success and false if no space is currently available. 
 */

template<typename T>
bool ArrayBlockingQueue<T>::offer(T new_value)
{ // FIXME:
    std::lock_guard<std::mutex> lk(mutex_);
    if(count_ = capacity_)
        return false;
    else
    {   
        enqueue(new_value);
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
    notEmpty.wait(lk, [this]{ return count_ > 0; });
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
    if(count == 0)
        return nullptr;
    return dequeue();
};


 /**
   * Inserts element at current put position, advances, and signals.
   * Call only when holding lock.
   */
template<typename T>
void ArrayBlockingQueue<T>::enqueue(T &value)
{
    items_[putIndex_] = std::move(new_value);
    if(++putIndex_ == capacity_)
        putIndex_ = 0;
    count++;
    notEmpty.notify_one();
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
    count--;
    notFull.notify_one();
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