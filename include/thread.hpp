#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>

class Thread_pool
{
public:
	Thread_pool(unsigned int n);
	template<class F, class... Args>
		auto enqueue(F&& task, Args&&... args)-> std::future<std::invoke_result_t<F, Args...>>;

	~Thread_pool();
private:
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;

	std::mutex mtx;
	std::condition_variable cv;
	bool stop = false;
};

inline Thread_pool::~Thread_pool()
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        stop = true;
    }

    cv.notify_all();

    for(auto &worker : workers)
        worker.join();
}

inline Thread_pool::Thread_pool(unsigned int n)
{
	while(n--)
	{
		workers.emplace_back(
			[this]
			{
				for(;;)
				{
					std::function<void()> task;

					std::unique_lock<std::mutex> lock(this->mtx);
					this->cv.wait(lock, [this]{return !tasks.empty() || stop;});
					if(stop && tasks.empty()){return;}
					task = std::move(tasks.front());
					tasks.pop();



					lock.unlock();

					task();
				}
			}
		);
	}
}

template<class F, class... Args>
auto Thread_pool::enqueue(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    using return_type = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(
            std::forward<F>(f),
            std::forward<Args>(args)...
        )
    );

    std::future<return_type> result = task->get_future();

    {
        std::lock_guard<std::mutex> lock(mtx);

        tasks.emplace(
            [task]()
            {
                (*task)();
            }
        );
    }

    cv.notify_one();

    return result;
}
