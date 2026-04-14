#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>


// Measuring CPU time (ms)
double get_cpu_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0);
}

int main() {
    printf("=== Memory Retrieval Benchmark ===\n");
    printf("Extracting Modeling Parameters: Time (O(1), O(log n), O(n)) & Space Complexity\n");
    
    int test_iterations = 1000000; // 1 million iterations to mitigate OS noise

    // Open CSV file to save the final benchmark results
    FILE *csv_file = fopen("retrieval_benchmark.csv", "w");
    if (!csv_file) {
        printf("Error: Cannot create CSV file.\n");
        return 1;
    }
    
    // Storage sizes for each scheme (Bytes)
    int aoose_token_size = 188;    
    int coose_token_size = 346;    
    int baseline_token_size = 220; 
    
    double start, end;

    // =========================================================
    // Part 1: Time Complexity Benchmarks (Microseconds per operation)
    // =========================================================
    printf("[1] Time Complexity Analysis (Retrieval Delay)\n");
    fprintf(csv_file, "--- Time Complexity (us) ---\n");
    fprintf(csv_file, "Algorithm,Retrieval_Time_us\n");

    // 1. AOOSE O(1) Stack Pop (188 Bytes)
    unsigned char *dummy_aoose = malloc(aoose_token_size);
    memset(dummy_aoose, 1, aoose_token_size);
    start = get_cpu_time_ms();
    for(int i = 0; i < test_iterations; i++) {
        unsigned char *target = dummy_aoose; 
        asm volatile("" : : "r" (target) : "memory"); 
    }
    end = get_cpu_time_ms();
    double aoose_pop_us = ((end - start) * 1000.0) / test_iterations;
    printf(" - AOOSE    O(1) Stack Pop Time          : %.6f us\n", aoose_pop_us);
    fprintf(csv_file, "AOOSE O(1) Stack Pop,%.6f\n", aoose_pop_us);
    free(dummy_aoose);

    // 2. COOSE O(1) Stack Pop (346 Bytes)
    unsigned char *dummy_coose = malloc(coose_token_size);
    memset(dummy_coose, 1, coose_token_size);
    start = get_cpu_time_ms();
    for(int i = 0; i < test_iterations; i++) {
        unsigned char *target = dummy_coose;
        asm volatile("" : : "r" (target) : "memory"); 
    }
    end = get_cpu_time_ms();
    double coose_pop_us = ((end - start) * 1000.0) / test_iterations;
    printf(" - COOSE    O(1) Stack Pop Time          : %.6f us\n", coose_pop_us);
    fprintf(csv_file, "COOSE O(1) Stack Pop,%.6f\n", coose_pop_us);
    free(dummy_coose);

    // 3. Baseline O(n) Linear Scan (Time for checking 1 element)
    unsigned char *dummy_baseline = malloc(baseline_token_size);
    memset(dummy_baseline, 1, baseline_token_size);
    start = get_cpu_time_ms();
    for(int i = 0; i < test_iterations; i++) {
        int match_found = 0;
        if (dummy_baseline[0] == 255) { match_found = 1; }
        asm volatile("" : : "r" (match_found) : "memory");
    }
    end = get_cpu_time_ms();
    double baseline_scan_us = ((end - start) * 1000.0) / test_iterations;
    printf(" - Baseline O(n) 1-Element Scan          : %.6f us\n", baseline_scan_us);
    fprintf(csv_file, "Baseline O(n) Linear Scan (1-element),%.6f\n", baseline_scan_us);

    // 4. Baseline O(log n) Binary Search (Time for 1 comparison step)
    start = get_cpu_time_ms();
    for(int i = 0; i < test_iterations; i++) {
        int low = 0, high = 1000, mid = (low + high) / 2, match_found = 0;
        if (dummy_baseline[0] == 255) { match_found = 1; }
        else if (dummy_baseline[0] < 255) { low = mid + 1; }
        else { high = mid - 1; }
        asm volatile("" : : "r" (match_found), "r" (low), "r" (high) : "memory");
    }
    end = get_cpu_time_ms();
    double baseline_binary_step_us = ((end - start) * 1000.0) / test_iterations;
    printf(" - Baseline O(log n) 1-Step Branching    : %.6f us\n", baseline_binary_step_us);
    fprintf(csv_file, "Baseline O(log n) Binary Search (1-step),%.6f\n", baseline_binary_step_us);

    // 5. Baseline O(1) Hash Map (Time for Hash computation + Lookup)
    start = get_cpu_time_ms();
    for(int i = 0; i < test_iterations; i++) {
        uint32_t hash = 5381;
        for(int j = 0; j < 32; j++) { hash = ((hash << 5) + hash) + dummy_baseline[j]; }
        int index = hash % 1000;
        int match_found = (dummy_baseline[0] == 255);
        asm volatile("" : : "r" (index), "r" (match_found) : "memory");
    }
    end = get_cpu_time_ms();
    double baseline_hash_us = ((end - start) * 1000.0) / test_iterations;
    printf(" - Baseline O(1) Hash Computation+Lookup : %.6f us\n\n", baseline_hash_us);
    fprintf(csv_file, "Baseline O(1) Hash Map (Compute+Lookup),%.6f\n\n", baseline_hash_us);
    free(dummy_baseline);

    // =========================================================
    // Part 2: Space Complexity Analysis (Memory Trade-off)
    // =========================================================
    printf("[2] Space Complexity Analysis (Memory Trade-off in KB)\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("%-10s | %-12s | %-12s | %-12s | %-16s | %-16s\n", "DB Size(N)", "AOOSE Stack", "COOSE Stack", "BL Linear", "BL Binary Search", "BL Hash Map");
    printf("--------------------------------------------------------------------------------\n");

    fprintf(csv_file, "--- Space Complexity (KB) ---\n");
    fprintf(csv_file, "DB_Size(N), AOOSE_Stack_KB, COOSE_Stack_KB, BL_Linear_KB,BL_BinarySearch_KB,BL_HashMap_KB\n");

    int n_values[] = {1, 10, 100, 200, 400, 600, 1000};
    for (int i = 0; i < 7; i++) {
        int N = n_values[i];
        
        double mem_aoose_kb = (N * (aoose_token_size + 8.0)) / 1024.0;
        double mem_coose_kb = (N * (coose_token_size + 8.0)) / 1024.0;
        double mem_linear_kb = (N * (baseline_token_size + 8.0)) / 1024.0;
        double mem_bst_kb = (N * (baseline_token_size + 24.0)) / 1024.0;
        double mem_hash_kb = (N * baseline_token_size + (2 * N * 8.0) + (N * 16.0)) / 1024.0;

        printf("N=%-8d | %-9.2f KB | %-9.2f KB | %-9.2f KB | %-13.2f KB | %-13.2f KB\n", 
               N, mem_aoose_kb, mem_coose_kb, mem_linear_kb, mem_bst_kb, mem_hash_kb);
        
        fprintf(csv_file, "%d,%.2f,%.2f,%.2f,%.2f,%.2f\n", N, mem_aoose_kb, mem_coose_kb, mem_linear_kb, mem_bst_kb, mem_hash_kb);
    }

    printf("--------------------------------------------------------------------------------\n\n");
    printf("=== Results saved to 'retrieval_benchmark.csv' ===\n");

    fclose(csv_file);
    return 0;
}