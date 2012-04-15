#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

uint32_t get_max_shm_size() {
	char buffer[32];
	bzero(buffer, 32);
	char* head = buffer;
	FILE *file = fopen("/proc/sys/kernel/shmmax", "r");
	if (!file) {
		return -1;
	}
	while(fread(head++, 1, 1, file) > 0) { }
	return atoi(buffer);
}

uint32_t shm_table_dump(char * path, char proj_id, FILE* stream) {
	key_t key = ftok(path, proj_id);
	if (key == -1) {
		perror("ftok");
		return EXIT_FAILURE;
	}
	int shmid;
	if ((shmid = shmget(key, 3 * sizeof(uint32_t), 0666)) < 0) {
		perror("shmget");
		return EXIT_FAILURE;
	}
	uint32_t* mem;
	if ((mem = (uint32_t *)(shmat(shmid, NULL, 0)) ) == (uint32_t *)-1) {
		perror("shmat");
		return EXIT_FAILURE;
	}
	uint32_t shmmax = get_max_shm_size();
	uint32_t shm_table_mem_size = mem[1];
	if (mem[1] > shmmax) {
		perror("Size of table more than shmmax in /proc/sys/kernel/shmmax");
		shmdt((void *) mem);
		return EXIT_FAILURE;
	}
	if (-1 == shmdt((void *) mem)) {
		perror("shmdt");
		return EXIT_FAILURE;
	}
	void * shm_table_mem;
	if ((shmid = shmget(key, shm_table_mem_size, 0666)) < 0) {
		perror("shmget");
		return EXIT_FAILURE;
	}
	if ((shm_table_mem = shmat(shmid, NULL, 0)) == (void *) -1) {
		perror("shmat");
		return EXIT_FAILURE;
	}
	size_t writed = fwrite(shm_table_mem, shm_table_mem_size, 1, stream);
	if (writed != 1) {
		perror("fwrite");
		shmdt(shm_table_mem);
		return EXIT_FAILURE;
	}
	if (-1 == shmdt(shm_table_mem)) {
		perror("shmdt");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		printf("usage: [path] [proj_id]\n");
		return EXIT_FAILURE;
	}
	shm_table_dump(argv[1], argv[2][0], stdout);
	return EXIT_SUCCESS;
}