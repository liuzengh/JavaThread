# JavaThread

用modern c++仿照java.utils.concurent包实现多线程组件。



### 注意
1. 采用非防御性编程，不进行类型检查和考虑null对象等，认为提供的数据正常。
## to-do

- [x] CountDownLatch, 缺文档
- [x] ArrayBlockingQueue，文档已完善
- [x] LinkedBlockingQueue，缺文档 
- [x] DelayQueue,  文档已完善
- [ ] PriorityBlockingQueue
- [ ] SynchronousQueue
- [x] LinkedBlockingDeque, 文档已完善
- [ ] ConcurrentMap
- [ ] ThreadPoolExector

### ArrayBlockingQueue

ArrayBlockingQueue是一个有界阻塞队列，队列中的对象是先进先出的（FIFO），最早放入的对象位于队列的头部，最后放入的对象存放在队列的尾部。这里的"有界"指的是指初始化ArrayBlockingQueue时需要显式指定队列的容量大小capacity，队列中最多只能存放capacity多个对象，如果用put方法放入一个对象到一个已满的队列，则会导致阻塞；反之，如果用take方法从一个已空队列中取出一个对象，也会阻塞。

#### 数据结构
在Java中使用的是一个固定大小为capacity的数组来存放对象，分别用putIndex和takeIndex下标指示队头和队尾，用count变量来记录当前队列中元素的个数，实现循环缓冲。
1. 当count等于0时，队列为空
2. 当count等于capacity时，队列为满
3. putIndex和takeIndex一开始都相等，且0，每次放或取都会向前移动一个位置，如果等于capacity则会重新赋值为0，这里相当于在对capacity取模，即`putIndex = (putIndex+1) % (capacity)`

我这里的实现的“数组”使用的是vector, 一开始在构造函数里面使用resize(capacity)指定构造capacity个对象，注意这里不建议使用reverse()函数，因为reserse只预留了空间，没有构造对象，vector的size()函数大小依然是返回原来的大小，即为0。用[]访问vector中的元素时虽然不会报错，但是不符合语义，revesrse函数应该和push_back和emplace_back函数配合使用。没有使用std中的array是因为STL中的array的模板为
`template < class T, size_t N > class array;`需要再指定一个size_t参数，不方便在构造函数里面初始化。

#### 并发控制

持有一把全局的互斥锁，在取出和放入对象时分别使用一个条件变量等待可放和可取条件，取出对象之后和放入对象之后分别通知可放事件和可取事件。以放入对象的put方法为例，该方法在队列满时会阻塞，生产线程会睡眠在条件变量notFull上，直到被通知队列非满，在enqueue方法中放入对象之后会notEmpty条件变量会通知非空事件给消费线程。
#### 资源管理

*放入对象*

在put方法中参数value的类型为引用这里减少了拷贝，用const限定同时说明该引用不可修改关联的对象。
> void ArrayBlockingQueue<T>::put(const T &value);

enqueue操作为实际的入队操作，该函数为私有函数，被调用时必须持有锁。enqueue的value参数和put的value参数引用同一个对象，依然减少了拷贝，用const限定同时说明该引用不可修改关联的对象。
> void ArrayBlockingQueue<T>::enqueue(const T &value)

把value放入队列items中时，items_[putIndex_]返回的是一个左值引用T&，而value为一个对常量的左值引用const T&, 使用了赋值=。这里相当于对items_[putIndex_]关联的左值做一个赋值操作，此时会调用T的拷贝赋值运算符或者移动赋值运算符。
> items_[putIndex_] = value; 
```c++
template<typename T>
void ArrayBlockingQueue<T>::put(const T &value)
{
    std::unique_lock<std::mutex> lk(mutex_);
    notFull_.wait(lk, [this]{ return count_ < capacity_; });
    enqueue(value);
}

template<typename T>
void ArrayBlockingQueue<T>::enqueue(const T &value)
{
    items_[putIndex_] = value; 
    if(++putIndex_ == capacity_)
        putIndex_ = 0;
    count_++;
    notEmpty_.notify_one();
}
```

*取出对象*

从队列中取出对象时，该对象在队列中不会再使用，有3种方法进行管理：
1. 复制+移动语义：把队列中的元素复制一份，返回。这里需要一次拷贝赋值操作或移动赋值操作；由于队列中的对象将要消亡，用st::move()得到右值引用，延长其生命周期，减少拷贝。但是函数返回时赋值给其他对象需要进行一次拷贝赋值或移动赋值。
2. 引用+移动语义：传递引用进去，这里需要一次拷贝赋值操作或移动赋值操作；由于队列中的对象将要消亡，用st::move()得到右值引用，延长其生命周期，减少拷贝。
3. 共享指针+移动语义：从堆上分配一个和队列中对象内容一致的对象，此时需要调用T的拷贝构造函数或者移动构造函数，同时用共享指针管理新分配的对象。由于队列中的对象将要消亡，可以用st::move()得到右值引用减少拷贝。

这里的实现采用的是方法3的，这样一方面可以比方法1复制会减少一次拷贝复制操作，令一方面比方法2引用不需要传递参数，之间返回，接口上是良好的。
```c++
template<typename T>
std::shared_ptr<T> ArrayBlockingQueue<T>::take()
{
    std::unique_lock<std::mutex> lk(mutex_);
    notEmpty_.wait(lk, [this]{ return count_ > 0; });
    return dequeue();
}
// 方法1： 复制+移动语义
T ArrayBlockingQueue<T>::dequeue()
{
    T res = std::move(items_[takeIndex_]);
    if(++takeIndex_ == capacity_)
        takeIndex_ = 0;
    count_--;
    notFull_.notify_one();
    return T;
}

// 方法2：引用+移动语义
ArrayBlockingQueue<T>::dequeue(&T value)
{
    value = std::move(items_[takeIndex_]);
    if(++takeIndex_ == capacity_)
        takeIndex_ = 0;
    count_--;
    notFull_.notify_one();
}

// 方法3：共享指针+移动语义
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
```
### DelayQueue

DelayQueue是一个无界阻塞队列，放置于队列中的延迟对象Delayed只有到期后才能被取出。从队列中取出的对象是按照它们的延时时间排序取出的，队头存放的对象过期时间最早。**值得注意的是，DelayQueue实现了领导者/追随者模式(Leader-Follower pattern)的变体，来最小化不必要的时间等待，我在并发控制一节会详细说明**。由于无界队列阻塞操作只可能出现在消费者一端，如果不存在过期的对象，取操作take会阻塞，poll操作会返回null；而生产者放置对象的put和offer操作不会产生阻塞。

#### 数据结构

底层的数据结构是优先队列(PriorityQueue)，始终维持队首的对象最早过期，持有一把全局锁和一个条件变量。

*Delayed对象*
1. 放置在队列中的Delayed接口需要实现Comparable接口，这是方便DelayQueue对Delayed对象进行比较，使得最先过期的放置于优先队列的队首。
2. Delayed对象同时需要实现java.util.concurrent.Delayed接口中的getDelay()的方法，getDelay方法如果返回0或者是负值，则说明该元素已经过期了，调用DelayQueue中的take()方法可以将该元素取出。

我用C++实现的一个自定义的Delayed类如下所示：
1. 需要实现Delay类中的<运算符进行重载，
2. getDelay方法直接返回超时的绝对时间，关于时间的操作我直接使用了chrono中的工具,使用`chrono::nanoseconds`最小时间间隔，使用稳定的时钟`std::chrono::steady_clock`。
3. 另外，getDelay方法应该声明为常成员函数，该操作中不修改成员变量的值，应该q.top()返回的是对常量的引用，这是因为我在实现中运用了如下写法q.top().getDelay()。

```c++
class Delayed
{
    private:
        std::chrono::steady_clock::time_point timeout_;
        int index_;
    public:
        Delayed(chrono::nanoseconds timeUint): timeout_(std::chrono::steady_clock::now()+ timeUint){};
        Delayed(std::chrono::steady_clock::time_point timeout, int index): timeout_(timeout), index_(index){};
        bool operator<(const Delayed&rhs) const
        {
            return this->timeout_ >= rhs.timeout_;
        }
        std::chrono::steady_clock::time_point  getDelay() const
        {
            return timeout_;
        };
        int getIndex() const {return index_; }
};
```

#### 并发控制

*领导者/追随者模式*
从服务器的并发编程角度来看可以分为半同步/半异步(half-sync/half-saync)模式和领导者/追随者模式(Leader-Followers)

领导者/追随者模式是多个工作线程轮流获得事件源集合，轮流监听、分发处理的一种模式。在任意时间点，程序都仅有一个领导者线程，它负责监听I/O事件、而其他线程都是追随者，它们休眠在线程池中等待成为领导者。当前的领导者如果检测到I/O事件，首先要从线程池中推选出新的领导者，然后处理I/O事件。此时新的领导者等待I/O事件，而原来的领导者则处理I/O事件，二者实现了并发。

在DelayQueue中的并发逻辑和上述说明非常类似，具体如下：
当一个线程成为leader的时候，它等待直到队首对象的超时的绝对时间，而其他线程则进入无限等待中。当leader线程从队列中取走元素之前必须通知其他线程，除非其他线程在这之间已经成为了leader。当生产者新放入的Delayed的对象比队列中所有的对象的超时时间点都靠前的时候，这时leader被重置为null，同时生产线程唤醒等待的消费线程，消费线程最后只有消费者争先成为leader。所有处在等待中的消费线程都随时准备好获取和失去领导权。

实现上用`std::thread::id leader_;`来标识当前的线程，`std::thread::id`可以提供比较运算，其内部类型实质为pthread_t是一个无符号的整型，线程库并没有提供其是否有效的判断，不能想当然的置为0当作无效状态，我这里额外使用`bool hasLeader_;`来提供有效判断，当该值为真是`leader_`代表的线程是leader线程，否找当前处于无leader状态。
    
*放入对象*
由于是无界阻塞队列，放入操作put和offer是一样的，需要注意的是如果放入前队列为空或者要放入的对象比队列中所有对象都先超时的话，需要重置leader，释放领导权，同时通知消费线程。
> queue_.size() == 0 ||  value < queue_.top()
```c++
template<typename T>
bool DelayQueue<T>::offer(const T &value)
{

    std::lock_guard<std::mutex> lock(mutex_);
    bool  resetLeader_ = queue_.size() == 0 ||  value < queue_.top()
                        ? true
                        : false;
    queue_.push(value);
    if(true)
    {
        hasLeader_ = false;
        available_.notify_one();
    }
    return true;
}
```
*取出对象*
 
 具体说明下面代码中的注释，一个典型的场景是考虑一个要很久过期的对象先入队，然后消费线程都睡眠在队首了，接着一个然后一个马上要过期的对象在入队的情形。
```c++
template<typename T>
std::shared_ptr<T> DelayQueue<T>::take()
{
    std::unique_lock<std::mutex> lock(mutex_);
    for(;;) // 没有在条件变量wait方法上加入条件，这是因为下面代码块逻辑稍多，需要无限循环，防止假醒
    {
        if(queue_.size() == 0)
            available_.wait(lock);
        else
        {
            std::chrono::steady_clock::time_point timeout =  (queue_.top()).getDelay(); 
            /*队首对象已经超时，退出循环，然后取出*/
            if(timeout <= std::chrono::steady_clock::now())
                break;
            
            /* 当一个线程成为leader的时候，它等待直到队首对象的超时的绝 对时间，而其他线程则进入无限等待中。*/
            if(hasLeader_)
                available_.wait(lock);
            else
            {
                std::thread::id thisThread =  std::this_thread::get_id();
                leader_ =  thisThread;
                available_.wait_until(lock, timeout);
                if(leader_ == thisThread)
                    hasLeader_ = false;
            }
        }
    }
    /*取出队首对象*/
    std::shared_ptr<T> const res(std::make_shared<T>(std::move(queue_.top())));
    queue_.pop(); 
    /* 当leader线程从队列中取走元素之前必须通知其他线程，除非其他线程在这之间已经成为了leader。*/
    if(!hasLeader_ && queue_.size() > 0)
        available_.notify_one();
    return res;
}
```




### LinkedBlockingDequeue
- Dequeue: Double End Queue, 双端队列

LinkedBlckingDeque是基于双向链表实现的 optionally-bounded 阻塞双端队列，构造时可以指定队列的容量大小，默认的容量大小为：`std::numeric_limits<int>::max()`。链表结点在每次进行插入操作时动态分配，直到达到容量上限为止。
双端队列允许在队列的两端进行插入和删除操作, 忽略阻塞等待时间，绝大多数的操作的时间复杂度为O(1)：
- 队首的操作：putFirst, offerFirst, takeFirst, pollFirst
- 队尾的操作：putLast, offerLast, takeLast, pollLast
- 移除全部元素：clear，时间复杂度为O(n)

数据结构：由于链表中的一个结点可以被前后两个结点所指向，所以对结点定义时采用了share_ptr进行包装，同时指向的对象资源也使用share_ptr指针管理。双向链表中的头结点指针first和尾结点指针last初始化时为空，再整个操作中分别保持以下循环不变式成立：
> (first == nullptr && last == nullptr) || (first->prev == nullptr && first->item != nullptr)
> 
> (first == nullptr && last == nullptr) || (last->next == nullptr && last->item != nullptr)           
```c++
struct Node
 {
    std::shared_ptr<T> item;
    std::shared_ptr<Node> prev;
    std::shared_ptr<Node> next;
    Node(T value){ item = std::make_shared<T>(std::move(value)); }
};
std::shared_ptr<T> first;
std::shared_ptr<T> last; 
```

并发实现：持有一把全局的互斥锁，在取出和放入元素时分别使用一个条件变量等待可放和可取条件。
以putLast方法为例：先构造指向元素的共享指针pnode，这里使用了std::move(value)移动语义，提高效率；然后再上锁，这样可以尽量减少锁的粒度；接着条件变量notFull_等待放入操作linkLast返回成功，否找将进入睡眠。linkFirst函数首先判断当前元素个数是否大于队列容量，如果是则返回false; 否找在链表的表头插入pnode结点，并将当前元素加1，在退出之前通知等待在notEmpty条件变量上的取操作线程。注意，这里使用的notify_one操作只唤醒其中一个等待在条件变量notEmpty上的线程，可以缓解竞争。
```c++
template<typename T>
void LinkedBlockingDeque<T>::putLast(T value)
{
    std::shared_ptr<T> pnode(new Node(std::move(value)));
    std::unique_lock<std::mutex> putLock(mutex_);
    notFull_.wait(putLock, [thise]{ return linkLast(std::move(pnode)); });
}

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

```

## Reference

1. http://tutorials.jenkov.com/java-util-concurrent/blockingqueue.html
2. [Package java.util.concurrent API document](https://docs.oracle.com/javase/8/docs/api/index.html?java/util/concurrent/package-summary.html)
3. [C++ Concurrency in Action](https://book.douban.com/subject/27036085/)
4. [Java Concurrency In Practice](https://book.douban.com/subject/1888733/)

