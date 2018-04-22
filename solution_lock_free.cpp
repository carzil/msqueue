#include <atomic>
#include <algorithm>
#include <memory>

template<typename T>
static void SortUnique(std::vector<T>& v) {
    std::sort(v.begin(), v.end());
    auto end = std::unique(v.begin(), v.end());
    v.erase(end, v.end());
}

class StdQueue
    : public IQueue {
private:
    struct Node {
        std::atomic<Node*> Next;
        uint64_t Value = 0;

        Node()
            : Next(nullptr)
        {
        }

        Node(uint64_t value)
            : Next(nullptr)
            , Value(value)
        {
        }
    };

    static constexpr size_t HazardPointersPerThread = 2;
    static constexpr size_t HazardPointersMaxRetireListSize = 2 * HazardPointersPerThread * GetMaxThreadCount();

    struct RetireList {
        size_t Size;
        Node* Ptrs[HazardPointersMaxRetireListSize];
    };

public:
    StdQueue() {
        for (size_t i = 0; i < GetMaxThreadCount(); i++) {
            HazardPointers[i][0] = nullptr;
            HazardPointers[i][1] = nullptr;
            RetireLists[i].Size = 0;
        }
        Node* node = new Node();
        Tail.store(node);
        Head.store(node);
    }

    ~StdQueue() {
        std::vector<Node*> toDelete;
        for (size_t i = 0; i < GetMaxThreadCount(); i++) {
            for (size_t j = 0; j < RetireLists[i].Size; j++) {
                toDelete.push_back(RetireLists[i].Ptrs[j]);
            }
        }

        for (Node* ptr = Head; ptr != nullptr; ptr = ptr->Next) {
            toDelete.push_back(ptr);
        }

        SortUnique(toDelete);

        for (Node* ptr : toDelete) {
            delete ptr;
        }
    }

    void Push(uint64_t value) override {
        Node* node = new Node(value);
        Node* tail;
        bool added = false;
        for (;;) {
            tail = Tail;
            MarkAsHazard(tail, 0);
            if (tail != Tail) {
                continue;
            }
            Node* next = tail->Next;
            if (tail != Tail) {
                continue;
            }
            if (next != nullptr) {
                Tail.compare_exchange_weak(tail, next);
                continue;
            }
            Node* null = nullptr;
            if (tail->Next.compare_exchange_strong(null, node)) {
                added = true;
                break;
            }
        }
        Tail.compare_exchange_strong(tail, node);
        Dehazard(0);
    }

    std::pair<bool, uint64_t> Pop() override {
        Node* head = nullptr;
        uint64_t value = 0;
        bool empty = false;
        for (;;) {
            head = Head;
            MarkAsHazard(head, 0);
            if (head != Head) {
                continue;
            }
            Node* tail = Tail;
            Node* next = head->Next;
            MarkAsHazard(next, 1);
            if (head != Head) {
                continue;
            }

            if (next == nullptr) {
                empty = true;
                break;
            }

            if (head == tail) {
                Tail.compare_exchange_strong(tail, next);
                continue;
            }

            value = next->Value;
            if (Head.compare_exchange_strong(head, next)) {
                break;
            }
        }

        Dehazard(0);
        Dehazard(1);
        if (!empty) {
            RetirePtr(head);
        }

        return { !empty, value };
    }

private:
    void MarkAsHazard(Node* ptr, size_t idx) {
        HazardPointers[GetCurrentThreadIndex()][idx].store(ptr);
    }

    void Dehazard(size_t idx) {
        HazardPointers[GetCurrentThreadIndex()][idx].store(nullptr);
    }

    void RetirePtr(Node* ptr) {
        if (ptr == nullptr) {
            return;
        }
        RetireList& myRetireList = RetireLists[GetCurrentThreadIndex()];
        myRetireList.Ptrs[myRetireList.Size++] = ptr;
        if (myRetireList.Size == HazardPointersMaxRetireListSize) {
            ShrinkMyRetireList();
        }
    }

    void ShrinkMyRetireList() {
        RetireList& myRetireList = RetireLists[GetCurrentThreadIndex()];

        Node* collectedHazardPointers[GetMaxThreadCount() * HazardPointersPerThread];
        size_t collectedHazardPointersSize = 0;
        for (size_t i = 0; i < GetMaxThreadCount(); i++) {
            for (size_t j = 0; j < HazardPointersPerThread; j++) {
                Node* ptr = HazardPointers[i][j];
                if (ptr != nullptr) {
                    collectedHazardPointers[collectedHazardPointersSize++] = ptr;
                }
            }
        }

        std::sort(&collectedHazardPointers[0], &collectedHazardPointers[collectedHazardPointersSize]);

        size_t pos = 0;
        for (size_t i = 0; i < myRetireList.Size; i++) {
            Node* ptr = myRetireList.Ptrs[i];
            if (!std::binary_search(&collectedHazardPointers[0], &collectedHazardPointers[collectedHazardPointersSize], ptr)) {
                delete ptr;
            } else {
                myRetireList.Ptrs[pos++] = ptr;
            }
        }
        myRetireList.Size = pos;
    }

    RetireList RetireLists[GetMaxThreadCount()];
    std::atomic<Node*> HazardPointers[GetMaxThreadCount()][HazardPointersPerThread];
    std::atomic<Node*> Head;
    std::atomic<Node*> Tail;
};

using TheQueue = StdQueue;
