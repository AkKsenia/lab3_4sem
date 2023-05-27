#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>


#define N 9//number of threads
#define M 1//number of cycles

void* memory_blocks[N];
int* dup_memory_block = (int*)memory_blocks;
pthread_mutex_t save_mutex, printf_mutex, fill_mutex;

//allocation of three memory blocks, output of addresses of allocated blocks
void* allocate_memory_blocks(void* arg) {
	int i = *(int*)arg;

	memory_blocks[i * 3] = my_malloc(16);//allocate 16 bytes of memory
	memory_blocks[i * 3 + 1] = my_malloc(1024);
	memory_blocks[i * 3 + 2] = my_malloc(1048576);

	pthread_mutex_lock(&printf_mutex);
	printf("%p %p %p\n", memory_blocks[i * 3], memory_blocks[i * 3 + 1], memory_blocks[i * 3 + 2]);
	pthread_mutex_unlock(&printf_mutex);
}

//the n-th thread fills the memory blocks allocated by the thread with the number n mod N / 3, the value n
void* fill_memory_blocks(void* arg) {
	int n = *(int*)arg;
	int number = n % (N / 3);

	pthread_mutex_lock(&fill_mutex);

	dup_memory_block[number * 3] = n;
	dup_memory_block[number * 3 + 1] = n;
	dup_memory_block[number * 3 + 2] = n;

	pthread_mutex_unlock(&fill_mutex);
}

//Saving to a data file the memory blocks allocated by the thread with the number n mod N/3,
//freeing the memory allocated for these blocks
void* save_to_file(void* arg) {
	int n = *(int*)arg;
	int number = n % (N / 3);

	pthread_mutex_lock(&save_mutex);

	FILE* file = fopen("file_for_saving.txt", "a");
	fprintf(file, "%d %d %d\n", dup_memory_block[number * 3], dup_memory_block[number * 3 + 1], dup_memory_block[number * 3 + 2]);
	fprintf(file, "\n");
	fclose(file);

	my_free(memory_blocks[number * 3]);
	my_free(memory_blocks[number * 3 + 1]);
	my_free(memory_blocks[number * 3 + 2]);

	pthread_mutex_unlock(&save_mutex);
}

int main(int argc, char* argv[]) {
	pthread_t threads[N];

	for (int m = 0; m < M; m++) {
		for (int i = 0; i < N / 3; i++) {
			if (pthread_create(&threads[i], NULL, &allocate_memory_blocks, &i) != 0) {
				perror("create");
				exit(EXIT_FAILURE);
			}
		}
		for (int i = 0; i < N / 3; i++) {
			if (pthread_join(threads[i], NULL) != 0) {
				perror("join");
				exit(EXIT_FAILURE);
			}
		}
		for (int i = N / 3; i < 2 * N / 3; i++) {
			if (pthread_create(&threads[i], NULL, &fill_memory_blocks, &i) != 0) {
				perror("create");
				exit(EXIT_FAILURE);
			}
		}
		for (int i = N / 3; i < 2 * N / 3; i++) {
			if (pthread_join(threads[i], NULL) != 0) {
				perror("join");
				exit(EXIT_FAILURE);
			}
		}
		for (int i = 2 * N / 3; i < N; i++) {
			if (pthread_create(&threads[i], NULL, &save_to_file, &i) != 0) {
				perror("create");
				exit(EXIT_FAILURE);
			}
		}
		for (int i = 2 * N / 3; i < N; i++) {
			if (pthread_join(threads[i], NULL) != 0) {
				perror("join");
				exit(EXIT_FAILURE);
			}
		}
	}

	exit(EXIT_SUCCESS);
}
