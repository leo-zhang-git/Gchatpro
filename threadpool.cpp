#include "threadpool.h"

std::threadpool::threadpool(unsigned short size) 
{
	addThread(size);
}

std::threadpool::~threadpool() 
{
    _run = false;
    _task_cv.notify_all(); // ���������߳�ִ��
    for (thread& thread : _pool) {
        //thread.detach(); // ���̡߳���������
        if (thread.joinable())
            thread.join(); // �ȴ���������� ǰ�᣺�߳�һ����ִ����
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
    {   //�����߳�����,�������� Ԥ�������� THREADPOOL_MAX_NUM
        _pool.emplace_back([this] { //�����̺߳���
            while (_run)
            {
                Task task; // ��ȡһ����ִ�е� task
                {
                    // unique_lock ��� lock_guard �ĺô��ǣ�������ʱ unlock() �� lock()
                    unique_lock<mutex> lock{ _lock };
                    _task_cv.wait(lock, [this] {
                        return !_run || !_tasks.empty();
                        }); // wait ֱ���� task
                    if (!_run && _tasks.empty())
                        return;
                    task = move(_tasks.front()); // ���Ƚ��ȳ��Ӷ���ȡһ�� task
                    _tasks.pop();
                }
                _idlThrNum--;
                task();//ִ������
                _idlThrNum++;
            }
            });
        _idlThrNum++;
    }
}