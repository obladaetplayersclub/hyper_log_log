#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <map>
#include <cstdint>

class RandomStreamGen {
private:
    std::mt19937 gen;
    std::uniform_int_distribution<> len_dist;
    std::uniform_int_distribution<> char_dist;
    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

public:
    RandomStreamGen() : gen(std::random_device{}()), len_dist(1, 30), char_dist(0, 61) {}
    std::string next() {
        int length = len_dist(gen);
        std::string res;
        res.reserve(length);
        for (int i = 0; i < length; ++i) res += chars[char_dist(gen)];
        return res;
    }
};

class HashFuncGen {
public:
    static uint32_t hash(const std::string& key) {
        uint32_t h = 0x9747b28c;
        for (char c : key) {
            h ^= c;
            h *= 0x5bd1e995;
            h ^= h >> 15;
        }
        h ^= h >> 13;
        h *= 0x5bd1e995;
        h ^= h >> 15;
        return h;
    }
};

class HyperLogLog {
private:
    int b;
    int m;
    double alphaMM;
    std::vector<int> registers;
    double get_alpha(int m) {
        if (m == 16) return 0.673;
        if (m == 32) return 0.697;
        if (m == 64) return 0.709;
        return 0.7213 / (1.0 + 1.079 / m);
    }
    int get_rho(uint32_t w) {
        if (w == 0) return 32 - b + 1;
        int rho = 1;
        while ((w & 1) == 0) {
            w >>= 1;
            rho++;
        }
        return rho;
    }

public:
    HyperLogLog(int b_bits) : b(b_bits), m(1 << b_bits) {
        registers.assign(m, 0);
        alphaMM = get_alpha(m) * m * m;
    }
    void add(const std::string& s) {
        uint32_t x = HashFuncGen::hash(s);
        uint32_t j = x >> (32 - b);
        uint32_t mask = (1 << (32 - b)) - 1;
        uint32_t w = x & mask;
        uint32_t sentry = (1 << (32 - b));
        registers[j] = std::max(registers[j], get_rho(w | sentry));
    }
    double estimate() {
        double sum_inv = 0.0;
        for (int val : registers) {
            sum_inv += std::pow(2.0, -val);
        }
        double E = alphaMM / sum_inv;
        if (E <= 2.5 * m) {
            int V = 0;
            for (int val : registers) if (val == 0) V++;
            if (V > 0) E = m * std::log((double)m / V);
        }
        return E;
    }
};

int main() {
    int b = 10;
    int max_elements = 80000;
    int num_streams = 40;
    int step = 1000;
    std::ofstream file("normal_version_results.csv");
    file << "Step,ExactCount,AvgEstimate,StdDev\n";
    std::map<int, std::vector<double>> results;
    std::cout << "Начало симуляции" << std::endl;
    for (int run = 0; run < num_streams; run++) {
        RandomStreamGen streamGen;
        HyperLogLog hll(b);
        for (int i = 1; i <= max_elements; i++) {
            std::string s = streamGen.next();
            hll.add(s);
            if (i % step == 0) results[i].push_back(hll.estimate());
        }
    }
    for (auto const& [current_step, estimates] : results) {
        double sum = 0;
        for (double v : estimates) sum += v;
        double mean = sum / estimates.size();
        double sq_sum = 0;
        for (double v : estimates) sq_sum += (v - mean) * (v - mean);
        double std_dev = std::sqrt(sq_sum / estimates.size());
        file << current_step << "," << current_step << "," << mean << "," << std_dev << "\n";
    }
    file.close();
    std::cout << "Сделано. CSV сгенерирован" << std::endl;
}