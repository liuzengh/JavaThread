# JavaThread

用modern c++仿照java.utils.concurent包实现多线程组件。

### 注意
1. 采用非防御性编程，不进行类型检查和考虑null对象等，认为提供的数据正常。
## to-do

- [x] CountDownLatch, 缺文档
- [x] ArrayBlockingQueue，缺文档
- [x] LinkedBlockingQueue，缺文档 
- [ ] DelayQueue
- [ ] PriorityBlockingQueue
- [ ] SynchronousQueue
- [x] LinkedBlockingDeque 文档完成
- [ ] ConcurrentMap
- [ ] ThreadPoolExector

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
2. https://docs.oracle.com/javase/8/docs/api/index.html?java/util/concurrent/package-summary.html
3. [C++ Concurrency in Action](https://book.douban.com/subject/27036085/)
4. [Java Concurrency In Practice](https://book.douban.com/subject/1888733/)

