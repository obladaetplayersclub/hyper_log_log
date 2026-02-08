#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <map>
#include <bitset>

const int B_BITS = 10;
const int M_REGS = 1 << B_BITS;      
const int BITS_PER_REG = 5;      
const int TOTAL_BITS = M_REGS * BITS_PER_REG;

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

class HyperLogLogOptimized {
private:
    double alphaMM;
    std::bitset<TOTAL_BITS> registers;
    double get_alpha(int m) {
        if (m == 16) return 0.673;
        if (m == 32) return 0.697;
        if (m == 64) return 0.709;
        return 0.7213 / (1.0 + 1.079 / m);
    }
    void set_val(int idx, uint8_t val) {
        int start = idx * BITS_PER_REG;
        for (int k = 0; k < BITS_PER_REG; ++k) {
            registers[start + k] = (val >> k) & 1;
        }
    }
    uint8_t get_val(int idx) {
        int start = idx * BITS_PER_REG;
        uint8_t val = 0;
        for (int k = 0; k < BITS_PER_REG; ++k) {
            if (registers[start + k]) val |= (1 << k);
        }
        return val;
    }
    uint8_t get_rho(uint32_t w) {
        if (w == 0) return (uint8_t)(32 - B_BITS + 1);
        uint8_t rho = 1;
        while ((w & 1) == 0) {
            w >>= 1;
            rho++;
        }
        return rho;
    }

public:
    HyperLogLogOptimized() {
        registers.reset();
        alphaMM = get_alpha(M_REGS) * M_REGS * M_REGS;
    }
    void add(const std::string& s) {
        uint32_t x = HashFuncGen::hash(s);
        uint32_t j = x >> (32 - B_BITS);
        uint32_t mask = (1 << (32 - B_BITS)) - 1;
        uint32_t w = x & mask;
        uint32_t sentry = (1 << (32 - B_BITS)); 
        uint8_t rho = get_rho(w | sentry);
        if (rho > 31) rho = 31;
        if (rho > get_val(j)) {
            set_val(j, rho);
        }
    }

    double estimate() {
        double sum_inv = 0.0;
        for (int j = 0; j < M_REGS; ++j) {
            sum_inv += std::pow(2.0, -get_val(j));
        }
        double E = alphaMM / sum_inv;
        if (E <= 2.5 * M_REGS) {
            int V = 0;
            for (int j = 0; j < M_REGS; ++j) {
                if (get_val(j) == 0) V++;
            }
            if (V > 0) E = M_REGS * std::log((double)M_REGS / V);
        }
        return E;
    }
};

int main() {
    int max_elements = 80000;
    int num_streams = 40;
    int step = 1000;
    std::ofstream file("upgraded_results.csv");
    file << "Step,ExactCount,AvgEstimate,StdDev\n";
    std::map<int, std::vector<double>> results;
    std::cout << "Начало симуляции" << std::endl;
    for (int run = 0; run < num_streams; run++) {
        RandomStreamGen streamGen;
        HyperLogLogOptimized hll;
        for (int i = 1; i <= max_elements; i++) {
            std::string s = streamGen.next();
            hll.add(s);
            if (i % step == 0) {
                results[i].push_back(hll.estimate());
            }
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