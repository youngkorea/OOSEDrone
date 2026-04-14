#include <pbc/pbc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

// Iteration number
#define ITERATIONS 600

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
    element_t P; 
    element_init_G1(P, pairing); 

    // IBS (Identity-Based Signature) Master & Keys
    element_t s, Ppub, QID_A, SID_A, QID_B, SID_B; 
    element_init_Zr(s, pairing); 
    element_init_G1(Ppub, pairing);
    element_init_G1(QID_A, pairing); 
    element_init_G1(SID_A, pairing);
    element_init_G1(QID_B, pairing);               
    element_init_G1(SID_B, pairing);               

    // IBK (Identity-Based Key Agreement) Master & Keys
    element_t s_bar, Ppub_bar, QID_A_bar, SID_A_bar, QID_B_bar, SID_B_bar; 
    element_init_Zr(s_bar, pairing); 
    element_init_G1(Ppub_bar, pairing);
    element_init_G1(QID_A_bar, pairing);           
    element_init_G1(SID_A_bar, pairing);           
    element_init_G1(QID_B_bar, pairing); 
    element_init_G1(SID_B_bar, pairing);

    // Chameleon Hash Key Pair for A
    element_t y_ch, HK_A; 
    element_init_Zr(y_ch, pairing);  // trapdoor key (CK_i = y)
    element_init_G1(HK_A, pairing); // hash key (HK_i = yP)

    // Encryption key pair for A (a, Ppub_A)
    element_t a_key, Ppub_A;        
    element_init_Zr(a_key, pairing);
    element_init_G1(Ppub_A, pairing);

    element_random(P); 
    
    // Setup IBS
    element_random(s);
    element_mul_zn(Ppub, P, s);

    // Setup IBK
    element_random(s_bar);
    element_mul_zn(Ppub_bar, P, s_bar);

    // User ID
    char *id_A = "ID_Alice@korea.ac.kr";
    char *id_B = "ID_Bob@korea.ac.kr";
    
    // Chameleon Hash Description
    char *ch = "ChamelonHashAB"; 

    // --- Key Extraction ---
    // 1. Extract IBS Keys
    element_from_hash(QID_A, id_A, strlen(id_A));
    element_mul_zn(SID_A, QID_A, s);

    element_from_hash(QID_B, id_B, strlen(id_B)); 
    element_mul_zn(SID_B, QID_B, s);

    // 2. Extract IBK Keys
    element_from_hash(QID_A_bar, id_A, strlen(id_A)); 
    element_mul_zn(SID_A_bar, QID_A_bar, s_bar);

    element_from_hash(QID_B_bar, id_B, strlen(id_B));
    element_mul_zn(SID_B_bar, QID_B_bar, s_bar);

    // 3. Extract Chameleon Hash Keys & Encryption key for A
    element_random(y_ch);
    element_mul_zn(HK_A, P, y_ch);

    element_random(a_key);               
    element_mul_zn(Ppub_A, P, a_key);    


    /* ==========================================================
       Variables Initialization
       ========================================================== */
    element_t m_prime, r_prime, h, tmp1, tmp2, tmp3, tmp4;
    element_t IBS_k, IBS_r_pair, IBS_U, IBS_v, IBS_v_rec;
    element_t x_on, X_ij, w, w_rec;
    element_t w_off, w_off_rec;          
    element_t m_actual, r_actual, y_ch_inv, m_diff, tmp_mul, h_rec;
    element_t e1, e2, e2_v, r_pair_rec;
    
    // Offline Parsing Variables
    element_t rec_U_off, rec_v_off, rec_m_prime_off, rec_r_prime_off, rec_h_off; 
    // Online Parsing Variables
    element_t rec_U, rec_v, rec_m_prime, rec_r_prime, rec_m, rec_r;

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
    
    element_init_Zr(x_on, pairing); 
    element_init_G1(X_ij, pairing); 
    element_init_GT(w, pairing); 
    element_init_GT(w_rec, pairing);
    element_init_GT(w_off, pairing);     
    element_init_GT(w_off_rec, pairing); 
    
    element_init_Zr(m_actual, pairing); 
    element_init_Zr(r_actual, pairing);
    element_init_Zr(y_ch_inv, pairing); 
    element_init_Zr(m_diff, pairing); 
    element_init_Zr(tmp_mul, pairing); 
    element_init_G1(h_rec, pairing);

    element_init_GT(e1, pairing); 
    element_init_GT(e2, pairing); 
    element_init_GT(e2_v, pairing); 
    element_init_GT(r_pair_rec, pairing);

    // Initialize Offline vars
    element_init_G1(rec_U_off, pairing);       
    element_init_Zr(rec_v_off, pairing);      
    element_init_Zr(rec_m_prime_off, pairing); 
    element_init_Zr(rec_r_prime_off, pairing); 
    element_init_G1(rec_h_off, pairing);       

    // Initialize Online vars
    element_init_G1(rec_U, pairing); 
    element_init_Zr(rec_v, pairing);
    element_init_Zr(rec_m_prime, pairing); 
    element_init_Zr(rec_r_prime, pairing);
    element_init_Zr(rec_m, pairing); 
    element_init_Zr(rec_r, pairing); 

    FILE *csv_file = fopen("benchmark_results_coose_600.csv", "w");
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

    // Offline Plaintext : U(G1) + v(Zr) + m'(Zr) + r'(Zr) + h(G1) + CH(Str)
    int plaintext_off_len = g1_len + zr_len + zr_len + zr_len + g1_len + ch_len;  

    // Online Plaintext : U(G1) + v(Zr) + ID(Str) + m'(Zr) + r'(Zr) + m(Zr) + r(Zr) + CH(Str)
    int plaintext_len = g1_len + zr_len + id_len + zr_len + zr_len + zr_len + zr_len + ch_len;

    // Buffer Allocation : r_pair(GT) + h(G1) + CH(Str) 
    unsigned char *hash_buf = malloc(gt_len + g1_len + ch_len); 

    printf("=== Start COOSE measurements (Iteration: %d times) ===\n", ITERATIONS);
    printf("Payload(Plaintext) Size: %d bytes\n\n", plaintext_len); 

    for (int iter = 0; iter < ITERATIONS; iter++) {
        
        // ----------------------------------------------------------
        // [1] OffSigncrypt 
        // ----------------------------------------------------------
        double off_start = get_cpu_time_ms();

        element_random(m_prime); 
        element_random(r_prime);
        element_mul_zn(tmp1, P, m_prime); 
        element_mul_zn(tmp2, HK_A, r_prime); 
        element_add(h, tmp1, tmp2);       

        // Hess ID-based signature protocol
        element_random(IBS_k);
        pairing_apply(IBS_r_pair, P, P, pairing); 
        element_pow_zn(IBS_r_pair, IBS_r_pair, IBS_k); 

        // Hash(r_pair || h || CH) for v 
        element_to_bytes(hash_buf, IBS_r_pair);
        element_to_bytes(hash_buf + gt_len, h);
        memcpy(hash_buf + gt_len + g1_len, ch, ch_len);                
        element_from_hash(IBS_v, hash_buf, gt_len + g1_len + ch_len);  
        
        element_mul_zn(tmp1, SID_A, IBS_v);
        element_mul_zn(tmp2, P, IBS_k); 
        element_add(IBS_U, tmp1, tmp2); 

        // Generate Self-Decryption Key (dk_{i,i})
        element_mul_zn(tmp1, Ppub_bar, a_key);
        pairing_apply(w_off, tmp1, QID_A_bar, pairing); 

        unsigned char gt_bytes[gt_len];
        unsigned char aes_key_off[SHA256_DIGEST_LENGTH]; 
        element_to_bytes(gt_bytes, w_off);
        SHA256(gt_bytes, gt_len, aes_key_off); // H3

        // Encrypt 
        unsigned char plaintext_off[plaintext_off_len];
        int off_offset = 0;
        off_offset += element_to_bytes(plaintext_off + off_offset, IBS_U);
        off_offset += element_to_bytes(plaintext_off + off_offset, IBS_v);
        off_offset += element_to_bytes(plaintext_off + off_offset, m_prime);
        off_offset += element_to_bytes(plaintext_off + off_offset, r_prime);
        off_offset += element_to_bytes(plaintext_off + off_offset, h);
        memcpy(plaintext_off + off_offset, ch, ch_len);

        unsigned char iv_off[12], tag_off[16];
        unsigned char ciphertext_off[plaintext_off_len + 16];
        int c_off_len = 0, len;
        RAND_bytes(iv_off, 12);

        EVP_CIPHER_CTX *ctx_off = EVP_CIPHER_CTX_new(); 
        EVP_EncryptInit_ex(ctx_off, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_EncryptInit_ex(ctx_off, NULL, NULL, aes_key_off, iv_off);
        EVP_EncryptUpdate(ctx_off, ciphertext_off, &len, plaintext_off, plaintext_off_len);
        c_off_len = len;
        EVP_EncryptFinal_ex(ctx_off, ciphertext_off + len, &len);
        c_off_len += len;
        EVP_CIPHER_CTX_ctrl(ctx_off, EVP_CTRL_GCM_GET_TAG, 16, tag_off); 
        EVP_CIPHER_CTX_free(ctx_off);

        double off_end = get_cpu_time_ms();
        double off_time = off_end - off_start; 
        total_off_time += off_time; 


        // ----------------------------------------------------------
        // [2] OnSigncrypt 
        // ----------------------------------------------------------
        double on_start = get_cpu_time_ms();

        // Recover Decryption Key and Decrypt
        pairing_apply(w_off_rec, Ppub_A, SID_A_bar, pairing); 
        
        unsigned char aes_key_off_rec[SHA256_DIGEST_LENGTH];
        element_to_bytes(gt_bytes, w_off_rec);
        SHA256(gt_bytes, gt_len, aes_key_off_rec);

        unsigned char decryptedtext_off[plaintext_off_len + 16];
        int dec_off_len = 0;
        
        EVP_CIPHER_CTX *ctx_dec_off = EVP_CIPHER_CTX_new();
        EVP_DecryptInit_ex(ctx_dec_off, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_DecryptInit_ex(ctx_dec_off, NULL, NULL, aes_key_off_rec, iv_off);
        EVP_DecryptUpdate(ctx_dec_off, decryptedtext_off, &len, ciphertext_off, c_off_len);
        dec_off_len = len;
        EVP_CIPHER_CTX_ctrl(ctx_dec_off, EVP_CTRL_GCM_SET_TAG, 16, tag_off); 
        int dec_off_ret = EVP_DecryptFinal_ex(ctx_dec_off, decryptedtext_off + len, &len); 
        EVP_CIPHER_CTX_free(ctx_dec_off);

        int sig_valid_on = 0;
        if (dec_off_ret > 0) {
            int doffset = 0;
            doffset += element_from_bytes(rec_U_off, decryptedtext_off + doffset);
            doffset += element_from_bytes(rec_v_off, decryptedtext_off + doffset);
            doffset += element_from_bytes(rec_m_prime_off, decryptedtext_off + doffset);
            doffset += element_from_bytes(rec_r_prime_off, decryptedtext_off + doffset);
            doffset += element_from_bytes(rec_h_off, decryptedtext_off + doffset);
            doffset += ch_len;

            // Verify Offline Signature (Authentication) using decrypted values
            pairing_apply(e1, rec_U_off, P, pairing);
            pairing_apply(e2, QID_A, Ppub, pairing);
            element_pow_zn(e2_v, e2, rec_v_off);
            element_div(r_pair_rec, e1, e2_v);

            element_to_bytes(hash_buf, r_pair_rec);
            element_to_bytes(hash_buf + gt_len, rec_h_off);
            memcpy(hash_buf + gt_len + g1_len, ch, ch_len);                    
            element_from_hash(IBS_v_rec, hash_buf, gt_len + g1_len + ch_len); 
            sig_valid_on = !element_cmp(rec_v_off, IBS_v_rec);

            // Compute collision r using Decrypted m', r'
            element_random(m_actual); // arbitrary message
            element_invert(y_ch_inv, y_ch);               
            element_sub(m_diff, rec_m_prime_off, m_actual); 
            element_mul(tmp_mul, y_ch_inv, m_diff);    
            element_add(r_actual, tmp_mul, rec_r_prime_off); 
        }

        // Generate Decryption Key for B
        element_random(x_on);
        element_mul_zn(X_ij, P, x_on); 
        element_mul_zn(tmp1, Ppub_bar, x_on);
        pairing_apply(w, tmp1, QID_B_bar, pairing); 

        unsigned char aes_key[SHA256_DIGEST_LENGTH]; 
        element_to_bytes(gt_bytes, w);
        SHA256(gt_bytes, gt_len, aes_key); // H2

        // Plaintext 
        unsigned char plaintext[plaintext_len];
        int offset = 0;

        offset += element_to_bytes(plaintext + offset, rec_U_off);       
        offset += element_to_bytes(plaintext + offset, rec_v_off);       
        memcpy(plaintext + offset, id_A, id_len); 
        offset += id_len;
        offset += element_to_bytes(plaintext + offset, rec_m_prime_off); 
        offset += element_to_bytes(plaintext + offset, rec_r_prime_off); 
        offset += element_to_bytes(plaintext + offset, m_actual);
        offset += element_to_bytes(plaintext + offset, r_actual);
        memcpy(plaintext + offset, ch, ch_len);
        offset += ch_len;

        unsigned char iv[12];
        RAND_bytes(iv, sizeof(iv));

        unsigned char ciphertext[plaintext_len + 16];
        unsigned char tag[16]; // MAC
        int ciphertext_len = 0;

        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); 
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

        // Key Agreement recovery using Bob's IBK Private Key
        pairing_apply(w_rec, X_ij, SID_B_bar, pairing);

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
        int sig_valid_un = 0;
        
        // Parse when GCM Tag verification passed
        if (dec_ret > 0) {
            dec_len += len;

            int doffset = 0;
            doffset += element_from_bytes(rec_U, decryptedtext + doffset); 
            doffset += element_from_bytes(rec_v, decryptedtext + doffset); 
            doffset += id_len; 
            doffset += element_from_bytes(rec_m_prime, decryptedtext + doffset); 
            doffset += element_from_bytes(rec_r_prime, decryptedtext + doffset); 
            doffset += element_from_bytes(rec_m, decryptedtext + doffset); 
            doffset += element_from_bytes(rec_r, decryptedtext + doffset); 
            doffset += ch_len;

            // Reconstruct h
            element_mul_zn(tmp3, P, rec_m);
            element_mul_zn(tmp4, HK_A, rec_r);
            element_add(h_rec, tmp3, tmp4);
            hash_valid = !element_cmp(h, h_rec);
            
            // Verify Signature using IBS Public Key
            pairing_apply(e1, rec_U, P, pairing);
            pairing_apply(e2, QID_A, Ppub, pairing);
            element_pow_zn(e2_v, e2, rec_v);
            element_div(r_pair_rec, e1, e2_v);

            element_to_bytes(hash_buf, r_pair_rec);
            element_to_bytes(hash_buf + gt_len, h_rec);
            memcpy(hash_buf + gt_len + g1_len, ch, ch_len);                    
            element_from_hash(IBS_v_rec, hash_buf, gt_len + g1_len + ch_len);  
            sig_valid_un = !element_cmp(rec_v, IBS_v_rec);
        }

        double un_end = get_cpu_time_ms();
        double un_time = un_end - un_start; 
        total_un_time += un_time;

        if (csv_file) { 
            fprintf(csv_file, "%d,%.4f,%.4f,%.4f\n", iter + 1, off_time, on_time, un_time); 
        }

        if (iter == ITERATIONS - 1) {
            printf("Verification Result of %dth time\n", ITERATIONS);
            if (dec_off_ret > 0 && dec_ret > 0 && sig_valid_on && sig_valid_un) {
                printf("-> Verification of Offline AES, Online AES, and Signature(Un) passed!\n\n");
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

    printf("=== COOSE Benchmark Result (Average CPU time) ===\n");
    printf("1. OffSigncrypt Avg: %.4f ms\n", avg_off);
    printf("2. OnSigncrypt  Avg: %.4f ms\n", avg_on);
    printf("3. UnSigncrypt  Avg: %.4f ms\n", avg_un);
    printf("======================================\n");
    
    if (csv_file) { 
        fprintf(csv_file, "\n"); 
        fprintf(csv_file, "Average,%.4f,%.4f,%.4f\n", avg_off, avg_on, avg_un); 
        fclose(csv_file);
        printf("-> Benchmark results are stored in 'benchmark_results_coose.csv'.\n\n"); 
    }

    /* ==========================================================
       Memory Deallocation
       ========================================================== */
    free(hash_buf);
    element_clear(P); 
    
    element_clear(s); 
    element_clear(Ppub);
    element_clear(QID_A); 
    element_clear(SID_A); 
    element_clear(QID_B);     
    element_clear(SID_B);     
    
    element_clear(s_bar); 
    element_clear(Ppub_bar);
    element_clear(QID_A_bar); 
    element_clear(SID_A_bar); 
    element_clear(QID_B_bar); 
    element_clear(SID_B_bar);
    
    element_clear(y_ch); 
    element_clear(HK_A);
    element_clear(a_key);       
    element_clear(Ppub_A);      
    
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
    element_clear(x_on); 
    element_clear(X_ij); 
    element_clear(w); 
    element_clear(w_rec);
    element_clear(w_off);       
    element_clear(w_off_rec);   
    
    element_clear(m_actual); 
    element_clear(r_actual); 
    element_clear(y_ch_inv);
    element_clear(m_diff); 
    element_clear(tmp_mul); 
    element_clear(h_rec);
    element_clear(e1); 
    element_clear(e2); 
    element_clear(e2_v); 
    element_clear(r_pair_rec);
    
    element_clear(rec_U_off);       
    element_clear(rec_v_off);       
    element_clear(rec_m_prime_off); 
    element_clear(rec_r_prime_off); 
    element_clear(rec_h_off);       
    
    element_clear(rec_U); 
    element_clear(rec_v); 
    element_clear(rec_m_prime); 
    element_clear(rec_r_prime); 
    element_clear(rec_m); 
    element_clear(rec_r); 
    pairing_clear(pairing);

    return 0;
}