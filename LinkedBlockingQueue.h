#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <limits>

template<typename T>
class LinkedBlockingQueue
{
    
    /*
     * A variant of the "two lock queue" algorithm.  The putLock gates
     * entry to put (and offer), and has an associated condition for
     * waiting puts.  Similarly for the takeLock.  The "count" field
     * that they both rely on is maintained as an atomic to avoid
     * needing to get both locks in most cases. Also, to minimize need
     * for puts to get takeLock and vice-versa, cascading notifies are
     * used. When a put notices that it has enabled at least one take,
     * it signals taker. That taker in turn signals others if more
     * items have been entered since the signal. And symmetrically for
     * takes signalling puts. Operations such as remove(Object) and
     * iterators acquire both locks.
     * */

    public:
        explicit LinkedBlockingQueue(int capacity);
        ~LinkedBlockingQueue();
        LinkedBlockingQueue(const LinkedBlockingQueue&) = delete;
        LinkedBlockingQueue& operator=(const LinkedBlockingQueue& ) = delete;

        void put(T new_value);
        bool offer(T new_value);
        std::shared_ptr<T> take();
        std::shared_ptr<T> poll();

        bool empty() const;
        int capacity() const;
        int size() const;
        void clear();


  
    private:
        /**
        * Linked list node class.
        */
        struct Node
        {
            std::shared_ptr<T> item;
            /**
             * One of:
             * - the real successor Node
             * - this Node, meaning the successor is head.next
             * - null, meaning there is no successor (this is the last node)
            */
            std::unique_ptr<T> next;
            Node(T value)
            {
                item = std::make_shared<T>(std::move(value)); 
            }
            Node();
        };
          /** The capacity bound, or std::numeric_limits<int>::max() if none */
        const int capacity_;

        /** Current number of elements */
        std::atomic<int> count_;

        /**
        * Head of linked list.
        * Invariant: head.item == null
        */
        std::unique_ptr<Node> head_;

        /**
        * Tail of linked list.
        * Invariant: last.next == null
        */
        Node *tail;

        /** Lock held by take, poll, etc */
        std::mutex headMutex_;

        /** Wait queue for waiting takes */
        std::condition_variable notEmpty_;
        
         /** Lock held by put, offer, etc */
        std::mutex tailMutex_;

         /** Wait queue for waiting puts */
        std::condition_variable notFull_;

        private:
            void enqueue( std::unique_ptr<T> pnode);
            std::shared_ptr<T> dequeue();
};

template<typename T>
LinkedBlockingQueue<T>::LinkedBlockingQueue(int capacity = std::numeric_limits<int>::max() ):
    capacity_(capacity), 
    count_(0),
    head_(new Node()),
    tail_(head_.get())
{

}

/* Inserts the specified element into this queue, 
 * waiting if necessary for space to become available. 
 */
template<typename T>
void LinkedBlockingQueue<T>::put(T new_value)
{


    std::unique_ptr<Node> pnode(new Node(new_value));
    std::unique_lock<std::mutex> putLcok(tailMutex_);
    
     /*
    * Note that count is used in wait guard even though it is
    * not protected by lock. This works because count can
    * only decrease at this point (all other puts are shut
    * out by lock), and we (or some other waiting put) are
    * signalled if it ever changes from capacity. Similarly
    * for all other uses of count in other wait guards.
    */
    
    notFull.wait(putLcok, [this]{ return count_.load() < capacity_; });
    enqueue(std::move(pnode));

    int c = count_.fetch_add(1);
    if(c + 1 < capacity_)
        notFull_.notify_one();
    putLcok.unlock();
    if(c == 0)
        notEmpty_.notify_one();

}

template<typename T>
bool LinkedBlockingQueue<T>::offer(T new_value)
{
    std::unique_ptr<Node> pnode(new Node(new_value));
    std::lock_guard<std::mutex> putLcok(tailMutex_);
    if(count_.load() == capacity_)
        return false;
    enqueue(std::move(pnode));
    
    int c = count_.fetch_add(1);
    if(c + 1 < capacity_)
        notFull_.notify_one();
    putLcok.unlock();
    if(c == 0)
        notEmpty_.notify_one();

    return true;

}


template<typename T>
void LinkedBlockingQueue<T>::enqueue( std::unique_ptr<T> pnode)
{
    tail->next = std::move(pnode);
    tail = (tail->next).get();
}

template<typename T>
std::shared_ptr<T> LinkedBlockingQueue<T>::poll()
{
    std::unique_lock<std::mutex> putLock(headMutex_);
    if(count_.load() == 0)
        return std::shared_ptr<T>(); //return nullptr;
    std::shared_ptr<T> res = dequeue();
    int c = count_.fetch_sub(1);
    if(c > 1)
        notEmpty_.notify_one();
    putLock.unlock();
    if(c == capacity_)
        notFull_.notify_one();
    return res;
}

template<typename T>
std::shared_ptr<T> LinkedBlockingQueue<T>::take()
{
    std::lock_guard<std::mutex> putLock(headMutex_);
    notEmpty_.wait(headLock, [this]{ count_.load() > 0; });
    std::shared_ptr<T> res = dequeue();
    
    int c = count_.fetch_sub(1);
    if(c > 1)
        notEmpty_.notify_one();
    putLock.unlock();
    if(c == capacity_)
        notFull_.notify_one();
    return res;
}


template<typename T>
std::shared_ptr<T> LinkedBlockingQueue<T>::dequeue()
{
    std::shared_ptr<T> const res((head_->next)->item);
    head_->next = std::move((head_->next)->next);    
    return res;
}



template<typename T>
bool LinkedBlockingQueue<T>::empty() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return count_.load() == 0;
}


template<typename T>
int LinkedBlockingQueue<T>::size() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return count_.load();
}

template<typename T>
int LinkedBlockingQueue<T>::capacity() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return capacity_;
}

template<typename T>
void LinkedBlockingQueue<T>::clear()
{
     /**
     * Locks to prevent both puts and takes.
     */
    // std::lock_guard<std::mutex> putLock(tailMutex_); Bad
    // std::lock_guard<std::mutex> takeLock(headMutex_); Bad
    // In C++17 std::scoped_lock guard(tailMutex_, headMutex_); Good
    // See:C++ Concurreny In Action 3.2.4 Deadlock: the problem and a solution
    std::lock(tailMutex_, headMutex_);
    std::lock_guard<std::mutex> putLock(tailMutex_, std::adopt_lock);
    std::lock_guard<std::mutex> takeLock(headMutex_, std::adopt_lock);
    while(head_->next)
    {
        ((head_->next)->item).reset();
        head_->next = (head_->next)->next;
    }
    tail_ = head_.get();
    if(count_.exchange(0) == capacity_)
        notFull_.notify_one();
}
