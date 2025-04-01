#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

const int NUM_THREADS = 4;
int SIZE;
std::vector<int> data;

void generate_data() {
    data.resize(SIZE);
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
    std::cout << "Enter the size of the dataset: ";
    std::cin >> SIZE;
    generate_data();

    int sum, min_value;
    auto start = std::chrono::high_resolution_clock::now();
    sequential(sum, min_value);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Sequential: Sum = " << sum << ", Min = " << min_value
        << ", Time = " << std::chrono::duration<double>(end - start).count() << " ms\n";

    // Версія з м’ютексом
    sum = 0, min_value = INT_MAX;
    start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    int chunk_size = SIZE / NUM_THREADS;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(mutex_version, std::ref(sum), std::ref(min_value), i * chunk_size, (i + 1) * chunk_size);
    }
    for (auto& t : threads) t.join();
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Mutex: Sum = " << sum << ", Min = " << min_value
        << ", Time = " << std::chrono::duration<double>(end - start).count() << " ms\n";

    // Версія з CAS (cmpxchg)
    atomic_sum = 0;
    atomic_min = INT_MAX;
    start = std::chrono::high_resolution_clock::now();
    threads.clear();
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(cas_version, i * chunk_size, (i + 1) * chunk_size);
    }
    for (auto& t : threads) t.join();
    end = std::chrono::high_resolution_clock::now();
    std::cout << "CAS: Sum = " << atomic_sum.load() << ", Min = " << atomic_min.load()
        << ", Time = " << std::chrono::duration<double>(end - start).count() << " ms\n";
    return 0;
}
