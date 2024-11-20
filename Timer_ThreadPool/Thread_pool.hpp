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

	};

}