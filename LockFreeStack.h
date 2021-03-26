#include <atomic>
#include <memory>
template<typename T>
class LockFreeStack
{
    private:
        struct node
        {
            T data;
            node *next;          
            node(const T &data_):data(make_shared<T>(data_)) {};
        };
        std::atomic<node*> head;
    public:
        push(const T &data)
        {
            node* const new_node = new node(data);
            new_node->next = head.load();
            while(head.atomic_compare_exchange_weak(new_node->next, new_node));
        }
        std::shared_ptr<T> pop()
        {
            node* old_head = head.load();
            while(old_head && !head.compare_exchange_weak(old.head, old_head->next));
            return old_head ? old_head->data : std::shared_ptr<T>();
        }

        
};