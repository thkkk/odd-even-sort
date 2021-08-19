# odd-even-sort
odd-even-sort, using MPI



Parallel odd-even-sort algorithm. Use MPI to implement the odd-even-sort algorithm, and the MPI process can only send messages to its neighboring processes.

`nprocs` is the number of processes. Each process has its own `data[0 ~ block_len-1]`. 

The `sort` function will sort all the `data[]`, using `nprocs` processes.



## 实验数据

| n         | N$\times$ P | 耗时(ms)     | 相对单进程的加速比 |
| --------- | ----------- | ------------ | ------------------ |
| 100000000 | 1$\times$1  | 12728.326000 | 1                  |
| 100000000 | 1$\times$2  | 6754.229000  | 1.884              |
| 100000000 | 1$\times$4  | 3559.514000  | 3.576              |
| 100000000 | 1$\times$8  | 2007.818000  | 6.339              |
| 100000000 | 1$\times$16 | 1340.771000  | 9.493              |
| 100000000 | 2$\times$16 | 870.302000   | 14.625             |



## 所做优化及结果

#### 每个阶段排序之后不进行check

此前，在每个阶段的奇偶排序进行完之后，都会进行一次进程之间的信息传递，以判断排序是否完成，这个过程要进行约$3*nprocs$次的send/recv。现在的优化是：总共只进行nprocs轮排序，不再进行check。这样的话，即使是目前在最小编号进程中的元素，而它值较大，本应排序到最大编号进程中，也可以在nprocs轮中排到正确的位置。

这样之后，大约有几十ms的优化。

#### 进程之间互相传递数据，然后进行优化后的归并

在一个排序阶段中，相邻进程块互相发送自己的全部数据，之后在每个块内部将两个块的数据进行归并，但是只保留最小/最大的block_len个元素，将其拷贝到自己的data上。这样可以省掉一半的归并时间。

这样之后大约有100+ms的优化。

#### 进程之间发送全部数据之前，先发送端点处的数据

进程之间发送全部数据之前，先发送端点处的数据，判断左边进程中的最大元素是否小于等于右边进程中的最小元素，如果是，那么无需进行后续数据的发送和归并。

这样之后大约有几十ms的优化。



## 带注释代码

```c++

void Worker::sort() {
    //data[0, block_len)
    if (out_of_range) return ;
    std::sort(data, data + block_len); 
    //先把当前进程数据排好序
    if (nprocs == 1) return ;

    bool odd_or_even = 0; // = 0: even;  = 1: odd;
    float *cp_data = new float [block_len];
    float *adj_data = new float [ceiling(n, nprocs)];

    int limit = nprocs;
    while(limit--){
        if(is_edge(rank, odd_or_even, last_rank)){  
            //边界情况，没有与其他进程存在于同一个进程块内

        }
        else if((rank & 1) == odd_or_even){  //receive info
            size_t adj_block_len = std::min(block_len, n - (rank + 1) * block_len);
            MPI_Request request[2];


            MPI_Isend(data + block_len - 1, 1, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, &request[0]);
            MPI_Irecv(adj_data, 1, MPI_FLOAT, rank + 1, 1, MPI_COMM_WORLD, &request[1]);
            MPI_Wait(&request[0], MPI_STATUS_IGNORE);
            MPI_Wait(&request[1], MPI_STATUS_IGNORE); //发送端点数据

            if(data [block_len - 1] > adj_data[0]) {  
                //此时两块之间存在未排好序的数据，需要排序
                MPI_Sendrecv(data, block_len, MPI_FLOAT, rank + 1, 0, 
                             adj_data, adj_block_len, MPI_FLOAT, rank + 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE); 
                //互相交换数据

                // merge
                merge_left(data, (int)block_len, adj_data, (int)adj_block_len, cp_data);  
                //进行归并排序，取前block_len个数据返回到cp_data中

                memcpy(data, cp_data, block_len * sizeof(float)); //拷贝回data
            }
        }
        else if ((rank & 1) == !odd_or_even){  //send info
            size_t adj_block_len = ceiling(n, nprocs);
            MPI_Request request[2];

            MPI_Isend(data, 1, MPI_FLOAT, rank - 1, 1, MPI_COMM_WORLD, &request[1]);
            MPI_Irecv(adj_data + adj_block_len - 1, 1, MPI_FLOAT, rank 
                      - 1, 0, MPI_COMM_WORLD, &request[0]);
            MPI_Wait(&request[1], MPI_STATUS_IGNORE);
            MPI_Wait(&request[0], MPI_STATUS_IGNORE);
            //发送端点数据

            if (adj_data[adj_block_len - 1] > data[0]){
                //此时两块之间存在未排好序的数据，需要排序
                MPI_Sendrecv(data, block_len, MPI_FLOAT, rank - 1, 1, 
                             adj_data, adj_block_len, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                //互相交换数据

                // merge
                merge_right(adj_data, (int)adj_block_len, data, (int)block_len, cp_data);
                //进行归并排序，取前block_len个数据返回到cp_data中

                memcpy(data, cp_data, block_len * sizeof(float)); //拷贝回data
            }

        } 
        odd_or_even ^= 1;
    }
    delete[] cp_data;
    delete[] adj_data;
}
```



