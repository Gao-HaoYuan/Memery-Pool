#ifndef _TIMER_
#define _TIMER_

// 定时器类
class UtilTimer {
public:
    UtilTimer() : next(nullptr){}
    UtilTimer(void *ptr_, size_t bytes_, void (*f)(void *ptr, size_t bytes)) 
            : ptr(ptr_), bytes(bytes_), CbFunc(f), next(nullptr) {}

    ~UtilTimer() = default;

public:
    // 内存单元首地址
    void                *ptr;  

    // 内存单元所占内存大小                                     
    size_t              bytes;                                      

    // 回调函数，将内存单元的首地址和大小传入该函数。清除该内存单元，同时，该成员变量为函数指针，需要指向一个函数
    void                (*CbFunc)(void *ptr, size_t bytes);   

    // 指向后一个定时器      
    UtilTimer*          next;                                       
};


// 定时器链表，它是一个升序链表，且带有头节点。
class TimerList 
{
private:
    UtilTimer* head;   // 头结点
    
public:
    TimerList() : head(nullptr) {}

    // 链表被销毁时，删除其中所有的定时器
    ~TimerList() 
    {
        UtilTimer* tmp = head;
        while(tmp) {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }
    
    // 将目标定时器timer添加到链表中
    void AddTimer(
        UtilTimer* timer            // I : 定时器
    ) 
    {
        if(!timer) 
        {
            return;
        }

        if(!head) 
        {
            head = timer;
            return; 
        }

        timer->next = head->next;
        head = timer;
    }

    // 定时器实际执行的函数
    // 每隔 5 秒定时器函数中执行一次 tick() 函数，以清除链表中的内存单元
    void Tick() {
        if(!head) {
            return;
        }

        UtilTimer* tmp = head;

        // 从头节点开始依次处理每个定时器，直到遇到一个尚未到期的定时器
        while(tmp) 
        {
            // 调用定时器的回调函数，以执行定时任务
            tmp->CbFunc(tmp->ptr, tmp->bytes);

            // 执行完定时器中的定时任务之后，就将它从链表中删除，并重置链表头节点
            head = tmp->next;

            delete tmp;
            tmp = head;
        }
    }
};

#endif
