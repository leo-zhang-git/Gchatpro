#pragma once
#include <vector>
#include <queue>
#include <future>
//#include <atomic>
//#include <condition_variable>
//#include <thread>
//#include <functional>
#include <stdexcept>
#include <iostream>


namespace std {
    class A {

    };
    constexpr int THREADPOOL_MAX_NUM = 16;

    //  �̳߳�,�����ύ��κ�������ķ����ʽ����������ִ��,���Ի�ȡִ�з���ֵ
    //  ��ֱ��֧�����Ա����, ֧���ྲ̬��Ա������ȫ�ֺ���,Opteron()������
    class threadpool
    {
    public:
        threadpool(unsigned short size = 4);
        ~threadpool();

        //  �ύһ������
        //  ����.get()��ȡ����ֵ��ȴ�����ִ����,��ȡ����ֵ
        //  �����ַ�������ʵ�ֵ������Ա��
        //  һ����ʹ��   bind�� .commit(std::bind(&Dog::sayHello, &dog));
        //  һ������   mem_fn�� .commit(std::mem_fn(&Dog::sayHello), this)
        template<class F, class... Args>
        auto commit(F&& f, Args&&... args)->future<decltype(f(args...))>;
        int idlCount();
        int thrCount();
#ifndef THREADPOOL_AUTO_GROW
    private:
#endif // !THREADPOOL_AUTO_GROW

        //  ���̳߳����ָ���������߳�
        void addThread(unsigned short size);
    private:
        using Task = function<void()>;    //  ��������
        vector<thread> _pool;           //  �̳߳�
        queue<Task> _tasks;            //  �������
        mutex _lock;                   //  ͬ��
        condition_variable _task_cv;   //  ��������
        atomic<bool> _run{ true };     //  �̳߳��Ƿ�ִ��
        atomic<int>  _idlThrNum{ 0 };  //  �����߳�����
    };
}

template<class F, class... Args>
auto std::threadpool::commit(F&& f, Args&&... args)->future<decltype(f(args...))>
{
    if (!_run)    // stoped ??
        throw runtime_error("commit on ThreadPool is stopped.");

    using RetType = decltype(f(args...)); // typename std::result_of<F(Args...)>::type, ���� f �ķ���ֵ����
    auto task = make_shared<packaged_task<RetType()> >(
        bind(forward<F>(f), forward<Args>(args)...)
        ); // �Ѻ�����ڼ�����,���(��)
    future<RetType> future = task->get_future();
    {    // ������񵽶���
        lock_guard<mutex> lock{ _lock };//�Ե�ǰ���������  lock_guard �� mutex �� stack ��װ�࣬�����ʱ�� lock()��������ʱ�� unlock()
        _tasks.emplace([task]() { // push(Task{...}) �ŵ����к���
            (*task)();
            });
    }
#ifdef THREADPOOL_AUTO_GROW
    if (_idlThrNum < 1 && _pool.size() < THREADPOOL_MAX_NUM)
        addThread(1);
#endif // !THREADPOOL_AUTO_GROW
    _task_cv.notify_one(); // ����һ���߳�ִ��

    return future;
}
