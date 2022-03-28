#include "threadpool.h"

std::threadpool::threadpool(unsigned short size) 
{
	addThread(size);
}

std::threadpool::~threadpool() 
{
    _run = false;
    _task_cv.notify_all(); // 唤醒所有线程执行
    for (thread& thread : _pool) {
        //thread.detach(); // 让线程“自生自灭”
        if (thread.joinable())
            thread.join(); // 等待任务结束， 前提：线程一定会执行完
    }
}



int std::threadpool::idlCount()
{
    return _idlThrNum;
}

int std::threadpool::thrCount()
{
    return _pool.size();
}

void std::threadpool::addThread(unsigned short size)
{
    for (; _pool.size() < THREADPOOL_MAX_NUM && size > 0; --size)
    {   //增加线程数量,但不超过 预定义数量 THREADPOOL_MAX_NUM
        _pool.emplace_back([this] { //工作线程函数
            while (_run)
            {
                Task task; // 获取一个待执行的 task
                {
                    // unique_lock 相比 lock_guard 的好处是：可以随时 unlock() 和 lock()
                    unique_lock<mutex> lock{ _lock };
                    _task_cv.wait(lock, [this] {
                        return !_run || !_tasks.empty();
                        }); // wait 直到有 task
                    if (!_run && _tasks.empty())
                        return;
                    task = move(_tasks.front()); // 按先进先出从队列取一个 task
                    _tasks.pop();
                }
                _idlThrNum--;
                task();//执行任务
                _idlThrNum++;
            }
            });
        _idlThrNum++;
    }
}