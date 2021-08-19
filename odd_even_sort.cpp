#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <mpi.h>
#include <cmath>

#include "worker.h"
using namespace std;


bool is_edge(int rank, bool odd_or_even, bool last_rank){
  if (odd_or_even == 0){
    return (rank & 1) == 0 && last_rank;
  }
  else{
    return rank == 0 || ((rank & 1) == 1 && last_rank);
  }
}

void merge_left(float *A, int nA, float *B, int nB, float *C){  //make sure C[nA-1] is available
  float *p1 = A, *A_end = A + nA, *p2 = B, *B_end = B + nB, *p = C, *C_end = C + nA;

  while( p != C_end && p1 != A_end && p2 != B_end)
    *(p++) = ((*p1) <= (*p2)) ? *(p1++) : *(p2++);
  while( p != C_end )
    *(p++) = *(p1++);
}
void merge_right(float *A, int nA, float *B, int nB, float *C){
  float *p1 = A + nA , *p2 = B + nB , *p = C + nB; 

  while( p != C && p1 != A && p2 != B )
    *(--p) = (*(p1-1) >= *(p2-1)) ? *(--p1) : *(--p2);
  while( p != C )
    *(--p) = *(--p2);
}


void Worker::sort() {
  /** Your code ... */

  //原来的编号: [block_len * rank, block_len * (rank + 1) - 1]
  //data[0, block_len)
  if (out_of_range) return ;
  std::sort(data, data + block_len);
  if (nprocs == 1) return ;

  bool odd_or_even = 0; // = 0: even;  = 1: odd;
  float *cp_data = new float [block_len];
  float *adj_data = new float [ceiling(n, nprocs)];

  int limit = nprocs;
  while(limit--){
    
    if(is_edge(rank, odd_or_even, last_rank)){ 

    }
    else if((rank & 1) == odd_or_even){  //receive info
      size_t adj_block_len = std::min(block_len, n - (rank + 1) * block_len);
      MPI_Request request[2];

      MPI_Isend(data + block_len - 1, 1, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, &request[0]);
      MPI_Irecv(adj_data, 1, MPI_FLOAT, rank + 1, 1, MPI_COMM_WORLD, &request[1]);
      MPI_Wait(&request[0], MPI_STATUS_IGNORE);
      MPI_Wait(&request[1], MPI_STATUS_IGNORE);

      if(data [block_len - 1] > adj_data[0]) {
        MPI_Sendrecv(data, block_len, MPI_FLOAT, rank + 1, 0, adj_data, adj_block_len, MPI_FLOAT, rank + 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // merge
        merge_left(data, (int)block_len, adj_data, (int)adj_block_len, cp_data);

        memcpy(data, cp_data, block_len * sizeof(float));
      }
    }
    else if ((rank & 1) == !odd_or_even){  //send info
      size_t adj_block_len = ceiling(n, nprocs);
      MPI_Request request[2];

      MPI_Isend(data, 1, MPI_FLOAT, rank - 1, 1, MPI_COMM_WORLD, &request[1]);
      MPI_Irecv(adj_data + adj_block_len - 1, 1, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, &request[0]);
      MPI_Wait(&request[1], MPI_STATUS_IGNORE);
      MPI_Wait(&request[0], MPI_STATUS_IGNORE);

      if (adj_data[adj_block_len - 1] > data[0]){
        MPI_Sendrecv(data, block_len, MPI_FLOAT, rank - 1, 1, adj_data, adj_block_len, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // merge
        merge_right(adj_data, (int)adj_block_len, data, (int)block_len, cp_data);

        memcpy(data, cp_data, block_len * sizeof(float));
      }

    } 
    odd_or_even ^= 1;
  }
  delete[] cp_data;
  delete[] adj_data;
}
