#include <limits>
#include <memory>
#include <mutex>
#include <condition_variable>
template<typename T>
class LinkedBlockingDeque
{

    
    /*
     * Implemented as a simple doubly-linked list protected by a
     * single lock and using conditions to manage blocking.
     */
    public:
        explicit LinkedBlockingDeque(int capacity);
        LinkedBlockingDeque(const &LinkedBlockingDeque) = delete;
        LinkedBlockingDeque& operator=(const &LinkedBlockingDeque) = delete;
        ~LinkedBlockingDeque() = default;
        void putFirst(T value);
        bool offerFirst(T value);
        std::shared_ptr<T> takeFirst();
        std::shared_ptr<T> pollFirst();

        void putLast(T value);
        bool offerLast(T value);
        std::shared_ptr<T> takeLast();
        std::shared_ptr<T> pollLast();

        
        bool empty() const;
        int capacity() const;
        int size() const;
        void clear();


    private:
        bool linkFirst(std::shared_ptr<T> pnode);
        std::shared_ptr<T> unlinkFirst();

        bool linkLast(std::shared_ptr<T> pnode);
        std::shared_ptr<T> unlinkLast();
        


    private:

        /** Maximum number of items in the deque */
        const int capacity_;
        
        /** Number of items in the deque */
        int count_;

        /** Main lock guarding all access */
        std::mutex mutex_;

        /** Condition for waiting takes */
        std::condition_variable notFull_;

        /** Condition for waiting puts */
        std::condition_variable notEmpty_;

        
        struct Node
        {
            /**
            * The item, or null if this node has been removed.
            */
            std::shared_ptr<T> item;

             /**
             * One of:
             * - the real predecessor Node
             * - this Node, meaning the predecessor is last
             * - null, meaning there is no predecessor
             */
            std::shared_ptr<Node> prev;
            
            /**
             * One of:
             * - the real successor Node
             * - this Node, meaning the successor is first
             * - null, meaning there is no successor
             */
            std::shared_ptr<Node> next;
            Node(T value){ item = std::make_shared<T>(std::move(value)); }
        };

        /**
         * Pointer to first node.
         * Invariant: (first == null && last == null) ||
         *            (first.prev == null && first.item != null)
         */
        std::shared_ptr<T> first;

        
        /**
         * Pointer to last node.
         * Invariant: (first == null && last == null) ||
         *            (last.next == null && last.item != null)
         */
        std::shared_ptr<T> last; 
};

template<typename T> 
LinkedBlockingDeque<T>::LinkedBlockingDeque(int capacity = std::numeric_limits<int>):
    capacity_(capacity),
    count_(0),
    last(nullptr)
{

}


// Basic linking and unlinking operations, called only while holding lock

 /**
  * Links node as first element, or returns false if full.
  */
template<typename T>
bool LinkedBlockingDeque<T>::linkFirst(std::shared_ptr<T> pnode)
{
    if(count_ >= capacity_)
        return false;
    pnode->next = first;

    if(last == nullptr)
        last = pnode;
    else
        frist->prev = pnode;
    first = pnode;
    ++count_;
    notEmpty_.notify_one();
    return true;
}

/**
  * Removes and returns first element, or null if empty.
  */

template<typename T>
std::shared_ptr<T> LinkedBlockingDeque<T>::unlinkFirst()
{
    if(first == nullptr)
        return std::shared_ptr<T>();
    std::shared_ptr<T> const res(first->item);
    if(first->next == nullptr)
        last.reset();
    else
        ((first->next)->prev).reset();
    first = first->next;
    --count_;
    notFull_.notify_one();
    return res;
}

/**
 * Links node as last element, or returns false if full.
 */

template<typename T>
bool LinkedBlockingDeque<T>::linkLast(std::shared_ptr<T> pnode)
{
    if(count_ >= capacity_)
        return false;
    pnode->prev = last;
    
    if(last == nullptr)
        first = pnode;
    else
        last->next = pnode;
    last = pnode;
    ++count_;
    notEmpty_.notify_one();
    return true;
}


/**
 * Removes and returns last element, or null if empty.
 */
template<typename T>
std::shared_ptr<T> LinkedBlockingDeque<T>::unlinkLast()
{
    if(last == nullptr)
        return std::shared_ptr<T>();
    std::shared_ptr<T> const res(last->item);
    if(last->prev == nullptr)
        first.reset();
    else
        ((last->prev)->next).reset();
    last = last->prev;
    --count_;
    notFull_.notify_one();
    return res;
}

template<typename T>
void LinkedBlockingDeque<T>::putFirst(T value)
{
    std::shared_ptr<T> pnode(new Node(std::move(value)));
    std::unique_lock<std::mutex> putLock(mutex_);
    notFull_.wait(putLock, [thise]{ return linkFirst(pnode); });
}

template<typename T>
bool LinkedBlockingDeque<T>::offerFirst(T value)
{
    std::shared_ptr<T> pnode(new Node(std::move(value)));
    std::lock_guard<std::mutex> putLock;
    return linkFirst(pnode);
}



template<typename T>
std::shared_ptr<T> LinkedBlockingDeque<T>::takeFirst()
{
    std::shared_ptr<T> res;
    std::unique_lock<std::mutex> takeLock(mutex_);
    
    notEmpty_.wait(takeLock, [&]{ 
        res = unlinkFirst();
        return res != nullptr; });
    return res;
}

template<typename T>
std::shared_ptr<T> LinkedBlockingDeque<T>::pollFirst()
{
    std::lock_guard<std::mutex> takeLock(mutex_);
    return unlinkFirst();
}





template<typename T>
void LinkedBlockingDeque<T>::putLast(T value)
{
    std::shared_ptr<T> pnode(new Node(std::move(value)));
    std::unique_lock<std::mutex> putLock(mutex_);
    notFull_.wait(putLock, [thise]{ return linkLast(pnode); });
}

template<typename T>
bool LinkedBlockingDeque<T>::offerLast(T value)
{
    std::shared_ptr<T> pnode(new Node(std::move(value)));
    std::lock_guard<std::mutex> putLock;
    return linkLast(pnode);
}



template<typename T>
std::shared_ptr<T> LinkedBlockingDeque<T>::takeLast()
{
    std::shared_ptr<T> res;
    std::unique_lock<std::mutex> takeLock(mutex_);
    notEmpty_.wait(takeLock, [&]{ 
        res = unlinkLast();
        return res != nullptr; });
    return res;
}

template<typename T>
std::shared_ptr<T> LinkedBlockingDeque<T>::pollLast()
{
    std::lock_guard<std::mutex> takeLock(mutex_);
    return unlinkLast();
}






template<typename T>
bool LinkedBlockingDeque<T>::empty() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return count_.load() == 0;
}


template<typename T>
int LinkedBlockingDeque<T>::size() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return count_.load();
}

template<typename T>
int LinkedBlockingDeque<T>::capacity() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return capacity_;
}


/**
* Atomically removes all of the elements from this deque. 
* The deque will be empty after this call returns.
*/

template<typename T>
void LinkedBlockingDeque<T>::clear()
{
   std::lock_guard<std::mutex> lk(mutex_);
   for(std::shared_ptr<T> f = first; f != nullptr; )
   {
       (f.item).reset();
       auto n = f->next;
       (f->prev).reset();
       (f->next).reset();
       f = n;
   }
   first.reset();
   last.reset();
   count_= 0;
   notFull_.notify_all();
}
