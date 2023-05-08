#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <ctime>
#include <vector>
#include <queue>
#include <future>
#include <mutex>
#include <queue>
#include <functional>
#include <future>
#include <thread>
#include <utility>
#include <vector>
#include <condition_variable>
#include <string>
#include <shared_mutex>

template<typename T>
class safe_queue {

public:
    bool empty() {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return que_.empty();
    }

    auto size() {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return que_.size();
    }

    void push(T &t) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        que_.push(t);
    }

    bool pop(T &t) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (que_.empty())return false;
        t = std::move(que_.front());
        que_.pop();
        return true;
    }

private:
    std::queue<T> que_;
    std::shared_mutex mutex_;
};

class ThreadPool {
private:
    class worker {
    public:
        ThreadPool *pool;

        explicit worker(ThreadPool *_pool) : pool{_pool} {}

        void operator()() {
            while (!pool->is_shut_down_) {
                {
                    std::unique_lock<std::mutex> lock(pool->mutex_);
                    pool->cv_.wait(lock, [this]() {
                        return this->pool->is_shut_down_ ||
                               !this->pool->que_.empty();
                    });
                }
                std::function<void()> func;
                bool flag = pool->que_.pop(func);
                if (flag) {
                    func();
                }
            }
        }
    };

public:
    explicit ThreadPool(int n) : threads_(n), is_shut_down_{false} {
        for (auto &t: threads_)
            t = std::thread{worker(this)};
    }

    ThreadPool(const ThreadPool &) = delete;

    ThreadPool(ThreadPool &&) = delete;

    ThreadPool &operator=(const ThreadPool &) = delete;

    ThreadPool &operator=(ThreadPool &&) = delete;

    template<typename F, typename... Args>
    auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
        std::function<decltype(f(args...))()> func = [&f, args...]() { return f(args...); };
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);
        std::function<void()> warpper_func = [task_ptr]() {
            (*task_ptr)();
        };
        que_.push(warpper_func);
        cv_.notify_one();
        return task_ptr->get_future();
    }

    ~ThreadPool() {
        auto f = submit([]() {});
        f.get();
        is_shut_down_ = true;
        cv_.notify_all();
        for (auto &t: threads_) {
            if (t.joinable()) t.join();
        }
    }

private:
    bool is_shut_down_;
    safe_queue<std::function<void()>> que_;
    std::vector<std::thread> threads_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

std::mutex m;

int main() {
    using namespace std;
    ThreadPool pool(8);
    int n = 20;
    for (int i = 1; i <= n; i++) {
        pool.submit([](int id) {
            if (id % 2 == 1) {
                this_thread::sleep_for(1s);
            }
            unique_lock<mutex> lock(m);
            cout << "id : " << id << endl;
        }, i);
    }
}