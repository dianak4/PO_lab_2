#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

const int SIZE = 10000000;
std::vector<int> data(SIZE);

void generate_data() {
    for (int i = 0; i < SIZE; ++i) {
        data[i] = rand() % 10000;  // Генеруємо випадкові числа від 0 до 9999
    }
}

// Послідовний варіант
void sequential(int& sum, int& min_value) {
    sum = 0;
    min_value = INT_MAX;
    for (int num : data) {
        if (num % 10 == 0) {
            sum += num;
            if (num < min_value) {
                min_value = num;
            }
        }
    }
}

// Варіант з м’ютексом
std::mutex mtx;
void mutex_version(int& sum, int& min_value, int start, int end) {
    int local_sum = 0;
    int local_min = INT_MAX;
    for (int i = start; i < end; ++i) {
        if (data[i] % 10 == 0) {
            local_sum += data[i];
            if (data[i] < local_min) {
                local_min = data[i];
            }
        }
    }
    std::lock_guard<std::mutex> lock(mtx);
    sum += local_sum;
    if (local_min < min_value) {
        min_value = local_min;
    }
}

// Варіант з CAS (cmpxchg)
std::atomic<int> atomic_sum(0);
std::atomic<int> atomic_min(INT_MAX);
void cas_version(int start, int end) {
    int local_sum = 0;
    int local_min = INT_MAX;
    for (int i = start; i < end; ++i) {
        if (data[i] % 10 == 0) {
            local_sum += data[i];
            if (data[i] < local_min) {
                local_min = data[i];
            }
        }
    }
    atomic_sum.fetch_add(local_sum, std::memory_order_relaxed);
    int old_min = atomic_min.load();
    while (local_min < old_min && !atomic_min.compare_exchange_weak(old_min, local_min));
}

int main() {
    generate_data();
    int sum, min_value;
    auto start = std::chrono::high_resolution_clock::now();
    sequential(sum, min_value);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Sequential: Sum = " << sum << ", Min = " << min_value
        << ", Time = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms\n";

    // Версія з м’ютексом
    sum = 0, min_value = INT_MAX;
    start = std::chrono::high_resolution_clock::now();
    std::thread t1(mutex_version, std::ref(sum), std::ref(min_value), 0, SIZE / 2);
    std::thread t2(mutex_version, std::ref(sum), std::ref(min_value), SIZE / 2, SIZE);
    t1.join();
    t2.join();
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Mutex: Sum = " << sum << ", Min = " << min_value
        << ", Time = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms\n";

    // Версія з CAS (cmpxchg)
    atomic_sum = 0;
    atomic_min = INT_MAX;
    start = std::chrono::high_resolution_clock::now();
    std::thread t3(cas_version, 0, SIZE / 2);
    std::thread t4(cas_version, SIZE / 2, SIZE);
    t3.join();
    t4.join();
    end = std::chrono::high_resolution_clock::now();
    std::cout << "CAS: Sum = " << atomic_sum.load() << ", Min = " << atomic_min.load()
        << ", Time = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms\n";
    return 0;
}
