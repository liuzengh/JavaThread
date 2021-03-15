#include <limits>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <atomic>
#include <thread>
template<typename T>
/**
 *  <E> the type of elements held in this queue
 */

class PriorityBlockingQueue
{
    public:
        explicit PriorityBlockingQueue(int initialCapacity = 0);
        PriorityBlockingQueue(const PriorityBlockingQueue&) = delete;
        PriorityBlockingQueue& operator=(const PriorityBlockingQueue&) = delete;
        void put(const T &x);
        bool offer(const T &x);
        std::shared_ptr<T> take();
        std::shared_ptr<T> poll();
        void clear();
        const T& peek();
    private:
        void shifUp(const T &x);
        void shifDown(int hole);
        std::shared_ptr<T> dequeue();
        // inline funtion
       // int parent(const int &index) const { return index >> 1; };
       // int leftChild(const int &index) const { return index << 1; };
       // int rightChild(const int &index) const { return 1 + (index << 1); };
    private:

        /**
        * Default array capacity.
        */
        static const kDefaultInitialCapacity = 11;

         /**
        * The maximum size of array to allocate.
        * Some VMs reserve some header words in an array.
        * Attempts to allocate larger arrays may result in
        * OutOfMemoryError: Requested array size exceeds VM limit
        */
        static const kMaxArraySize  = std::numeric_limits<int>::max() - 8;

        /**
        * The number of elements in the priority queue.
        */
        int size_;

        /**
         * The capacity of the priority queue
         */
        int capacity_;

        /**
        * Lock used for all public operations.
        */
        std::mutex mutex_;
        
        /**
         * Condition for blocking when empty
         */
        std::condition_variable notEmpty_;

        /**
        * Priority queue represented as a balanced binary heap: the two
        * children of queue[n] are queue[2*n+1] and queue[2*(n+1)].  The
        * priority queue is ordered by comparator, or by the elements'
        * natural ordering, if comparator is null: For each node n in the
        * heap and each descendant d of n, n <= d.  The element with the
        * lowest value is in queue[0], assuming the queue is nonempty.
        */
        T *array_;

        /**
        * Spinlock for allocation
        */
        std::atomic_flag allocationSpinLock;
};

template<typename T>
PriorityBlockingQueue<T>::PriorityBlockingQueue(int initialCapacity = 0):
    size_(0),
    capacity_(1 + std::max(initialCapacity, kDefaultInitialCapacity)),
    allocationSpinLock(ATOMIC_FLAG_INIT)
{
    array_ = new T[initialCapacity];
}


/**
 * Inserts the specified element into this priority queue.
 * As the queue is unbounded, this method will never block.
 * */
template<typename T>
void PriorityBlockingQueue<T>::put(const T& x)
{
    offer(x);
}

/**
 * Inserts the specified element into this priority queue.
 * As the queue is unbounded, this method will never return {@code false}.
 */
template<typename T>
bool PriorityBlockingQueue<T>::offer(const T& x)
{
    std::unique_lock<std::mutex> lock(mutex_);
    while(size_  >= capacity_ - 1)
    {
        /**
        * Tries to grow array to accommodate at least one more element
        * (but normally expand by about 50%), giving up (allowing retry)
        * on contention (which we expect to be rare). Call only while
        * holding lock.
        */
        
        lock.unlock();  // must release and then re-acquire main lock
        T *newQueue = nullptr;
        int newCap = 0;

        if(allocationSpinLock.test_and_set(std::memory_order_acquire))
        {
            int oldCap = capacity_;
            int newCap = oldCap + ((oldCap < 64) ?
                                    (oldCap + 2) :  // grow faster if small
                                    (oldCap >> 1));
             //FIXME: possible memory overflow
            T *newQueue = new T[newCap];
            
            allocationSpinLock.clear();
        }

        if(newQueue == nullptr) // back off if another thread is allocating
            std::this_thread::yield();
          
         lock.lock();
        if(newQueue != nullptr)
        {
            for(int k = 0; k < size_; ++k) // TODO: find faster copy method
                newQueue[k] = std::move(array_[k]);
            std::swap(array_, newQueue);
            delete [] newQueue;
            capacity_ = newCap;
        }
    }
    siftUp(x);
    notEmpty_.notify_one();
    lock.unlock();
    return true;

}

/**
 * Inserts item x at position size_+1, maintaining heap invariant by
 * promoting x up the tree until it is greater than or equal to
 * its parent, or is the root
 */
template<typename T>
void PriorityBlockingQueue<T>::shifUp(const T& x)
{
    int hole = ++size_;
    T copy = x; // copy not move;
    array_[0] = std::move(copy); 
    for(; x < array_[hole>>1]; hole >>= 1)
    {
        array_[hole] = std::move(array_[hole >> 1]);
    }
    array_[hole] = std::move(array_[0]);
}

template<typename T>
 std::shared_ptr<T> PriorityBlockingQueue<T>::take()
 {
    std::unique_lock<std::mutex> takeLock;
    std::shared_ptr<T> res;
    notEmpty_.wait(takeLock, [this]{ res = dequeue();
                                     return res != nullptr; })
    return res;
 }

template<typename T>
std::shared_ptr<T> PriorityBlockingQueue<T>::poll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return dequeue();
}

template<typename T>
std::shared_ptr<T> PriorityBlockingQueue<T>::dequeue()
{
    if(size_ == 0)
        return std::shared_ptr<T>();
    else
    {
        std::shared_ptr<T> const res(std::make_shared<T>(std::move(array_[1])));
        array_[1] = std::move(array_[size_--]);
        shifDown(1);
        return res;
    }
}

/**
 * shift item array[hole] at position hole down, maintaining heap invariant by
 * demoting array[hole] down the tree repeatedly until it is less than or
 *  equal to its children or is a leaf.
 */

template<typename T>
void  PriorityBlockingQueue<T>::shifDown(int hole)
{   
    T tmp = std::move(array_[hole]);
    for(int child = hole << 1; child <= size_; hole = child, child =  hole << 1)
    {
        if(child == size_ && array_[child + 1] < array_[child])
            ++child;

        if(array_[child] < tmp)
            array_[hole] = std::move(array_[child]);
        else
            break;
    }
    array_[hole] = std::move(tmp);
}


/**
 * Atomically removes all of the elements from this queue.
 * The queue will be empty after this call returns.
 */
template<typename T>
void  PriorityBlockingQueue<T>::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    capacity_ = 0;
    size_ = 0;
    delete [] array_;
    
}
template<typename T>
const T& PriorityBlockingQueue<T>::peek()
{
    std::lock_guard<std::mutex> lock(mutex_);
    // FIXME: deal with array_ is empty() case 
    return array_[1];
}