#include "tasksys.h"
#include <iostream>


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): 
    ITaskSystem(num_threads), 
    numThreads(num_threads), 
    /*threadPool(std::make_unique<std::vector<std::thread>>())--c++11 does not have make_unique, only c++14 later has*/
    threadPool(new std::vector<std::thread>)  {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    // for smart pointer, using ->
    threadPool->reserve(num_threads);

}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    // for (int i = 0; i < num_total_tasks; i++) {
    //     runnable->runTask(i, num_total_tasks);
    // }

    std::atomic<int> taskIndex(0); // atomic variable shared by all threads, 
                                   // if it reaches num_total_tasks, then all assigned tasks are distributed and done by assigned threads

    // runnable represents the test class, runTask is a function of this class
    auto threadTask = [&]() {
        while (true) {
            int i = taskIndex.fetch_add(1);
            if (i >= num_total_tasks) break;
            runnable->runTask(i, num_total_tasks);
        }
    };

    for (int i = 0; i < numThreads; ++i) {
        threadPool->emplace_back(threadTask);
    }
    
    for (auto& thread : *threadPool) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    threadPool->clear();
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

// Function that lets each threads wait until tasks come in to notify them
// void TaskSystemParallelThreadPoolSpinning::workerThreadLoop(int threadId) {
//     while (true) {
//         std::function<void()> task;
//         {   
//             if (!threadQueues[threadId].empty()) {
//                 task = std::move(threadQueues[threadId].front());
//                 threadQueues[threadId].pop();
//             } 
//             else {
//                 std::unique_lock<std::mutex> lock(taskMutex);
//                 if (done) {
//                     break;
//                 }
//                 taskCondition.wait(lock, [this, threadId]() {
//                     return done || !threadQueues[threadId].empty();
//                 });
//             }
//         }

//         // let the thread execute this task if it is available
//         if (task)
//         {   
            
//             task();
//             std::unique_lock<std::mutex> lock(taskMutex);
//             --activeTasks;
//             if (activeTasks == 0) {
//                 taskCompleteCondition.notify_all();
//             }
//         }
//     }
// }

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): 
        ITaskSystem(num_threads), 
        numThreads(num_threads),
        stopFlag(false),
        currentTaskId(0),
        completedTasks(0),
        runnable(nullptr),
        totalTasks(0){
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    threadPool.reserve(numThreads);
    // std::cout << "Enter run function" << std::endl; no io output since python script does not output it
    for (int i = 0; i < numThreads; ++i) {
        threadPool.emplace_back([this] {
            while (true) { // Calling constructor would let the thread all run while (true) but does not execute the code below 
                            // until the run function is called, which assigns runnable and taskId
                IRunnable* currentRunnable = nullptr;
                int taskId = -1;
                {
                    std::lock_guard<std::mutex> lock(taskMutex);
                    if (runnable && currentTaskId < totalTasks) {
                        taskId = currentTaskId++;
                        currentRunnable = runnable;
                        if (currentTaskId >= totalTasks) {
                            runnable = nullptr;
                        }
                    }
                }

                if (currentRunnable) { // if there is assigned task, then run them, and increase completedTasks
                    currentRunnable->runTask(taskId, totalTasks);
                    completedTasks.fetch_add(1);
                }
                else if (stopFlag) {
                    break;
                }
                
            }
        });
    }
    
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    // if destructor is called, stop running and assining, end the loop
    stopFlag.store(true);
    for (auto& thread : threadPool) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    // for (int i = 0; i < num_total_tasks; i++) {
    //     runnable->runTask(i, num_total_tasks);
    // }
    // int tasksPerThread = (num_total_tasks + threadPool.size() - 1) / threadPool.size();

    {   // assign tasks to threads
        std::lock_guard<std::mutex> lock(taskMutex);
        this->runnable = runnable;
        this->totalTasks = num_total_tasks;
        this->currentTaskId = 0;
        this->completedTasks = 0;
    }

    // after assigning, notifying all threads
    while (completedTasks.load() < num_total_tasks) {
        
    }
}


TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): 
        ITaskSystem(num_threads),
        numThreads(num_threads),
        currentRunnable(nullptr),
        stopFlag(false)
{
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    threadPool.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        threadPool.emplace_back([this] () {
            while (true) {
                IRunnable *runnable = nullptr;
                int taskId;
                // while (currentTaskId && currentTaskId >= totalTasks) { // it does reach here, which means there is a point that currentTaskId >= totalTasks
                //     std::cout << "it reaches here" << std::endl;
                // }
                if (totalTasks != 0 && currentTaskId != 0 && currentTaskId >= totalTasks ) { //这个判断条件应该错了
                    completeAll.notify_one();
                    break;
                }
                // while (currentTaskId && currentTaskId >= totalTasks) { // it does not reach here
                //     std::cout << "it reaches here" << std::endl;
                // }
                // while (currentTaskId > 0) { // reaches here 说明分配发生了
                //     std::cout << "it reaches here" << std::endl;
                // }

                std::unique_lock<std::mutex> grd(taskMutex);
                taskAvailable.wait(grd, [this](){
                    // while (true) { //it does reach here, 说明notifyall()是成功的，但是只有部分成功了，有几个threadusage非常低，说明有threads没有接收到信息
                    //     std::cout << "aaaaaa";
                    // }
                    return currentRunnable != nullptr;});
                
                currentRunnable->runTask(currentTaskId, totalTasks);
                ++currentTaskId;

            }

            // while (true) { looks like the threads immediately reach here without running tasks
            //     std::cout << "aaaaaaaaaa";
            // }
        });
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    // while (true) it does not reach here, so there are tasks running in other threads, but the threads are instead sleeping
    // {   
    //     std::cout << "It reaches";
    //     std::cout << "It reaches";
    //     std::cout << "It reaches";
    //     std::cout << "It reaches";
    //     std::cout << "It reaches";
    //     std::cout << "It reaches"; std::cout << "It reaches";
    //     std::cout << "It reaches"; std::cout << "It reaches";
    //     std::cout << "It reaches";
    //     std::cout << "It reaches";
    //     std::cout << "It reaches"; std::cout << "It reaches";
    // }
    
    for (auto& thread : threadPool) {
        thread.join();
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //    for (int i = 0; i < num_total_tasks; i++) {
    //     runnable->runTask(i, num_total_tasks);
    // }
    {
        std::unique_lock<std::mutex> lock(taskMutex);
        currentRunnable = runnable; 
        totalTasks = num_total_tasks;
        currentTaskId = 0;
        taskAvailable.notify_all();
    }
    


    std::unique_lock<std::mutex> lock(taskMutex);
    completeAll.wait(lock, [this] () {
        return currentTaskId >= totalTasks;
    });

    // while (true) {
    //     std::cout << "it reaches here" << std::endl;
    // } It reaches here but thread are not performing tasks
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
