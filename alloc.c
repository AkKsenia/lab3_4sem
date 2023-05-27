#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>


#define BLOCK_SIZE 30

//to simplify further use
typedef struct meta_block* block;

//using linked list to represent memory block 
struct meta_block {
	size_t size;
	block next;
	block previous;
	bool is_free;
	void* pointer;
	char data[1];//the array’s pointer indicate the end of the meta-data
};
void* head = NULL;

//we would like to make sure that only one thread has access to the resource and the rest wait for the resource to be released
//in the pthreads library, one of the methods to resolve this situation is mutexes:
pthread_mutex_t malloc_lock, free_lock;

//find_block will iterate through list and return the block which has size >= size
block find_block(block* last, size_t size) {
	block current_block = (block)head;
	while (current_block && current_block->is_free && current_block->size >= size) {
		*last = current_block;//we need to keep the last visited chunk, so the malloc function can
		//easily extends the end of the heap if we found no fitting chunk
		current_block = current_block->next;
	}
	return current_block;
}

//here we we move the break and initialize a new block
block extend_heap(block last, size_t size) {
	block current_block;
	current_block = sbrk(0);//returns the current end of the heap
	//sbrk(size) increases the program data space by size bytes


	if (sbrk(BLOCK_SIZE + size) == (void*)-1)//sbrk() fails
		return NULL;

	current_block->size = size;
	current_block->next = NULL;
	current_block->previous = last;
	current_block->pointer = current_block->data;
	if (last)
		last->next = current_block;
	current_block->is_free = false;
	return current_block;
}

//this function cut the block passed in argument to make data block of the wanted size
void split_block(block current_block, size_t size) {
	block new_block;

	new_block->data[1] = current_block->data[1] + size;//increasing pointers by size bytes
	//when a chunk is wide enough to held the asked size plus a new chunk
	new_block->size = current_block->size - size - BLOCK_SIZE;
	//we insert a new chunk in the list
	new_block->next = current_block->next;
	new_block->previous = current_block;
	new_block->is_free = true;
	new_block->pointer = new_block->data;

	current_block->size = size;
	current_block->next = new_block;
	if (new_block->next)
		new_block->next->previous = new_block;
}

size_t align8(size_t size) {//we want the size to be multiples of 8
	if (size & 0x7 == 0)//checking that there are zeros at the end
		return size;
	return ((size >> 3) + 1) << 3;
	//>> - bitwise shift to the right by 3
	//+ 1 -to make the number bigger than the original one
	//<<3 - *8
}


//-------IMPLEMENTING MALLOC-------

//malloc(size) will return a pointer to a block of memory of the size = size
void* my_malloc(size_t size) {
	block current_block, last;

	//after creating a mutex, it can be captured using the function:
	pthread_mutex_lock(&malloc_lock);

	size_t size_ = align8(size);//align the requested size

	if (head) {
		last = head;
		//once lock is available, program will try to find if there are any free block already available
		current_block = find_block(&last, size_);
		if (current_block) {//if we found a chunk
			if ((current_block->size - size_) >= (BLOCK_SIZE + 8))
				split_block(current_block, size_);//we try to split the block
			current_block->is_free = false;
		}
		else {
			current_block = extend_heap(last, size_);//otherwise, we extend the heap
			if (!current_block) {
				pthread_mutex_unlock(&malloc_lock);//freeing the mutex
				return NULL;
			}
		}
	}
	else {
		current_block = extend_heap(NULL, size_);//otherwise, we extend the heap
		if (!current_block) {
			pthread_mutex_unlock(&malloc_lock);//freeing the mutex
			return NULL;
		}
		head = current_block;
	}
	pthread_mutex_unlock(&malloc_lock);//freeing the mutex
	current_block->pointer = &(current_block->data);
	return current_block->data;
}


//-------IMPLEMENTING CALLOC-------

//calloc does malloc with data initialization to 0s’
void* my_calloc(size_t number_of_elements, size_t size_of_elements) {
	//do the malloc with right size
	size_t* new_ = my_malloc(number_of_elements * size_of_elements);
	if (new_) {
		//the data block size in the chunk is always a multiple of 8, so we'll iterate by 8 bytes steps
		size_t size8 = align8(number_of_elements * size_of_elements) >> 3;
		for (size_t i = 0; i < size8; i++)
			new_[i] = 0;//put 0 on any bytes in the block
	}
	return new_;
}

//when we free a chunk, and its neighbors are also free, we can fusion them in one bigger chunk to solve fragmentation problem
block fusion(block current_block) {
	//if the next chunk is free, we sum the sizes of the current chunk and the next one, plus the meta-data size
	if (current_block->next && current_block->next->is_free) {
		current_block->size += BLOCK_SIZE + current_block->next->size;
		// we make next field point to the successor of our successor
		current_block->next = current_block->next->next;
		//if this successor exists we update its predecessor
		if (current_block->next)
			current_block->next->previous = current_block;
	}
	return current_block;
}

//getting access to the block corresponding to a given pointer
block get_block(void* pointer) {
	pointer -= BLOCK_SIZE;
	return pointer;
}

int valid_address(void* pointer) {
	if (head)
		if (pointer > head && pointer < sbrk(0))
			return pointer == get_block(pointer)->pointer;
	return 0;//if the pointer is outside the heap, it can not be a valid pointer
}


//-------IMPLEMENTING FREE-------

//free() is used to deallocate or release the memory blocks to reduce their wastage
void my_free(void* pointer) {
	block current;

	pthread_mutex_lock(&free_lock);

	if (valid_address(pointer)) {
		current = get_block(pointer);//get the block address
		current->is_free = true;
		if (current->previous && current->previous->is_free)
			current = fusion(current->previous);//if the previous exists and is free, we step backward in the block list and fusion the two blocks
		if (current->next)
			fusion(current);//we also try fusion with then next block
		else {
			if (current->previous)
				current->previous->next = NULL;
			else
				head = NULL;
			brk(current);//if we are at the end of the heap, we just have to put the break at the chunk position with a simple call to brk()
		}
	}

	pthread_mutex_unlock(&free_lock);
}

//copying data from one block to another
void copy_block(block first, block second) {
	int* f_data, * s_data;
	f_data = first->pointer;
	s_data = second->pointer;
	for (size_t i = 0; i * 8 < first->size && i * 8 < first->size; i++)
		s_data[i] = f_data[i];
}


//-------IMPLEMENTING REALLOC-------

void* my_realloc(void* pointer, size_t size) {
	size_t size_;
	block current_block, new_;
	void* newptr;

	if (!pointer)
		return my_malloc(size);//if we pass realloc a NULL pointer, it's supposed to act just like malloc

	if (valid_address(pointer)) {
		size_ = align8(size);
		current_block = get_block(pointer);
		//if we pass it a previously malloced pointer, it should free up space if the size is smaller than the previous size
		if (current_block->size >= size_) {
			if (current_block->size - size_ >= (BLOCK_SIZE + 8))
				split_block(current_block, size_);
		}
		else {
			//if the next block is free and provide enough space, we fusion and try to split if necessary
			if (current_block->next && current_block->next->is_free && (current_block->size + BLOCK_SIZE + current_block->next->size) >= size_) {
				fusion(current_block);
				if (current_block->size - size_ >= (BLOCK_SIZE + 8))
					split_block(current_block, size_);
			}
			else {
				newptr = my_malloc(size_);//allocate a new block using malloc
				if (!newptr)
					return NULL;
				new_ = get_block(newptr);
				copy_block(current_block, new_);//copy data from the old one to the new one
				my_free(pointer);//free the old block
				return newptr;//return the new pointer
			}
		}
		return pointer;
	}
	return NULL;
}
