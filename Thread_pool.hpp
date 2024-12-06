#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <iostream>
namespace toys {
	using TaskFunc = std::function<void()>;
	

	class ThreadPool {
	private:
		std::vector<std::thread> m_worker;//线程是工作者去执行任务的
		std::queue<TaskFunc> m_tasks;//任务队列
		std::mutex m_task_mutex;//互斥锁
		std::condition_variable m_condition;
		std::atomic<bool> m_stop;
		void working();
	public:
		ThreadPool(int num_worker);
		~ThreadPool();
		//添加一个任务到任务队列中，并返回一个 std::future 对象，该对象可以用来获取任务的执行结果。
		template <typename F, typename... Args>
		auto add(F&& f, Args &&...args)->std::future<typename std::result_of<F(Args...)>::type>;

	};
	//这个方法用于直接在 m_worker（通常是一个 std::vector<std::thread> 类型的容器）中构造新的 std::thread 对象。
	//这里传递了 ThreadPool::working 作为线程函数，并将 this 指针作为参数传递给 working 函数，确保线程可以访问类的成员变量和函数。
	ThreadPool::ThreadPool(int num_worker) : m_stop(false) {
		for (int i = 0; i < num_worker; i++)
		{
			//std::thread t(&ThreadPool::working, this);
			//m_worker.push_back(t);
			//emplace_back 函数则是直接在容器的末尾构造元素(线程)，它可以接受任意数量和类型的参数
			m_worker.emplace_back(&ThreadPool::working, this);
		}
	}
	ThreadPool::~ThreadPool() {
		m_stop.store(true);//终止所有线程
		m_condition.notify_all();//终止所有线程
		for (std::thread& worker : m_worker) {
			worker.join();//回收线程
		}
	}
	void ThreadPool::working() {
		while (true)
		{
			std::unique_lock<std::mutex> lock(m_task_mutex);
			/*
1.锁管理：在调用 wait 之前，必须通过 std::unique_lock 锁定互斥锁。wait 调用时会自动释放这个锁，允许其他线程获得锁以修改条件（例如添加任务到队列中）。
2. 等待条件：线程在调用 wait 后会阻塞，直到其他线程对条件变量调用 notify_one 或 notify_all。
这时，阻塞的线程会被唤醒来重新检查条件函数。如果条件函数返回 true，则 wait 结束并继续执行；如果条件函数返回 false，线程会再次阻塞。
3. 重新获得锁：当线程被唤醒并且条件函数返回 true 后，wait 会在返回之前重新获得互斥锁。这确保了线程在继续执行后续代码之前，状态是受保护的。*/
//进入阻塞队列，操作系统的实际调度线程会根据优先级，先后请求等策略来调度
			m_condition.wait(lock, [this]()->bool {return !m_tasks.empty() || m_stop; });
			if (m_stop) {
				break;//终止所有线程
			}
			TaskFunc task(std::move(m_tasks.front()));
			m_tasks.pop();
			lock.unlock();
			//线程并发执行任务，只有在对任务队列操作时要加锁
			task();
		}
	}

	template <typename F, typename... Args>
	auto ThreadPool::add(F&& f, Args &&...args)->std::future<typename std::result_of<F(Args...)>::type> {
		using ReturnType = typename std::result_of<F(Args...)>::type;
		//任务封装：使用 std::packaged_task 封装一个可调用对象和其参数。
		//std::packaged_task 对象可以将函数调用的结果存储在一个 std::future 对象中，这使得可以异步获取函数的返回值。
		//参数绑定：使用 std::bind 和 std::forward 来正确地绑定函数和其参数，同时保持参数的完美转发（Perfect Forwarding）。这确保了函数参数的正确传递和优化。
		auto pkgt_ptr = std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		std::future<ReturnType> result = pkgt_ptr->get_future();
		{
			std::lock_guard<std::mutex> lock(m_task_mutex);
			//添加任务
			m_tasks.emplace([pkgt_ptr]() {(*pkgt_ptr)(); });
		}
		//通过 m_condition.notify_one() 唤醒一个正在等待的工作线程，让它执行新添加的任务。
		m_condition.notify_one();
		return result;
	}

	
}

int func1(int a, int b)
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
	return a + b;
}

void func2(int& v)
{
	v += 1;
}

int main(int argc, char** argv)
{
	toys::ThreadPool tp(3);
	int v = 3;
	auto f1 = tp.add(&func1, 9, 6);
	auto f2 = tp.add(&func2, std::ref(v));
	auto f3 = tp.add(&func1, 9, 6);
	auto f4 = tp.add(&func1, 9, 6);
	auto f5 = tp.add(&func1, 9, 6);
	auto f6 = tp.add(&func1, 9, 6);
	f2.wait();
	std::cout << v << std::endl;
	std::cout << f1.get() << std::endl;
	std::cout << f3.get() << std::endl;
	std::cout << f4.get() << std::endl;
	std::cout << f5.get() << std::endl;
	std::cout << f6.get() << std::endl;
	return 0;
}