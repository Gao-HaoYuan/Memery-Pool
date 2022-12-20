#pragma once
#ifndef _ALLOC_
#define _ALLOC_

#include "Interval.h"
#include "Timer.h"

#include <cstdlib>
#include <new>
#include <mutex>
#include <memory>

#define     TIMESLOT        10
#define     _ALIGN          8
#define     _MAX_BYTES      128
#define     _NFREELIST      _MAX_BYTES / _ALIGN


// inst 的作用主要用于声明内存池的类别，0是默认的内存池
template<int inst>
class FirstMalloc
{
private:
    static void (*Handler)(); // 函数指针，属于类的一个成员变量，指向(OOM) out of memory 操作

    static void *OOM_Malloc(size_t);

    static void *OOM_Realloc(void *, size_t); 

public:
    /**
     * 调用std中的malloc分配内存，当堆中无内存可以分配时，调用OOM机制
     * 
     * 返回分配内存的首地址
     * 
     * 堆中的内存是线程公有的
     */
    static void *Allocate(
        size_t n    // I : 指定需要分配的内存的数值
    )
    {
        void *result = std::malloc(n);

        if(result == nullptr)
        {
            result = OOM_Malloc(n);
        }

        return result;
    }

    static void Deallocate(
        void *p,    // I : 释放内存的首地址
        size_t      // I : 释放内存的大小
    ) 
    {
        std::free(p);
    }

    /**
     * 调用std中的realloc函数重新分配内存, 当堆中无内存可以分配时，调用OOM机制
     * 
     * 返回新的分配的内存的首地址
     */
    static void *Reallocate(
        void *p,            // I : 内存的首地址
        size_t,             // I : 旧内存的大小，暂时无用
        size_t NewSize      // I : 重新分配内存的大小
    )
    {
        void *result = std::realloc(p, NewSize);

        if(result == nullptr)
        {
            result = OOM_Realloc(p, NewSize);
        }

        return result;
    }

    /**
     * 将成员变量 handler 赋值
     * 
     * 返回旧的 OOM 机制的函数
     */
    static void (*SetMallocHandler(
        void (*f)()         // I : OOM机制的函数
    ))()                    // 返回值是一个函数指针
    {
        void (*old)() = Handler;
        Handler = f;
        return old;
    }
};

template<int inst>
void (*FirstMalloc<inst>::Handler)() = nullptr;

/**
 * 循环调用OOM机制，分配内存
 * 
 * 返回分配内存的首地址
 */
template<int inst>
void *FirstMalloc<inst>::OOM_Malloc(
    size_t n                // I : 分配内存的大小
)
{
    void (*MyHandler)();
    void *result;

    for(; ;)
    {
        MyHandler = Handler;

        if(MyHandler == nullptr)
        {
            std::bad_alloc();
        }

        MyHandler();

        result = std::malloc(n);

        if(result)
        {
            return result;
        }
    }
}

/**
 * 循环调用OOM机制，分配内存
 * 
 * 返回分配内存的首地址
 */
template<int inst>
void *FirstMalloc<inst>::OOM_Realloc(
    void *p,        // I : 旧内存的首地址
    size_t n        // I : 需要分配的新的内存的大小
)
{
    void (*MyHandler)();
    void *result;

    for(; ;)
    {
        MyHandler = Handler;

        if(MyHandler == nullptr)
        {
            std::bad_alloc();
        }

        MyHandler();

        result = std::realloc(p, n);

        if(result)
        {
            return result;
        }
    }
}

// 默认的一级空间配置器
typedef FirstMalloc<0> Malloc;


template<bool threads, int inst>
class SecondMalloc
{
private:
    // 自由链表的节点
    union obj
    {
        union obj *next; // 下一个空间的区块
        char client[1]; // 本块内存的首地址
    };

    static obj *volatile FreeList[_NFREELIST]; // 自由链表

    static char *StartFree; // 分配内存的首地址
    static char *EndFree;   // 分配内存的尾地址
    static size_t HeapSize; // 附加内存的大小---->

    static obj *volatile Pool; // 以单链表的形式记录分配的内存块，用以内存释放

    static TimerList *DelList; // 以单链表的形式记录将要释放的内存单元

    static IntervalId id;       // 定时器的返回id， 用于关闭定时任务

    static std::unique_ptr<std::mutex> MtxFreeList;         // 自由链表的锁

    static std::shared_ptr<std::mutex> MtxDelList;          // 用于记录将要删除的内存单元的链表的锁

    /**
     * 通过传入匿名函数执行指定的回调函数，并返回定时器的 id
     * 
     * Return : 定时器id
     */
    static IntervalId getId()
    {
        return setInterval(TIMESLOT, [&](
            DelList->Tick();
        ));
    }

    /**
     * 根据申请的块的大小，获取自由链表编号
     * 
     * 返回自由链表的偏移量
     */
    static size_t FreeListIndex(
        size_t bytes    // I : 需要申请的内存大小
    )
    {
        return ((bytes + _ALIGN - 1) / _ALIGN - 1);
    }
    
    /**
     * 根据 bytes,上调到8的倍数
     * 
     * 返回调整后的内存大小
     */
    static size_t RoundUP(
        size_t bytes // I : 需要申请的内存大小
    )
    {
        return ((bytes + _ALIGN -1) & ~(_ALIGN - 1));
    }

    static void *Refill(size_t bytes, size_t nobjs = 20);

    static char *ChunkAlloc(size_t bytes, size_t& nobjs);

public:
    static void *Allocate(size_t bytes, size_t nobjs = 20);

    static void Deallocate(void *ptr, size_t bytes);

    static void RecordDelNode(void *ptr, size_t bytes);
    
    static void *Reallocate(void *ptr, size_t old_size, size_t new_size, size_t nobjs = 20);


    /**
     * 返回定时器链表的锁，在内存池外部进行加锁操作
     * 
     * Return : 锁
     */
    static std::shared_ptr<std::mutex> getDelMutex()
    {
        return MtxDelList;
    }   

    /**
     * 一般应该由主线程释放
     * 
     * 遍历单链表，释放内存
     */
    static void free()
    {  
        DelList->Tick();

        obj *cur = Pool, *next = nullptr;

        while(cur)
        {   
            next = cur->next;
            std::free(cur);
            cur = next;
        }

        // 按理说当程序结束的时候，这个类就不存在了，所有的成员也都自己销毁了，
        // 为了逻辑正确还是写了这些代码
        Pool = nullptr;

        for(int i = 0; i < 16; i++)
        {
            FreeList[i] = nullptr;
        }

     StartFree = EndFree = nullptr;
    }
};

    /*
    * 初始化参数
    */
    template<bool threads, int inst>
    char* SecondMalloc<threads, inst>::StartFree = nullptr;

    template<bool threads, int inst>
    char* SecondMalloc<threads, inst>::EndFree = nullptr;

    template<bool threads, int inst>
    size_t SecondMalloc<threads, inst>::HeapSize = sizeof(obj);

    template<bool threads, int inst>
    typename SecondMalloc<threads, inst>::obj *volatile SecondMalloc<threads, inst>::Pool = nullptr;

    template<bool threads, int inst>
    TimerList *SecondMalloc<threads, inst>::DelList = new TimerList();

    template<bool threads, int inst>
    IntervalId SecondMalloc<threads, inst>::id = SecondMalloc<threads, inst>::getId();

    template<bool threads, int inst>
    std::unique_ptr<std::mutex> SecondMalloc<threads, inst>::MtxFreeList = std::make_unique<std::mutex>();

    template<bool threads, int inst>
    std::shared_ptr<std::mutex> SecondMalloc<threads, inst>::MtxDelList = std::make_shared<std::mutex>();

    template<bool threads, int inst>
    typename SecondMalloc<threads, inst>::obj *volatile SecondMalloc<threads, inst>::FreeList[_NFREELIST] = 
    {
        nullptr, nullptr, nullptr, nullptr, 
        nullptr, nullptr, nullptr, nullptr, 
        nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr
    };


    /**
     * 当申请的内存大于128字节直接使用一级配置器分配内存，当申请内存小于128字节，首先获取自由链表的编号，然后分配内存并填充自由链表
     * 
     * 需要对自由链表加锁
     * 
     * 返回分配内存的首地址
     */
    template<bool threads, int inst>
    void *SecondMalloc<threads, inst>::Allocate(
        size_t bytes,      // I : 申请的内存的大小
        size_t nobjs       // I : 分配内存的块数
    )
    {
        if(bytes > (size_t)_MAX_BYTES){
            return (Malloc::Allocate(bytes));
        }
        
        // std::lock_guard<std::mutex> locker(*MtxFreeList.get());
        MtxFreeList->lock();

        // 选择自由链表编号
        obj *volatile *MyList = FreeList + FreeListIndex(bytes);
        obj *result = *MyList;

        // 没有可用的自由链表， 重新填充自由链表
        if(result == nullptr){
            void *r = Refill(RoundUP(bytes), nobjs);

            MtxFreeList->unlock();
            return r;
        }

        //调整freelist, 将list后面的空间前移，返回list所指的空间
        *MyList = result->next;

        MtxFreeList->unlock();
        return result; // 返回分配的空间的地址
    }

    /**
     * 释放分配的内存，当申请的内存大于128字节，通过一级空间配置器释放内存，
     * 当内存小于128字节，把内存放回自由链表，内存并未真正释放
     * 需要对自由链表加锁
     */
    template<bool threads, int inst>
    void SecondMalloc<threads, inst>::Deallocate(
        void *ptr,          // I : 申请内存的首地址
        size_t bytes        // I : 释放内存的大小
    )
    {
        if(bytes > (size_t)_MAX_BYTES){
            Malloc::Deallocate(ptr, bytes);
            return;
        }

        MtxFreeList->lock();

        obj * node = static_cast<obj*>(ptr);
        obj* volatile *MyList = FreeList + FreeListIndex(bytes);
        node->next = *MyList;
        *MyList = node;

        MtxFreeList->unlock();
    }

    /**
     * 把将要释放的内存用链表进行记录，并在之后释放
     */
    template<bool threads, int inst>
    void SecondMalloc<threads, inst>::RecordDelNode(
        void *ptr,          // I : 申请内存的首地址
        size_t bytes        // I : 释放内存的大小
    )
    {
        if(bytes > (size_t)_MAX_BYTES){
            Malloc::Deallocate(ptr, bytes);
            return;
        }

        UtilTimer *tmp = new UtilTimer(ptr, bytes, Deallocate);
        DelList->AddTimer(tmp);
    }

    /**
     * 重新分配空间
     * 
     * 首先释放旧的内存
     * 然后分配新的内存
     */
    template<bool threads, int inst>
    void* SecondMalloc<threads, inst>::Reallocate(
        void *ptr,                 // I : 重新分配内存的首地址
        size_t OldSize,            // I : 释放的内存的大小
        size_t NewSize,            // I : 重新分配的内存的大小
        size_t nobjs               // I : 分配内存的块数
    )
    {
        Deallocate(ptr, OldSize);
        ptr = Allocate(NewSize, nobjs);
        return ptr;
    }


    /**
     * 首先调用ChunkAlloc函数分配内存
     * 
     * 如果nobjs为1说明自由链表还有空间，否则把分配的内存重现链接到自由链表中
     * 
     * 返回分配内存的首地址
     */
    template<bool threads, int inst>
    void *SecondMalloc<threads, inst>::Refill(
        size_t bytes,               // I : 分配内存的大小
        size_t nobjs               // I : 多分配的内存的块数，默认值为20
    )
    {
        char *chunk = ChunkAlloc(bytes, nobjs);

        if(nobjs == 1)
        {
            return chunk;
        }

        obj *volatile *MyList = FreeList + FreeListIndex(bytes);
        obj *result = (obj *)chunk;

        obj *CurrentObj, *NextObj;

        *MyList = NextObj = (obj*)(chunk + bytes);

        for(size_t i = 1; ; ++i)
        {
            CurrentObj = NextObj;

            if(i == nobjs - 1)
            {
                CurrentObj->next = nullptr;
                break;
            }

            NextObj = (obj*)((char*)NextObj + bytes);
            CurrentObj->next = NextObj;
        }

        return result;
    }

    /**
     * 分配内存块，首先计算扩充的内存，如果内存池中有足够的内存，就直接返回，如果内存池中可以分配真实所需的内存返回内存首地址
     * 如果内存不够需要重新分配内存，首先如果内存池中有剩余的内存，就把它链接到前面的自由链表中，然后分配内存
     * 用单链表记录分配内存的首地址，用于释放内存
     */
    template <bool threads, int inst>
    char *SecondMalloc<threads, inst>::ChunkAlloc(
        size_t bytes,                   // I   : 分配内存的大小
        size_t &nobjs                   // I/O : 分配内存的块数
    )
    {
        char* result;
        size_t bytesNeed = bytes * nobjs;
        size_t bytesLeft = EndFree - StartFree;

        if(bytesLeft >= bytesNeed)
        {
            result = StartFree;
            StartFree += bytesNeed;
            return result;
        }
        else if(bytesLeft >= bytes){ // 提供不了 nobjs 个块的内存
            nobjs = bytesLeft / bytes;
            bytesNeed = nobjs * bytes;
            result = StartFree;
            StartFree = StartFree + bytesNeed;
            return result; 
        }
        else
        {
            size_t bytes_to_get = 2 * bytesNeed + HeapSize;

            // 如果内存池还有剩余，把剩余的空间加入到 free list 中
            // 注意此时 bytes_left 小于 bytes， 所以会把剩余的内存池存到 前面的链表中         
            if(bytesLeft > 0){
                obj* volatile *MyList = FreeList + FreeListIndex(bytesLeft);
                ((obj*)StartFree)->next = *MyList;
                *MyList = (obj*)StartFree;
            } 

            StartFree = (char*)std::malloc(bytes_to_get);

            if(nullptr == StartFree){//堆的内存不足
                obj *volatile *MyList, *p;

                // 在自由链表中寻找空间, 且区块足够大的 free list
                for(size_t i = bytes; i <= _MAX_BYTES; i += _ALIGN){
                    MyList = FreeList + FreeListIndex(i);
                    p = *MyList;
                    if(p){
                        *MyList = p->next;
                        StartFree = (char*)p;
                        EndFree = StartFree + i;
                        return ChunkAlloc(bytes, nobjs);
                    }
                }

                EndFree = nullptr;
                //使用OOM机制分配内存
                StartFree = (char*)Malloc::Allocate(bytes_to_get);
            }

            EndFree = StartFree + bytes_to_get;
            
            // 单链表 Pool 每个节点共 8 字节大小，记录内存首地址
            ((obj*)StartFree)->next = Pool;
            Pool = (obj*)StartFree;

            // StartFree 偏移 8 字节，防止该段内存被使用
            StartFree += HeapSize;

            return ChunkAlloc(bytes, nobjs);
        }
    }   

    typedef SecondMalloc<true, 0> DefaultMalloc;

#endif
