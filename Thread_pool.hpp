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
		std::vector<std::thread> m_worker;//�߳��ǹ�����ȥִ�������
		std::queue<TaskFunc> m_tasks;//�������
		std::mutex m_task_mutex;//������
		std::condition_variable m_condition;
		std::atomic<bool> m_stop;
		void working();
	public:
		ThreadPool(int num_worker);
		~ThreadPool();
		//���һ��������������У�������һ�� std::future ���󣬸ö������������ȡ�����ִ�н����
		template <typename F, typename... Args>
		auto add(F&& f, Args &&...args)->std::future<typename std::result_of<F(Args...)>::type>;

	};
	//�����������ֱ���� m_worker��ͨ����һ�� std::vector<std::thread> ���͵��������й����µ� std::thread ����
	//���ﴫ���� ThreadPool::working ��Ϊ�̺߳��������� this ָ����Ϊ�������ݸ� working ������ȷ���߳̿��Է�����ĳ�Ա�����ͺ�����
	ThreadPool::ThreadPool(int num_worker) : m_stop(false) {
		for (int i = 0; i < num_worker; i++)
		{
			//std::thread t(&ThreadPool::working, this);
			//m_worker.push_back(t);
			//emplace_back ��������ֱ����������ĩβ����Ԫ��(�߳�)�������Խ����������������͵Ĳ���
			m_worker.emplace_back(&ThreadPool::working, this);
		}
	}
	ThreadPool::~ThreadPool() {
		m_stop.store(true);//��ֹ�����߳�
		m_condition.notify_all();//��ֹ�����߳�
		for (std::thread& worker : m_worker) {
			worker.join();//�����߳�
		}
	}
	void ThreadPool::working() {
		while (true)
		{
			std::unique_lock<std::mutex> lock(m_task_mutex);
			/*
1.�������ڵ��� wait ֮ǰ������ͨ�� std::unique_lock ������������wait ����ʱ���Զ��ͷ�����������������̻߳�������޸�����������������񵽶����У���
2. �ȴ��������߳��ڵ��� wait ���������ֱ�������̶߳������������� notify_one �� notify_all��
��ʱ���������̻߳ᱻ���������¼��������������������������� true���� wait ����������ִ�У���������������� false���̻߳��ٴ�������
3. ���»���������̱߳����Ѳ��������������� true ��wait ���ڷ���֮ǰ���»�û���������ȷ�����߳��ڼ���ִ�к�������֮ǰ��״̬���ܱ����ġ�*/
//�����������У�����ϵͳ��ʵ�ʵ����̻߳�������ȼ����Ⱥ�����Ȳ���������
			m_condition.wait(lock, [this]()->bool {return !m_tasks.empty() || m_stop; });
			if (m_stop) {
				break;//��ֹ�����߳�
			}
			TaskFunc task(std::move(m_tasks.front()));
			m_tasks.pop();
			lock.unlock();
			//�̲߳���ִ������ֻ���ڶ�������в���ʱҪ����
			task();
		}
	}

	template <typename F, typename... Args>
	auto ThreadPool::add(F&& f, Args &&...args)->std::future<typename std::result_of<F(Args...)>::type> {
		using ReturnType = typename std::result_of<F(Args...)>::type;
		//�����װ��ʹ�� std::packaged_task ��װһ���ɵ��ö�����������
		//std::packaged_task ������Խ��������õĽ���洢��һ�� std::future �����У���ʹ�ÿ����첽��ȡ�����ķ���ֵ��
		//�����󶨣�ʹ�� std::bind �� std::forward ����ȷ�ذ󶨺������������ͬʱ���ֲ���������ת����Perfect Forwarding������ȷ���˺�����������ȷ���ݺ��Ż���
		auto pkgt_ptr = std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		std::future<ReturnType> result = pkgt_ptr->get_future();
		{
			std::lock_guard<std::mutex> lock(m_task_mutex);
			//�������
			m_tasks.emplace([pkgt_ptr]() {(*pkgt_ptr)(); });
		}
		//ͨ�� m_condition.notify_one() ����һ�����ڵȴ��Ĺ����̣߳�����ִ������ӵ�����
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