#include <pbc/pbc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

// Iteration number
#define ITERATIONS 1000

// Measuring CPU time(ms)
double get_cpu_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0);
}

int main(int argc, char **argv) {
    /* ================================================================
       0. Bilinear Pairing Initialization (Type A - Symmetric Pairing)
       ================================================================ */
    pairing_t pairing;
    FILE *sys_param_file = fopen("a.param", "r");
    if (!sys_param_file) {
        printf("Error: Cannot find a.param file.\n");
        return 1;
    }
    char param[1024];
    size_t count = fread(param, 1, 1024, sys_param_file);
    fclose(sys_param_file);
    pairing_init_set_buf(pairing, param, count); // pbc/pbc.h

    // Compute length of group G2 
    int gt_len = pairing_length_in_bytes_GT(pairing);

    /* ==========================================================
       1. System Parameters Generation & 2. Key Generation
       ========================================================== */
    element_t P, P1, Ppub, s, QID_A, SID_A, QID_B, SID_B, x, Y;
    element_init_G1(P, pairing); 
    element_init_G1(P1, pairing);
    element_init_Zr(s, pairing);
    element_init_G1(Ppub, pairing);
    element_init_G1(QID_A, pairing); 
    element_init_G1(SID_A, pairing);
    element_init_G1(QID_B, pairing); 
    element_init_G1(SID_B, pairing);
    element_init_Zr(x, pairing); //trapdoor key
    element_init_G1(Y, pairing); //hash key

    element_random(P); 
    element_random(s);
    element_mul_zn(Ppub, P, s);

    char *id_A = "ID_Alice@korea.ac.kr";
    char *id_B = "ID_Bob@korea.ac.kr";
    char *ch = "ChamelonHash";

    element_from_hash(QID_A, id_A, strlen(id_A));
    element_mul_zn(SID_A, QID_A, s);
    element_from_hash(QID_B, id_B, strlen(id_B));
    element_mul_zn(SID_B, QID_B, s); // element_mul_zn(return, G1, Zr);

    element_random(x);
    element_mul_zn(Y, P, x);


    element_t m_prime, r_prime, h, tmp1, tmp2, tmp3, tmp4;
    element_t IBS_k, IBS_r_pair, IBS_U, IBS_v, IBS_v_rec;
    element_t y, X, w, w_rec;
    element_t m_actual, r_actual, x_inv, m_diff, tmp_mul, h_rec;
    element_t e1, e2, e2_v, r_pair_rec;

    element_t rec_U, rec_v, rec_m, rec_r, rec_Y;

    element_init_Zr(m_prime, pairing); 
    element_init_Zr(r_prime, pairing);
    element_init_G1(h, pairing); 
    element_init_G1(tmp1, pairing); 
    element_init_G1(tmp2, pairing);
    element_init_G1(tmp3, pairing); 
    element_init_G1(tmp4, pairing);
    
    element_init_Zr(IBS_k, pairing); 
    element_init_GT(IBS_r_pair, pairing);
    element_init_G1(IBS_U, pairing); 
    element_init_Zr(IBS_v, pairing); 
    element_init_Zr(IBS_v_rec, pairing);
    
    element_init_Zr(y, pairing); 
    element_init_G1(X, pairing); 
    element_init_GT(w, pairing); 
    element_init_GT(w_rec, pairing);
    
    element_init_Zr(m_actual, pairing); 
    element_init_Zr(r_actual, pairing);
    element_init_Zr(x_inv, pairing); 
    element_init_Zr(m_diff, pairing); 
    element_init_Zr(tmp_mul, pairing);
    element_init_G1(h_rec, pairing);

    element_init_GT(e1, pairing); 
    element_init_GT(e2, pairing); 
    element_init_GT(e2_v, pairing); 
    element_init_GT(r_pair_rec, pairing);

    element_init_G1(rec_U, pairing); 
    element_init_Zr(rec_v, pairing);
    element_init_Zr(rec_m, pairing); 
    element_init_Zr(rec_r, pairing); 
    element_init_G1(rec_Y, pairing); 

    FILE *csv_file = fopen("benchmark_results_sun.csv", "w");
    if (csv_file) { 
        fprintf(csv_file, "Iteration,OffSigncrypt_ms,OnSigncrypt_ms,UnSigncrypt_ms\n"); 
    }

    double total_off_time = 0.0;
    double total_on_time = 0.0;
    double total_un_time = 0.0;

    int g1_len = element_length_in_bytes(P); 
    int zr_len = element_length_in_bytes(s); 
    int id_len = strlen(id_A); 
    int ch_len = strlen(ch); 

    // Plaintext : U(G1) + v(Zr) + ID(Str) + m(Zr) + r(Zr) + H(Str)
    int plaintext_len = g1_len + zr_len + id_len + zr_len + zr_len + ch_len; 

    printf("=== Start measurements (Iteration: %d times) ===\n", ITERATIONS);
    printf("Payload(Plaintext) Size: %d bytes\n\n", plaintext_len); 


    for (int iter = 0; iter < ITERATIONS; iter++) {
        
        // ----------------------------------------------------------
        // [1] OffSigncrypt 
        // ----------------------------------------------------------
        double off_start = get_cpu_time_ms();

        element_random(m_prime); 
        element_random(r_prime);
        element_mul_zn(tmp1, P, m_prime); 
        element_mul_zn(tmp2, Y, r_prime); 
        element_add(h, tmp1, tmp2);       

        // Hess ID-based signature protocol
        element_random(P1);
        element_random(IBS_k);
        pairing_apply(IBS_r_pair, P1, P, pairing); // pairing_apply(return, G1, G1, pairing);     
        element_pow_zn(IBS_r_pair, IBS_r_pair, IBS_k); // element_pow_zn(return, base, exponent);

        unsigned char gt_bytes[gt_len]; // variable-length array butter
        element_to_bytes(gt_bytes, IBS_r_pair);
        element_from_hash(IBS_v, gt_bytes, gt_len); 
        
        element_mul_zn(tmp1, SID_A, IBS_v);
        element_mul_zn(tmp2, P1, IBS_k);
        element_add(IBS_U, tmp1, tmp2); // U = v * SID_A + k * P1

        element_random(y);
        element_mul_zn(X, P, y); 
        element_mul_zn(tmp1, Ppub, y);
        pairing_apply(w, tmp1, QID_B, pairing); 

        unsigned char aes_key[SHA256_DIGEST_LENGTH]; 
        element_to_bytes(gt_bytes, w);
        SHA256(gt_bytes, gt_len, aes_key); // H1

        double off_end = get_cpu_time_ms();
        double off_time = off_end - off_start; 
        total_off_time += off_time; 


        // ----------------------------------------------------------
        // [2] OnSigncrypt 
        // ----------------------------------------------------------
        double on_start = get_cpu_time_ms();

        element_random(m_actual); // arbitrary message: 128 bytes

        element_invert(x_inv, x);               
        element_sub(m_diff, m_prime, m_actual); 
        element_mul(tmp_mul, x_inv, m_diff);    
        element_add(r_actual, tmp_mul, r_prime);

        // Plaintext
        unsigned char plaintext[plaintext_len];
        int offset = 0;

        offset += element_to_bytes(plaintext + offset, IBS_U);
        offset += element_to_bytes(plaintext + offset, IBS_v);
        memcpy(plaintext + offset, id_A, id_len); 
        offset += id_len;
        offset += element_to_bytes(plaintext + offset, m_actual);
        offset += element_to_bytes(plaintext + offset, r_actual);
        memcpy(plaintext + offset, ch, ch_len);
        offset += ch_len;

        unsigned char iv[12];
        RAND_bytes(iv, sizeof(iv));

        unsigned char ciphertext[plaintext_len + 16];
        unsigned char tag[16];
        int ciphertext_len = 0, len;

        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); //ssl
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_EncryptInit_ex(ctx, NULL, NULL, aes_key, iv);
        EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len);
        ciphertext_len = len;
        EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
        ciphertext_len += len;
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag); // integrity tag
        EVP_CIPHER_CTX_free(ctx);

        double on_end = get_cpu_time_ms();
        double on_time = on_end - on_start; 
        total_on_time += on_time;


        // ----------------------------------------------------------
        // [3] UnSigncrypt 
        // ----------------------------------------------------------
        double un_start = get_cpu_time_ms();

        pairing_apply(w_rec, X, SID_B, pairing);

        unsigned char aes_key_rec[SHA256_DIGEST_LENGTH];
        element_to_bytes(gt_bytes, w_rec);
        SHA256(gt_bytes, gt_len, aes_key_rec);

        unsigned char decryptedtext[plaintext_len + 16];
        int dec_len = 0;
        
        EVP_CIPHER_CTX *ctx_dec = EVP_CIPHER_CTX_new();
        EVP_DecryptInit_ex(ctx_dec, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_DecryptInit_ex(ctx_dec, NULL, NULL, aes_key_rec, iv);
        EVP_DecryptUpdate(ctx_dec, decryptedtext, &len, ciphertext, ciphertext_len);
        dec_len = len;
        
        EVP_CIPHER_CTX_ctrl(ctx_dec, EVP_CTRL_GCM_SET_TAG, 16, tag); 
        int dec_ret = EVP_DecryptFinal_ex(ctx_dec, decryptedtext + len, &len); 
        EVP_CIPHER_CTX_free(ctx_dec);

        int hash_valid = 0; 
        int sig_valid = 0;
        
        // Parse when GCM Tag verification passed
        if (dec_ret > 0) {
            dec_len += len;

            int doffset = 0;
            doffset += element_from_bytes(rec_U, decryptedtext + doffset); 
            doffset += element_from_bytes(rec_v, decryptedtext + doffset); 
            doffset += id_len; 
            doffset += element_from_bytes(rec_m, decryptedtext + doffset); 
            doffset += element_from_bytes(rec_r, decryptedtext + doffset); 
            doffset += ch_len;

            // Hash verification
            element_mul_zn(tmp3, P, rec_m);
            element_mul_zn(tmp4, Y, rec_r);
            element_add(h_rec, tmp3, tmp4);
            hash_valid = !element_cmp(h, h_rec);

            // Verify Signature
            pairing_apply(e1, rec_U, P, pairing);
            pairing_apply(e2, QID_A, Ppub, pairing);
            element_pow_zn(e2_v, e2, rec_v);
            element_div(r_pair_rec, e1, e2_v);

            element_to_bytes(gt_bytes, r_pair_rec);
            element_from_hash(IBS_v_rec, gt_bytes, gt_len); 
            sig_valid = !element_cmp(rec_v, IBS_v_rec);
        }

        
        double un_end = get_cpu_time_ms();
        double un_time = un_end - un_start; 
        total_un_time += un_time;

        if (csv_file) { 
            fprintf(csv_file, "%d,%.4f,%.4f,%.4f\n", iter + 1, off_time, on_time, un_time); 
        }

        if (iter == ITERATIONS - 1) {
            printf("Verification Result of %dth time\n", ITERATIONS);
            if (dec_ret > 0 && hash_valid && sig_valid) {
                printf("-> Verification of AES-GCM, Hash, and Signature passed!\n\n");
            } else {
                printf("-> Error: Verification failed.\n\n");
            }
        }
    }

    /* ==========================================================
        Benchmark Result
        ========================================================== */
    double avg_off = total_off_time / ITERATIONS;
    double avg_on = total_on_time / ITERATIONS;
    double avg_un = total_un_time / ITERATIONS;

    printf("=== Benchmark Result (Average CPU time) ===\n");
    printf("1. OffSigncrypt Avg: %.4f ms\n", avg_off);
    printf("2. OnSigncrypt  Avg: %.4f ms\n", avg_on);
    printf("3. UnSigncrypt  Avg: %.4f ms\n", avg_un);
    printf("======================================\n");
    
    if (csv_file) { 
        fprintf(csv_file, "\n"); 
        fprintf(csv_file, "Average,%.4f,%.4f,%.4f\n", avg_off, avg_on, avg_un); 
        fclose(csv_file);
        printf("-> Benchmark results are stored in 'benchmark_results_sun.csv'.\n\n"); 
    }


    /* ==========================================================
       Memory Deallocation
       ========================================================== */
    element_clear(P); 
    element_clear(P1); 
    element_clear(Ppub); 
    element_clear(s);
    element_clear(QID_A); 
    element_clear(SID_A); 
    element_clear(QID_B); 
    element_clear(SID_B);
    element_clear(x); 
    element_clear(Y);
    element_clear(m_prime); 
    element_clear(r_prime); 
    element_clear(h);
    element_clear(tmp1); 
    element_clear(tmp2); 
    element_clear(tmp3); 
    element_clear(tmp4);
    element_clear(IBS_k); 
    element_clear(IBS_r_pair); 
    element_clear(IBS_U); 
    element_clear(IBS_v); 
    element_clear(IBS_v_rec);
    element_clear(y); 
    element_clear(X); 
    element_clear(w); 
    element_clear(w_rec);
    element_clear(m_actual); 
    element_clear(r_actual); 
    element_clear(x_inv);
    element_clear(m_diff); 
    element_clear(tmp_mul); 
    element_clear(h_rec);
    element_clear(e1); 
    element_clear(e2); 
    element_clear(e2_v); 
    element_clear(r_pair_rec);
    element_clear(rec_U); 
    element_clear(rec_v); 
    element_clear(rec_m); 
    element_clear(rec_r); 
    element_clear(rec_Y); 
    pairing_clear(pairing);

    return 0;
}
