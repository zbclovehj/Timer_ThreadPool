#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

namespace toys {

	using TaskFunc = std::function<void()>;
    struct Task
    {
        uint64_t id;       // �����Ψһ��ʶ��
        uint64_t period;   // ����ִ�е�ʱ��
        bool repeated;     // �����Ƿ��ظ�ִ��
        TaskFunc func;     // ��������ĺ�����ʹ��֮ǰ��������ͱ���
        bool removed;      // ��������Ƿ��Ƴ�

        // ���캯��
        Task(uint64_t id, uint64_t period, bool repeated, TaskFunc func)
            : id(id), period(period), repeated(repeated), func(func), removed(false)
        {
        }
    };
    class Timer {
    private:
        std::thread m_worker;//�߳��ǹ�����
        std::atomic<bool> m_stop; //ȷ�����bool���͵�ֵ�Ĳ�����ԭ�����͵�
        //����������map����ʱ����Ϊִ�еı�׼ ԭʼ���ö������洢����ΪҪʵ�ֶ�ʱ����������ʱ������־����ִ��˳��
        std::multimap<uint64_t, Task> m_tasks;//һ��ʱ�̿��ܶ�Ӧ������� ÿ��ʱ��Ҫִ�е�����
        //���ڱ����������ݣ���ֹ����߳�ͬʱ����ͬһ��Դ������ݾ�̬��
        // �ڶ��̻߳����У�������߳���Ҫ���ʺ��޸Ĺ�����Դ���� std::multimap��ʱ��ʹ�û�������ȷ��ÿ��ֻ��һ���߳̿���ִ���޸Ĳ�����
        std::mutex m_task_mutex;
        std::condition_variable m_condition;
        uint64_t m_cur_id;
        void run();
        uint64_t now();
    public:
        Timer();
        ~Timer();
        //���һ��������period_ms���ִ��func
        uint64_t add(uint64_t period_ms, bool repeated, TaskFunc func);
        bool remove(uint64_t);
       
    };
    //����ʱ��ĺ�����
    uint64_t Timer::now()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    }
    Timer::Timer() : m_stop(false) {
        //������һ���µ��̣߳����߳̽�ִ�е�ǰ���� (this) �� run ��Ա����������ζ�� run ��������һ���������߳��ϲ���ִ�С�
        m_worker = std::thread(&Timer::run, this);

    }
    //�ڶ�������ʱ��ȷ��ֹͣ�������̨�߳�
    Timer::~Timer()
    {
        m_stop.store(true);//ʹ�� store ������Ϊ����ȷ��ʾ����һ��д������������ bool ������ֱ�Ӹ�ֵͨ��Ҳ�ǰ�ȫ�ġ�
        m_condition.notify_all();//�������п������ڵȴ������������߳�
        m_worker.join();//ȷ���߳����
    }
    uint64_t Timer::add(uint64_t period_ms, bool repeated, TaskFunc func) {
        uint64_t when = now() + period_ms;  // ��������Ӧ��ִ�е�ʱ���
        Task task(m_cur_id, period_ms, repeated, func);  // ��������ʵ��

        {
            //�� std::lock_guard ���Զ����������������ͽ�����ȷ�����޸� m_tasks ӳ��ʱ���ᷢ�����ݾ�����
            std::lock_guard<std::mutex> lock(m_task_mutex);  // ʹ�û��������������б�
            m_tasks.insert({ when, task });  // ��������ӵ�������ӳ����
        }

        m_condition.notify_all();  // ֪ͨ���еȴ����̼߳���������
        return m_cur_id++;  // ���ص�ǰ����ID��������ID�Ա��´�ʹ��
    }
    bool Timer::remove(uint64_t id) {
        bool flag = false;
        std::lock_guard<std::mutex> lock(m_task_mutex);
        //ɾ��ʱ�������id = id������ ֻ�Ƴ�һ������
        std::multimap<uint64_t, Task>::iterator it = std::find_if(m_tasks.begin(), m_tasks.end(),
            [id](const std::pair<uint64_t, Task>& item) ->bool {return item.second.id == id; }
        );
        if (it != m_tasks.end()) {
            it->second.removed = true;//�������ɾ��
            flag = true;
        }
        return flag;//���ɾ���Ƿ�ɹ�

    }



}