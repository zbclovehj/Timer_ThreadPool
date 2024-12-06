#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <iostream>
namespace toys {

	using TaskFunc = std::function<void()>;
    struct Task
    {
        uint64_t id;       // 任务的唯一标识符
        uint64_t period;   // 任务执行的时间
        bool repeated;     // 任务是否重复执行
        TaskFunc func;     // 任务关联的函数，使用之前定义的类型别名
        bool removed;      // 标记任务是否被移除

        // 构造函数
        Task(uint64_t id, uint64_t period, bool repeated, TaskFunc func)
            : id(id), period(period), repeated(repeated), func(func), removed(false)
        {
        }
    };
    class Timer {
    private:
        std::thread m_worker;//线程是工作者
        std::atomic<bool> m_stop; //确保这个bool类型的值的操作是原子类型的
        //到来的任务map，以时间作为执行的标准 原始是用队列来存储，因为要实现定时器，所以用时间来标志任务执行顺序
        std::multimap<uint64_t, Task> m_tasks;//一个时刻可能对应多个任务 每个时刻要执行的任务
        //用于保护共享数据，防止多个线程同时访问同一资源造成数据竞态。
        // 在多线程环境中，当多个线程需要访问和修改共享资源（如 std::multimap）时，使用互斥锁来确保每次只有一个线程可以执行修改操作。
        std::mutex m_task_mutex;
        std::condition_variable m_condition;
        uint64_t m_cur_id;
        void run();
        uint64_t now();
    public:
        Timer();
        ~Timer();
        //添加一个任务在period_ms秒后执行func
        uint64_t add(uint64_t period_ms, bool repeated, TaskFunc func);
        bool remove(uint64_t);
       
    };
    //返回时间的毫秒数
    uint64_t Timer::now()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    }
    Timer::Timer() : m_stop(false) {
        //创建了一个新的线程，该线程将执行当前对象 (this) 的 run 成员函数。这意味着 run 函数将在一个单独的线程上并行执行。
        m_worker = std::thread(&Timer::run, this);

    }
    //在对象销毁时正确地停止和清理后台线程
    Timer::~Timer()
    {
        m_stop.store(true);//使用 store 方法是为了明确表示这是一个写操作，尽管在 bool 类型上直接赋值通常也是安全的。
        m_condition.notify_all();//唤醒所有可能正在等待条件变量的线程
        m_worker.join();//确保线程完成
    }
    uint64_t Timer::add(uint64_t period_ms, bool repeated, TaskFunc func) {
        uint64_t when = now() + period_ms;  // 计算任务应该执行的时间点
        Task task(m_cur_id, period_ms, repeated, func);  // 创建任务实例。period_ms为任务执行时间间隔

        {
            //用 std::lock_guard 来自动管理互斥锁的锁定和解锁，确保在修改 m_tasks 映射时不会发生数据竞争。
            std::lock_guard<std::mutex> lock(m_task_mutex);  // 使用互斥锁保护任务列表
            m_tasks.insert({ when, task });  // 将任务添加到多任务映射中
        }

        m_condition.notify_all();  // 通知所有等待的线程检查任务队列
        return m_cur_id++;  // 返回当前任务ID，并递增ID以备下次使用
    }
    bool Timer::remove(uint64_t id) {
        bool flag = false;
        std::lock_guard<std::mutex> lock(m_task_mutex);
        //删除时间最近，id = id的任务 只移除一个任务
        std::multimap<uint64_t, Task>::iterator it = std::find_if(m_tasks.begin(), m_tasks.end(),
            [id](const std::pair<uint64_t, Task>& item) ->bool {return item.second.id == id; }
        );
        if (it != m_tasks.end()) {
            it->second.removed = true;//标记任务被删除
            flag = true;
        }
        return flag;//标记删除是否成功

    }
    void Timer::run() {
        while (true) {
            std::unique_lock<std::mutex> lock(m_task_mutex);
            m_condition.wait(lock, [this]()->bool {return !m_stop || !m_tasks.empty(); });
            if (m_stop) {
                break;//如果停止的话，终止线程执行
            }
            uint64_t cur_time = now();
            //不断的处理时间最近的任务
            std::multimap<uint64_t, Task>::iterator it = m_tasks.begin();
            uint64_t task_time = it->first;
            if (cur_time >= task_time) {
                Task& cur_task = it->second;
                if (!cur_task.removed) {
                    lock.unlock();//解锁去执行任务
                    cur_task.func();//线程执行任务
                    lock.lock();//加锁去访问任务组
                    if (cur_task.repeated && !cur_task.removed)//当前任务加入要重复执行并且没有被移除
                    {
                        uint64_t when = cur_time + cur_task.period;//获取新任务的执行时间点，执行任务的时间间隔+执行任务时的时间
                        Task new_task(cur_task.id, cur_task.period, cur_task.repeated, cur_task.func);
                        m_tasks.insert({ when,new_task });
                    }
                }
                m_tasks.erase(it);
            }
            else
            {   //等待到任务执行时间 前面是锁，后面是条件
                m_condition.wait_for(lock, std::chrono::milliseconds(task_time - cur_time));
                std::cout << "到时间了可以开始执行任务" << std::endl;
            }
        }
    }


}


#include <iomanip>


void print_func(int id)
{
    // 获取自纪元以来的时间点
    auto now = std::chrono::system_clock::now();

    // 转换为自纪元以来的毫秒数
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    // 转换为日期和时间
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time);

    // 输出日期和时间
    std::cout << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "." << std::setw(3) << std::setfill('0') << millis % 1000
        << " id = " << id << std::endl;
}

int main(int argc, char** argv)
{
    toys::Timer* timer_p = new toys::Timer();
    //它允许你预先绑定一些参数到一个函数或函数对象上，从而生成一个新的可调用对象。这个新的可调用对象可以在之后被调用，而无需再提供那些已经绑定的参数。
    timer_p->add(2000, false, std::bind(print_func, 0));
    uint64_t id = timer_p->add(200, true, std::bind(print_func, 1));
    timer_p->add(500, true, std::bind(print_func, 2));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    timer_p->remove(id);
    std::cout << "timer_p->remove(id)" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    delete timer_p;
    std::cout << "delete timer_p" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return 0;
}