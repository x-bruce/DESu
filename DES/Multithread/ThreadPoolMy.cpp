// ThreadPoolMy.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "ThreadPoolMy.h"


/*-------------------------------------------WORKER------------------------------------------------------*/

Worker::Worker(ThreadPoolMy* _parent)
	: parent_{ _parent }, free_{ true }
{
}

/*-------------------------------------------------------------------------------------------------------*/

/*
While run, expects tasks ant processes them.
Waits for parent's pool queue population.
For correct termination using such ugly thing as fake tasks, that we are pushing into the queue in terminate_all function.
*/
void Worker::run()
{
	while (true)
	{
		function_wrapper task;
		parent_->working_queue.wait_and_pop(task);
		if (pool()->is_terminated())
		{
			parent_->_size--;
			return;
		}

		free_.store(false);
		parent_->busy_workers_count.fetch_add(1);
		// if there are exception, we will intercept it with future
		task();
		--parent_->not_done_tasks;
		parent_->busy_workers_count.fetch_sub(1);
		free_.store(true);
	}
}

/*-----------------------------------------ThreadPool---------------------------------------------------*/

ThreadPoolMy::ThreadPoolMy(ThreadPoolMy::size_type n)
	:_size{ n }, working_queue{}, busy_workers_count{ 0 }, terminated_{ false }, not_done_tasks{ 0 }
{
	try
	{
		for (size_type i = 0; i < n; ++i)
		{
			std::shared_ptr<Worker> uptrw{ new Worker(this) };
			workers.push_back(uptrw);
			threads.push_back(std::thread(&Worker::run, uptrw.get()));
		}
	}
	catch (...)
	{
		terminate_all();
	}
}

/*-------------------------------------------------------------------------------------------------------*/

ThreadPoolMy::~ThreadPoolMy()
{
	terminate_all();
}

/*-------------------------------------------------------------------------------------------------------*/

void ThreadPoolMy::join_threads()
{
	for (int i = 0; i < threads.size(); ++i)
	{
		threads[i].join();
	}
}

/*-------------------------------------------------------------------------------------------------------*/

/*
Use this if you want to wait all tasks to complete before processing later.
*/
void ThreadPoolMy::wait_all_tasks()
{
	// blocking adding new tasks in queue
	std::lock_guard<std::mutex> add_lck{ add_mtx };
	// using active wait - well, it is anyways blocking call, so we can let it be this way
	while (not_done_tasks.load())
	{
		std::this_thread::yield();
	}
}

/*-------------------------------------------------------------------------------------------------------*/

// correctly(?) terminating all workers
void ThreadPoolMy::terminate_all()
{
	terminated_ = true;
	//fake tasks

	while (_size)
	{
		for (int i = 0; i < _size; ++i)
		{
			working_queue.push(function_wrapper());
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	join_threads();
}