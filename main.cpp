#include "gtest/gtest.h"

#include <queue>
#include <random>
#include <thread>

// You must implmement a queue with this interface in your programming assignment.
class IQueue
{
public:
    virtual void Push(uint64_t value) = 0;
    virtual std::pair<bool, uint64_t> Pop() = 0;
};

// For simplicity, you may assume that threads are explicitly enumerated
// with indexes ranging from 0 to N.
int GetCurrentThreadIndex();
int GetMaxThreadCount();
void SetCurrentThreadIndex(int thread_index);

// Thread index tracking code.
static thread_local int this_thread_index_ = -1;
static constexpr int max_thread_count_ = 8;

int GetCurrentThreadIndex()
{
    return this_thread_index_;
}

int GetMaxThreadCount()
{
    return max_thread_count_;
}

void SetCurrentThreadIndex(int thread_index)
{
    assert(thread_index >= 0 && thread_index < max_thread_count_);
    this_thread_index_ = thread_index;
}

////////////////////////////////////////////////////////////////////////////////
// TESTS
////////////////////////////////////////////////////////////////////////////////

struct Uint64Generator
{
    std::random_device random_device_;
    std::seed_seq seed_sequence_;
    std::mt19937_64 pseudo_random_generator_;

    Uint64Generator()
        : seed_sequence_{random_device_(), random_device_(), random_device_(), random_device_()}
        , pseudo_random_generator_(seed_sequence_)
    {
    }

    uint64_t Next()
    {
        return pseudo_random_generator_();
    }
};

void StressTest(IQueue* queue, int iterations, size_t producer_thread_count, size_t consumer_thread_count)
{
    std::atomic<int> thread_indexer = {0};

    std::vector<std::thread> producer_threads;
    producer_threads.reserve(producer_thread_count);
    std::vector<uint64_t> producer_partial_xorsums(producer_thread_count, 0);

    for (size_t thread_index = 0; thread_index < producer_thread_count; ++thread_index) {
        std::thread producer(
            [queue, thread_index, iterations, producer_thread_count, &producer_partial_xorsums, &thread_indexer]() {
                SetCurrentThreadIndex(thread_indexer++);
                Uint64Generator generator;
                uint64_t partial_xorsum = 0;
                auto from = thread_index * iterations / producer_thread_count;
                auto to = (thread_index + 1) * iterations / producer_thread_count;
                for (auto iteration = from; iteration < to; ++iteration) {
                    auto value = generator.Next();
                    partial_xorsum ^= value;
                    queue->Push(value);
                }
                producer_partial_xorsums[thread_index] = partial_xorsum;
            });
        producer_threads.emplace_back(std::move(producer));
    }

    std::vector<std::thread> consumer_threads;
    consumer_threads.reserve(consumer_thread_count);
    std::vector<uint64_t> consumer_partial_xorsums(consumer_thread_count, 0);

    for (size_t thread_index = 0; thread_index < consumer_thread_count; ++thread_index) {
        std::thread consumer(
            [queue, thread_index, iterations, consumer_thread_count, &consumer_partial_xorsums, &thread_indexer]() {
                SetCurrentThreadIndex(thread_indexer++);
                uint64_t partial_xorsum = 0;
                auto from = thread_index * iterations / consumer_thread_count;
                auto to = (thread_index + 1) * iterations / consumer_thread_count;
                for (auto iteration = from; iteration < to; ++iteration) {
                    std::pair<bool, uint64_t> result;
                    do {
                        std::this_thread::yield();
                        result = queue->Pop();
                    } while (!result.first);
                    partial_xorsum ^= result.second;
                }
                consumer_partial_xorsums[thread_index] = partial_xorsum;
            });
        consumer_threads.emplace_back(std::move(consumer));
    }

    for (auto& producer : producer_threads) {
        producer.join();
    }

    for (auto& consumer : consumer_threads) {
        consumer.join();
    }

    auto producer_xorsum = std::accumulate(
        producer_partial_xorsums.begin(),
        producer_partial_xorsums.end(),
        0,
        std::bit_xor<>());
    auto consumer_xorsum = std::accumulate(
        consumer_partial_xorsums.begin(),
        consumer_partial_xorsums.end(),
        0,
        std::bit_xor<>());

    EXPECT_EQ(producer_xorsum, consumer_xorsum);
}

template <class Queue>
class QueueTest : public ::testing::Test
{
};

// XXX(sandello): Here we plug in the solution.
#include SOLUTION_FILE

typedef ::testing::Types<TheQueue> QueueTestTypes;
static constexpr size_t queue_test_iterations_ = 100000;

TYPED_TEST_CASE(QueueTest, QueueTestTypes);

TYPED_TEST(QueueTest, OneThread)
{
    SetCurrentThreadIndex(0);

    TypeParam q;
    std::pair<bool, uint64_t> r;

    q.Push(42);
    q.Push(17);

    r = q.Pop();
    EXPECT_TRUE(r.first);
    EXPECT_EQ(r.second, 42);

    r = q.Pop();
    EXPECT_TRUE(r.first);
    EXPECT_EQ(r.second, 17);

    r = q.Pop();
    EXPECT_FALSE(r.first);
}

TYPED_TEST(QueueTest, TwoThreads)
{
    TypeParam queue;
    StressTest(&queue, queue_test_iterations_, 1, 1);
}

TYPED_TEST(QueueTest, FourThreads1P3C)
{
    TypeParam queue;
    StressTest(&queue, queue_test_iterations_, 1, 3);
}

TYPED_TEST(QueueTest, FourThreads2P2C)
{
    TypeParam queue;
    StressTest(&queue, queue_test_iterations_, 2, 2);
}

TYPED_TEST(QueueTest, FourThreads3P1C)
{
    TypeParam queue;
    StressTest(&queue, queue_test_iterations_, 3, 1);
}

TYPED_TEST(QueueTest, EightThreads)
{
    TypeParam queue;
    StressTest(&queue, queue_test_iterations_, 4, 4);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

