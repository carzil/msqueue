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

using TheQueue = StdQueue;


