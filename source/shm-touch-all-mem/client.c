#include <assert.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>

#include "common/common.h"


#include <sys/mman.h>
#define SHM_SIZE_1M 0x100000 /* 1M */
#define SHM_SIZE_1G 0x40000000 /* 1G */
#define SHM_SIZE_4G 0x100000000 /* 4G */
#define SHM_SIZE_8G 0x200000000 /* 8G */
#define SHM_START1 0x7ffff6cfc000 /**/
#define SHM_START2 0x7fffb8dfc000 /* 0x7fffb7dfc000 will restore to 0x7fffb7dfc000 */
#define SHM_START3 0x7fffb6dfc000 /* 0x7fffb7dfc000 will restore to 0x7fffb7dfc000 */
#define SHM_START4 0x7ffdf7dfc000 /* 8G */
#define SHM_START5 0x7ffdf7c55000 /* 8G */

#define SHM_SIZE SHM_SIZE_4G
//#define SHM_START SHM_START4
#define SHM_START SHM_START5

#ifdef __x86_64__
#define SYSCALL_PCN_DSHM_MMAP   334
#define SYSCALL_PCN_DSHM_MUNMAP   335
#define SYSCALL_GETTID 186
#elif __aarch64__
#define SYSCALL_PCN_DSHM_MMAP   289
#define SYSCALL_PCN_DSHM_MUNMAP   290
#define SYSCALL_GETTID 178
#elif __powerpc64__
#define SYSCALL_PCN_DSHM_MMAP   -1
#define SYSCALL_PCN_DSHM_MUNMAP   -2
#define SYSCALL_GETTID 207
#else
#error Does not support this architecture
#endif

long int popcorn_dshm_mmap(unsigned long addr, unsigned long len,
                    unsigned long prot, unsigned long flags,
                    unsigned long fd, unsigned long pgoff) {
    return syscall(SYSCALL_PCN_DSHM_MMAP, addr, len, prot, flags, fd, pgoff);
}

long int popcorn_dshm_munmap(unsigned long addr, size_t len) {
    return syscall(SYSCALL_PCN_DSHM_MUNMAP, addr, len);
}


void cleanup(char* shared_memory) {
	printf("popcorn_dshm_munmap()\n");
	popcorn_dshm_munmap((unsigned long)shared_memory, SHM_SIZE);
}

void shm_wait(atomic_char* guard) {
	while (atomic_load(guard) != 'c')
		;
}

void shm_notify(atomic_char* guard) {
	atomic_store(guard, 's');
}

void communicate(char* shared_memory, struct Arguments* args) {
	// Buffer into which to read data
	//int i;
	unsigned long i;
	void* buffer = malloc(args->size);

	atomic_char* guard = (atomic_char*)shared_memory;
	atomic_init(guard, 's');
	assert(sizeof(atomic_char) == 1);

	//for (; args->count > 0; --args->count) {
	//for (i = args->count; i > 0; --i) {
	for (i = 0; i < args->count; ++i) {
#if 0 // perf=0
		//printf("[dbg] msg cnt %d/%d\n", i, args->count);
		printf("[dbg] msg cnt %lu/%d pgs (%lu M)\n",
				i, args->count, (i * 4096) / 1024 / 1024);
        if (i <= 100 || i % ( args->count / 100) == 0) {
            printf("\t[dbg] #%lu/%u (<100 || =(count/100)**)\n", i, args->count);
        }
#endif
		shm_wait(guard);
		// Read
		//memcpy(buffer, shared_memory + 1, args->size);
		memcpy(buffer, shared_memory + ((i+1) * 4096), args->size); // init: +1 to avoide sync bit

		// Write back
		//memset(shared_memory + 1, '*', args->size);
		memset(shared_memory + ((i+1) * 4096), '*', args->size);

		shm_notify(guard);
	}

	free(buffer);
}

int main(int argc, char* argv[]) {
	// The *actual* shared memory, that this and other
	// processes can read and write to as if it were
	// any other plain old memory
	char* shared_memory;

	// Fetch command-line arguments
	struct Arguments args;

	parse_arguments(&args, argc, argv);
	printf("args.size = %d args.count = %d\n",
			args.size, args.count);


	shared_memory = (char*)popcorn_dshm_mmap(SHM_START, SHM_SIZE,
			PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (shared_memory) {
        printf("mmap *ptr = %p size = 0x%lx (sizeof shared_memory 0x%lx)\n",
				shared_memory, (unsigned long)SHM_SIZE, sizeof(shared_memory));
    } else {
        printf("ERROR: Got a big problem: NULL returned mmap *ptr\n");
        exit(-1);
    }

	if (shared_memory != (void*)SHM_START) {
        printf("ERROR: Got a mmap *ptr that is not I want (want: %p got: %p)\n",
                    (void*)SHM_START, shared_memory);
        if (shared_memory) {
            printf("popcorn_dshm_munmap()\n");
            popcorn_dshm_munmap((unsigned long)shared_memory, SHM_SIZE);
        }
        printf("ERROR: doesn't get expected returned mmap *ptr\n");
        exit(-1);
    }
	printf("[GOOD] got shared_memory %p == (as expected 0x%lx)\n",
            shared_memory, (unsigned long)SHM_START);

    printf("init: warm up to get rid of pophype dsm bug\n");
	unsigned long i;
	for (i = 0; i < SHM_SIZE; i += 4096) {
		shared_memory[i] = 'a';
	}
    printf("init done\n");

	communicate(shared_memory, &args);

	cleanup(shared_memory);

	return EXIT_SUCCESS;
}
