#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

using namespace std;

const vector<int> NUM_THREADS = { 4, 8, 16, 32, 64, 128, 320 };
int SIZE;
vector<int> dataset;
mutex mtx;
atomic<long long> atomic_sum(0);
atomic<int> atomic_min(INT_MAX);

void generate_dataset() {
    dataset.resize(SIZE);
    for (int i = 0; i < SIZE; ++i) {
        dataset[i] = rand() % 1000;  // Генеруємо випадкові числа від 0 до 9999
    }
}

// Послідовний варіант
void sequential(int& sum, int& min_value) {
    sum = 0;
    min_value = INT_MAX;
    for (int num : dataset) {
        if (num % 10 == 0) { // чи кратне 10 як треба за умовою
            sum += num;
            if (num < min_value) {
                min_value = num;
            }
        }
    }
}

// Варіант з м’ютексом
void mutex_version(int& sum, int& min_value, int start, int end) { // виконує кожен потік
    long long local_sum = 0;
    int local_min = INT_MAX;
    for (int i = start; i < end; ++i) {
        if (dataset[i] % 10 == 0) {
            local_sum += dataset[i];
            if (dataset[i] < local_min) {
                local_min = dataset[i];
            }
        }
    }
    lock_guard<mutex> lock(mtx); // тільки один потік може модифікувати sum і min_value
    sum += local_sum;
    if (local_min < min_value) {
        min_value = local_min;
    }
}

// Варіант з CAS (cmpxchg)
void cas_version(int start, int end) {
    int local_sum = 0;
    int local_min = INT_MAX;
    for (int i = start; i < end; ++i) {
        if (dataset[i] % 10 == 0) {
            local_sum += dataset[i];
            if (dataset[i] < local_min) {
                local_min = dataset[i];
            }
        }
    }
    atomic_sum.fetch_add(local_sum, memory_order_relaxed); // кожен потік додає свою суму в глобальну. не залежить від порядку
    int old_min = atomic_min.load(); //  копія старого мінімуму з пам'яті
    while (local_min < old_min && !atomic_min.compare_exchange_weak(old_min, local_min)); //if atomic_min = old_min -> local_min else: refresh old_min 
}

int main() {
    cout << "Enter the size of the datasetset: ";
    cin >> SIZE;
    generate_dataset();

    int sum, min_value;
    auto start = chrono::high_resolution_clock::now();
    sequential(sum, min_value);
    auto end = chrono::high_resolution_clock::now();
    cout << "Sequential: Sum = " << sum << ", Min = " << min_value
        << ", Time = " << chrono::duration<double>(end - start).count() << " seconds\n";

    cout << "\n" << endl;

    // Версія з м’ютексом
    
    for (int num_threads : NUM_THREADS) {
        sum = 0, min_value = INT_MAX;
        start = chrono::high_resolution_clock::now();
        vector<thread> threads;
        int chunk_size = SIZE / num_threads;
        for (int i = 0; i < num_threads; ++i) {
            int start_idx = i * chunk_size;
			int end_idx = (i == num_threads - 1) ? SIZE : (i + 1) * chunk_size; // останній потік обробляє залишок
            threads.emplace_back(mutex_version, ref(sum), ref(min_value), start_idx, end_idx);
        }
        for (auto& t : threads) t.join();
        end = chrono::high_resolution_clock::now();
        cout << "Mutex \t|" << num_threads << " threads|: \tSum = " << sum << ", Min = " << min_value
            << ", Time = " << chrono::duration<double>(end - start).count() << " seconds\n";
    }

	cout << "\n" << endl;

    // Версія з CAS
    
    for (int num_threads : NUM_THREADS) {
        atomic_sum = 0, atomic_min = INT_MAX;
        start = chrono::high_resolution_clock::now();
        vector<thread> threads;
        int chunk_size = SIZE / num_threads;
        for (int i = 0; i < num_threads; ++i) {
            int start_idx = i * chunk_size;
            int end_idx = (i == num_threads - 1) ? SIZE : (i + 1) * chunk_size;
            threads.emplace_back(cas_version, start_idx, end_idx);
        }
        for (auto& t : threads) t.join();
        end = chrono::high_resolution_clock::now();
        cout << "CAS \t|" << num_threads << " threads|: \tSum = " << atomic_sum.load() << ", Min = " << atomic_min.load()
            << ", Time = " << chrono::duration<double>(end - start).count() << " seconds\n";
    }

    return 0;
}

