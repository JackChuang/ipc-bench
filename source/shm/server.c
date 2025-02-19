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
#define SHM_SIZE_8G 0x200000000 /* 8G */
#define SHM_START1 0x7ffff6cfc000 /**/
#define SHM_START2 0x7fffb8dfc000 /* 0x7fffb7dfc000 will restore to 0x7fffb7dfc000 */
#define SHM_START3 0x7fffb6dfc000 /* 0x7fffb7dfc000 will restore to 0x7fffb7dfc000 */
#define SHM_START4 0x7ffdf7dfc000 /* 8G */
#define SHM_START5 0x7ffdf7c55000 /* 8G */

#define SHM_SIZE SHM_SIZE_8G
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


//void cleanup(int segment_id, char* shared_memory) {
void cleanup(char* shared_memory) {
	// Detach the shared memory from this process' address space.
	// If this is the last process using this shared memory, it is removed.
//	shmdt(shared_memory);

	/*
		Deallocate manually for security. We pass:
			1. The shared memory ID returned by shmget.
			2. The IPC_RMID flag to schedule removal/deallocation
				 of the shared memory.
			3. NULL to the last struct parameter, as it is not relevant
				 for deletion (it is populated with certain fields for other
				 calls, notably IPC_STAT, where you would pass a struct shmid_ds*).
	*/
//	shmctl(segment_id, IPC_RMID, NULL);
	printf("popcorn_dshm_munmap()\n");
	popcorn_dshm_munmap((unsigned long)shared_memory, SHM_SIZE);
}

void shm_wait(atomic_char* guard) {
	while (atomic_load(guard) != 's')
		;
}

void shm_notify(atomic_char* guard) {
	atomic_store(guard, 'c');
}

void communicate(char* shared_memory, struct Arguments* args) {
	struct Benchmarks bench;
	int message;
	void* buffer = malloc(args->size);
	atomic_char* guard = (atomic_char*)shared_memory;

	// Wait for signal from client
	shm_wait(guard);
	setup_benchmarks(&bench);

	for (message = 0; message < args->count; ++message) {
		//printf("[dbg] msg cnt %d\n", message);
		bench.single_start = now();

		// Write
		memset(shared_memory + 1, '*', args->size);

		shm_notify(guard);
		shm_wait(guard);

		// Read
		memcpy(buffer, shared_memory + 1, args->size);

		benchmark(&bench);
	}

	evaluate(&bench, args);
	free(buffer);
}

int main(int argc, char* argv[]) {
	// The identifier for the shared memory segment
//	int segment_id;

	// The *actual* shared memory, that this and other
	// processes can read and write to as if it were
	// any other plain old memory
	char* shared_memory;

	// Key for the memory segment
//	key_t segment_key;

	// Fetch command-line arguments
	struct Arguments args;
	parse_arguments(&args, argc, argv);
    printf("args.size = %d args.count = %d\n",
            args.size, args.count);

//	segment_key = generate_key("shm");

	/*
		The call that actually allocates the shared memory segment.
		Arguments:
			1. The shared memory key. This must be unique across the OS.
			2. The number of bytes to allocate. This will be rounded up to the OS'
				 pages size for alignment purposes.
			3. The creation flags and permission bits, where:
				 - IPC_CREAT means that a new segment is to be created
				 - IPC_EXCL means that the call will fail if
					 the segment-key is already taken (removed)
				 - 0666 means read + write permission for user, group and world.
		When the shared memory key already exists, this call will fail. To see
		which keys are currently in use, and to remove a certain segment, you
		can use the following shell commands:
			- Use `ipcs -m` to show shared memory segments and their IDs
			- Use `ipcrm -m <segment_id>` to remove/deallocate a shared memory segment
	*/
//	segment_id = shmget(segment_key, 1 + args.size, IPC_CREAT | 0666);

//	if (segment_id < 0) {
//		throw("Error allocating segment");
//	}

	/*
		Once the shared memory segment has been created, it must be
		attached to the address space of each process that wishes to
		use it. For this, we pass:
			1. The segment ID returned by shmget
			2. A pointer at which to attach the shared memory segment. This
				 address must be page-aligned. If you simply pass NULL, the OS
				 will find a suitable region to attach the segment.
			3. Flags, such as:
				 - SHM_RND: round the second argument (the address at which to
					 attach) down to a multiple of the page size. If you don't
					 pass this flag but specify a non-null address as second argument
					 you must ensure page-alignment yourself.
				 - SHM_RDONLY: attach for reading only (independent of access bits)
		shmat will return a pointer to the address space at which it attached the
		shared memory. Children processes created with fork() inherit this segment.
	*/
//	shared_memory = (char*)shmat(segment_id, NULL, 0);
	shared_memory = (char*)popcorn_dshm_mmap(SHM_START, SHM_SIZE,
			PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

//	if (shared_memory == (char*)-1) {
//		throw("Error attaching segment");
//	}
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
    printf("got shared_memory %p == (expect 0x%lx)\n",
			shared_memory, (unsigned long)SHM_START);

	communicate(shared_memory, &args);

	//cleanup(segment_id, shared_memory);
	cleanup(shared_memory);

	return EXIT_SUCCESS;
}
