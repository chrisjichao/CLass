#include <functional>
#include <atomic>
#include <condition_variable> 
#include <vector>

class Timer 
{
public:
	typedef std::function<void(void*)> TimerTask;

private://类中类
	class TimerTaskWrapper
	{
	public:
		TimerTaskWrapper(Timer::TimerTask task, unsigned long timePoint, void* param)
		: _timePoint(timePoint), _task(task), _param(param) 
		{}
		virtual ~TimerTaskWrapper() {}
		unsigned long getPoint() const { return _timePoint; }
		void setPoint(unsigned long point) { _timePoint = point; }
		void* getParam() const { return _param; }
		void run() const { _task(_param); }

	private:
		unsigned long _timePoint;
		Timer::TimerTask _task;//到时间后执行这个函数
		void* _param;
	};


public:
	Timer() : _stopFlag(false) 
	{
		_looper = std::thread(std::bind(&Timer::run, this));
	}
	virtual ~Timer() {}
	void start(TimerTask task, long long timePoint, void* param) 
	{
		std::lock_guard<std::mutex> guard(_queueMutex);
		
		_queue.emplace_back(task, timePoint, param);
		if (1 == _queue.size()) 
		{
			std::unique_lock <std::mutex> lock(_cvMutex);
			_queueCV.notify_all();
		}
	}
	void stop() 
	{
		_stopFlag = true;
	}
	void join() 
	{
		if (_looper.joinable()) 
			_looper.join();
	}

private:
	TimerTaskWrapper popMinTask() 
	{
		std::lock_guard<std::mutex> guard(_queueMutex);
		int minIndex = 0;
		for (int i = 1; i < _queue.size(); ++i)//在这里延时
		{
			if (_queue.at(minIndex).getPoint() > _queue.at(i).getPoint()) 
			{
				minIndex = i;
			}
		}
		auto task = _queue.at(minIndex);
		_queue.erase(_queue.begin() + minIndex);
		return task;
	}

	void run() 
	{
		while (!_stopFlag) 
		{
			{
				std::unique_lock <std::mutex> lock(_cvMutex);
				while (_queue.empty()) _queueCV.wait(lock);
			}
			auto task = popMinTask();
			{
				std::unique_lock <std::mutex> lock(_timeoutMutex);
				_timeoutCV.wait_for(lock, std::chrono::milliseconds(task.getPoint()));
			}
			task.run();
		}
	}

private:
	std::atomic_bool _stopFlag;
	std::mutex _queueMutex;
	std::mutex _cvMutex;
	std::mutex _timeoutMutex;
	std::thread _looper;
	std::condition_variable _queueCV;
	std::condition_variable _timeoutCV;
	std::vector<TimerTaskWrapper> _queue;
};
