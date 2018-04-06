// Just a simple single-threaded queue.
class StdQueue
    : public IQueue
{
public:
    void Push(uint64_t value) override
    {
        queue_.push(value);
    }

    std::pair<bool, uint64_t> Pop() override
    {
        if (queue_.empty()) {
            return std::make_pair(false, 0);
        }

        auto value = queue_.front();
        queue_.pop();

        return std::make_pair(true, value);
    }

private:
    std::queue<uint64_t> queue_;
};

// Just a simple multi-threaded lock-based queue.
class StdQueueWithMutex
    : public IQueue
{
public:
    void Push(uint64_t value) override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        queue_.Push(value);
    }

    std::pair<bool, uint64_t> Pop() override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return queue_.Pop();
    }

private:
    std::mutex mutex_;
    StdQueue queue_;
};

using TheQueue = StdQueueWithMutex;

