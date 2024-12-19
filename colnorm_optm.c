// optimized version of matrix column normalization
#include "colnorm.h"

////////////////////////////////////////////////////////////////////////////////
// REQUIRED: Paste a copy of your sumdiag_benchmark from an ODD grace
// node below.
//
// -------REPLACE WITH YOUR RUN + TABLE --------
/*
grace5:~/216-sync/Projects/p5-code: ./colnorm_benchmark
==== Matrix Column Normalization Benchmark Version 1 ====
------ Tuned for ODD grace.umd.edu machines -----
Running with REPEATS: 2 and WARMUP: 1
Running with 4 sizes and 4 thread_counts (max 4)
  ROWS   COLS   BASE  T   OPTM SPDUP POINT TOTAL
  1111   2223  0.041  1  0.026  1.61  0.69  0.69
                      2  0.024  1.74  0.80  1.49
                      3  0.025  1.63  0.71  2.19
                      4  0.028  1.49  0.58  2.77
  2049   4098  0.260  1  0.157  1.65  0.73  3.49
                      2  0.094  2.76  1.46  4.96
                      3  0.088  2.96  1.57  6.53
                      4  0.073  3.57  1.84  8.36
  4099   8197  2.462  1  0.343  7.18  2.84 11.21
                      2  0.175 14.08  3.82 15.02
                      3  0.250  9.85  3.30 18.32
                      4  0.281  8.75  3.13 21.45
  6001  12003  5.460  1  0.737  7.41  2.89 24.34
                      2  0.379 14.42  3.85 28.19
                      3  0.493 11.07  3.47 31.66
                      4  0.416 13.13  3.71 35.37
RAW POINTS: 35.37
TOTAL POINTS: 35 / 35
*/
// -------REPLACE WITH YOUR RUN + TABLE --------


// You can write several different versions of your optimized function
// in this file and call one of them in the last function.

typedef struct {
  int thread_id;                // logical id of thread, 0,1,2,...
  int thread_count;             // total threads working on summing
  matrix_t mat;                 // matrix to sum
  vector_t avg;                 // vector to place sums
  vector_t std;
  pthread_mutex_t *vec_lock;    // mutex to lock the vec before adding on results
  pthread_barrier_t *barrier;
} colsums_context_t;

void *col_worker(void *arg);

int cn_verA(matrix_t mat, vector_t avg, vector_t std, int thread_count) {
  
  for (int i = 0; i < mat.cols; i++) { //initialize vals to 0
    VSET(avg, i, 0);
    VSET(std, i, 0);
  }

    pthread_mutex_t vec_lock;
    pthread_mutex_init(&vec_lock, NULL);

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, thread_count);

    pthread_t threads[thread_count];           // track each thread
    colsums_context_t ctxs[thread_count];      // context for each trhead

    for (int i = 0; i < thread_count; i++) { //initializes the contexts array

      ctxs[i].thread_id = i;
      ctxs[i].thread_count = thread_count;
      ctxs[i].mat = mat;
      ctxs[i].avg = avg;
      ctxs[i].std = std;
      ctxs[i].vec_lock = &vec_lock;
      ctxs[i].barrier = &barrier;

      pthread_create(&threads[i], NULL, col_worker, &ctxs[i]);
    }

    for (int i = 0; i < thread_count; i++) { 
      pthread_join(threads[i], NULL);
    }

      pthread_barrier_destroy(&barrier);
      pthread_mutex_destroy(&vec_lock);          // get rid of the lock to avoid a memory leak
  
  return 0;
}

void *col_worker(void *arg) {

  colsums_context_t ctx = *((colsums_context_t *) arg);
  matrix_t mat = ctx.mat;
  vector_t avg = ctx.avg;
  vector_t std = ctx.std;

  //to split up thread by rows 
  int rows_per_thread = mat.rows / ctx.thread_count;
  int beg_row = rows_per_thread * ctx.thread_id;
  int end_row = rows_per_thread * (ctx.thread_id+1);
  if(ctx.thread_id == ctx.thread_count-1){
    end_row = mat.rows;
  }

  //to split up thread by cols
  int cols_per_thread = mat.cols / ctx.thread_count;
  int beg_col = cols_per_thread * ctx.thread_id;
  int end_col = cols_per_thread * (ctx.thread_id+1);
  if(ctx.thread_id == ctx.thread_count-1){
    end_col = mat.cols;
  }

  double *local_vec = malloc(sizeof(double) * avg.len);
  for(int i=0; i<avg.len; i++){
    local_vec[i] = 0;
  }

  //to add the average to local_vec
  for(int i=beg_row; i<end_row; i++){
    for(int j=0; j<mat.cols; j++){
      double el_ij = MGET(mat, i, j);
      local_vec[j] += el_ij / mat.rows;
    }
  }

  pthread_mutex_lock(ctx.vec_lock);
  //now adding average to the gloabl vec
  for (int i = 0; i < avg.len; i++) {
    avg.data[i] += local_vec[i];
    local_vec[i] = 0;
  }

  pthread_mutex_unlock(ctx.vec_lock);

  pthread_barrier_wait(ctx.barrier);

  //adding the difference squared to local vec
  for (int i = beg_row; i < end_row; i++) {
    for (int j = 0; j < std.len; j++) {
      double diff = MGET(mat, i, j) - avg.data[j];
      local_vec[j] += diff*diff; 
    }
  }

  pthread_mutex_lock(ctx.vec_lock);

      for (int i = 0; i < std.len; i++) {
        std.data[i] += local_vec[i]; //std now contains the squared numbers
        local_vec[i] = 0;
      }

  pthread_mutex_unlock(ctx.vec_lock);

  pthread_barrier_wait(ctx.barrier);
  //square rooting them to get standard deviation
  for (int i = beg_col; i < end_col; i++) {
    std.data[i] = sqrt(std.data[i] / mat.rows); //std
  }

    pthread_barrier_wait(ctx.barrier);

  //normalizing every item in matrix 
  for (int i = beg_row; i < end_row; i++) {
    
      for (int j = 0; j < mat.cols; j++) {
        double mij = MGET(mat, i, j);
        mij = (mij - avg.data[j])/std.data[j];
        if (fabs(mij) <  DIFFTOL) {
          mij = abs(mij);
        }
        MSET(mat, i, j, mij);
      }

  }
  // free the local vector before ending
  free(local_vec);
  return NULL;

}

int cn_verB(matrix_t mat, vector_t avg, vector_t std, int thread_count) {

  return 0;

}


int colnorm_OPTM(matrix_t mat, vector_t avg, vector_t std, int thread_count){
  // call your preferred version of the function
  return cn_verA(mat, avg, std, thread_count);
}

////////////////////////////////////////////////////////////////////////////////
// REQUIRED: DON'T FORGET TO PASTE YOUR TIMING RESULTS FOR
// sumdiag_benchmark FROM AN ODD GRACE NODE AT THE TOP OF THIS FILE
////////////////////////////////////////////////////////////////////////////////
