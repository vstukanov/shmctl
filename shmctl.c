/*
 * main.c
 *
 *  Created on: Apr 3, 2012
 *      Author: valera
 */

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

void* open_table_head(char* path, char proj_id) {
	key_t key;
	if (-1 == (key = ftok(path, (int) proj_id))) {
		perror("ftok");
		return (void *) -1;
	}

	int shmid;
	if ((shmid = shmget(key, 3 * sizeof(uint32_t), 0666)) < 0) {
		perror("shmid");
		return (void *) -1;
	}

	uint32_t* mem;
	if ((mem = (uint32_t *) shmat(shmid, NULL, 0)) == (void *) -1) {
		perror("shmat");
		return (void *) -1;
	}

	if ((mem[1] < mem[0]) || (mem[1] < mem[2])) {
		if (-1 == shmdt(mem)) {
			perror("shmdt");
			return (void *) -1;
		}
		perror("incorrect table head");
		return (void *) -1;
	}

	uint32_t max_shm = get_max_shm_size();
	uint32_t table_size = mem[1];
	if (mem[1] > max_shm) {
		perror("incorrect table head. To big memery size");
		return (void *) -1;
	}

	// printf("table size: %d\nmemory size: %d\nsegment size: %d\n", mem[0], mem[1], mem[2]);

	// Add promt there

	if (-1 == shmdt((void *) mem)) {
		perror("shmdt");
		return (void *) -1;
	}

	if ((shmid = shmget(key, table_size, 0666)) < 0) {
		perror("shmget");
		return (void *) -1;
	}

	void * ret_val;
	if ((ret_val = shmat(shmid, NULL, 0)) == (void *) -1) {
		perror("shmat");
		return (void *) -1;
	}
	return ret_val;
}

void close_table(void * mem) {
	if (-1 == shmdt(mem)) {
		perror("shmdt");
	}
}

void print_table_info(void * mem) {
	uint32_t * u32_mem = (uint32_t *) mem;
	printf("tabel size: %d, memory size: %d, segment size: %d\n", u32_mem[0], u32_mem[1], u32_mem[2]);
	printf("stored entries: %d, used segments: %d, real used memory: %d\n", u32_mem[3], u32_mem[4], u32_mem[5]);
	uint32_t tused = u32_mem[2] * u32_mem[4], mem_lost = tused - u32_mem[5];
	float kpd = 100 - ((float) u32_mem[5] / (float) tused) * 100;
	printf("total used memory: %d, lost memory: %d, lost = %0.2f%s\n", tused, mem_lost, kpd, "%");
}

void print_item(void * mem, uint32_t id) {
	uint32_t* u32_mem = (uint32_t *) mem;
	uint32_t* item_head = mem + 6 * sizeof(uint32_t) + (id - 1) * 3 * sizeof(uint32_t);
	uint32_t item_size = item_head[2];
	printf("id: %d, start: %d, size: %d\n", item_head[0], item_head[1], item_head[2]);
	if (item_head[1] == 0) {
		printf("no item in table\n");
		return;
	}
	uint32_t offset = (u32_mem[0] + 2) * 3 * sizeof(uint32_t);
	void * data_mem = mem + offset + (item_head[1] - 1) * u32_mem[2];
	uint32_t seg_count = 0;
	uint32_t data_len = 0;
	uint32_t data_path[512];
	uint32_t need_to_read = item_head[2];
	data_path[0] = item_head[1];
	printf("\n------------------------------\n");
	while(need_to_read) {
		uint32_t seg_flag = * (uint32_t *) (data_mem + (u32_mem[2] - sizeof(uint32_t)));
		data_path[1 + seg_count++] = seg_flag;
		if (seg_flag == 0) {
			perror("next segment empty");
			// TODO: backtrace
			break;
		}
		if (seg_flag == -1) {
			if (need_to_read > u32_mem[2]) {
				perror("last segment but not all data readed!");
				break;
			}
			fwrite(data_mem, need_to_read, 1, stdout);
			data_len += need_to_read;
			need_to_read -= need_to_read;
			printf("<END>");
			break;
		} else {
			fwrite(data_mem, u32_mem[2] - sizeof(uint32_t), 1, stdout);
			data_mem = mem + offset + (seg_flag -1) * u32_mem[2];
			data_len += u32_mem[2] - sizeof(uint32_t);
			need_to_read -= u32_mem[2] - sizeof(uint32_t);
		}
	}
	printf("\n------------------------------\n");
	printf("segments readed: %d, data readed: %d, need to read: %d\ndata path: ", seg_count, data_len, need_to_read);
	uint32_t i;
	for(i = 0; i < seg_count; i ++) {
		if ((i + 1) == seg_count) {
			printf("%d.\n", data_path[i]);
		} else {
			printf("%d -> ", data_path[i]);
		}
	}
}

uint32_t test_item(void * mem, uint32_t id) {
	uint32_t* u32_mem = (uint32_t *) mem;
	uint32_t* item_head = mem + 6 * sizeof(uint32_t) + (id - 1) * 3 * sizeof(uint32_t);
	uint32_t item_size = item_head[2];
	if (item_head[1] == 0) {
		printf("no item in table\n");
		return 0;
	}
	uint32_t offset = (u32_mem[0] + 2) * 3 * sizeof(uint32_t);
	void * data_mem = mem + offset + (item_head[1] - 1) * u32_mem[2];
	uint32_t need_to_read = item_head[2];
	while(need_to_read) {
		uint32_t seg_flag = * (uint32_t *) (data_mem + (u32_mem[2] - sizeof(uint32_t)));
		if (seg_flag == 0) {
			perror("next segment empty");
			return 0;
		}
		if (seg_flag == -1) {
			if (need_to_read > u32_mem[2]) {
				printf("too early last segment: %d\n", id);
				return 0;
			}
			need_to_read = 0;
		} else {
			data_mem = mem + offset + (seg_flag -1) * u32_mem[2];
			need_to_read -= u32_mem[2] - sizeof(uint32_t);
		}
	}
	return 1;
}

void test_all(void * mem) {
	uint32_t total_count = 0;
	uint32_t success_count = 0;
	uint32_t fail_count = 0;
	uint32_t* u32_mem = (uint32_t *) mem;
	uint32_t* cont_table = (uint32_t *) (mem + 6 * sizeof(uint32_t));
	uint32_t i;
	for(i = 1; i <= u32_mem[0]; i++) {
		if (cont_table[1] != 0) {
			total_count++;
			if (test_item(mem, i)) {
				success_count ++;
			} else {
				fail_count ++;
			}
		}
		cont_table = (uint32_t *)((void *)cont_table + 3 *sizeof(uint32_t));
	}
	printf("total: %d\nsuccess: %d\nfail: %d\n", total_count, success_count, fail_count);
}

int main(int argc, char* argv[]) {
	void * mem = open_table_head("/var/run/30m2k.shm", 'i');
	printf("--- --- --- INFO --- --- ---\n");
	print_table_info(mem);
	printf("--- --- --- TEST --- --- ---\n");
	test_all(mem);
	if (argc > 1) {
		printf("--- --- --- DATA --- --- ---\n");
		print_item(mem, atoi(argv[1]));
	}
	printf("--- --- --- ---- --- --- ---\n");
	if (mem != (void *) -1) {
		close_table(mem);
	}
	return EXIT_SUCCESS;
}
