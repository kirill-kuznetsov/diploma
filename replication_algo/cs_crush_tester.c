/*
 * File:   crush_cs_tester.c
 * Author: kirill
 *
 * Created on 15 Май 2011 г., 21:44
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>

#include "crush.h"
#include "cs_crush.h"
#include "cs_crush_tester.h"
#include "cs_map_creator.h"

/*
 *
 */
inline long long get_time();
void test_map(int devs_per_node, int root_bucket, int nodes,
        int node_lvl_bucket, int blocks_num, struct map_test_data * test_data);
int test_write(struct crush_map* map, int *all_devices, int blocks_num);
void test_remap( int blocks_num, struct map_test_data* test_data);
void print_device_status(int *result, int replica_num);
void estimate_migration(struct crush_map * m1, struct crush_map *m2, int block_id,
        int replica_num, struct migration_data* test_data);
void estimate_specific_migration(struct crush_map * m1, struct crush_map *m2, int block_id,
        int replica_num, struct migration_data* test_data, int *result1,
        int *result2);
int max_disbalance(int *all_devices, int dev_num, int blocks_num);

int main(int argc, char** argv) {

    return (EXIT_SUCCESS);
}

void test_all() {
}

void test_map(int devs_per_node, int root_bucket, int nodes,
        int node_lvl_bucket, int blocks_num, struct map_test_data* test_data) {
    struct crush_map *map = create_map(devs_per_node, root_bucket, nodes,
            node_lvl_bucket);
    //maps for remapping test
    struct crush_map *extra_node_map = create_map(devs_per_node, root_bucket, nodes + 1,
            node_lvl_bucket);
    //TODO: extra devices remapping
    struct crush_map *extra_device_map = create_map(devs_per_node, root_bucket, nodes,
            node_lvl_bucket);

    int rule_id = 0;
    int dev_num = nodes*devs_per_node;
    int all_devices[dev_num];
    memset(all_devices, 0, sizeof (int));
    test_data->write_time = test_write(map, all_devices, rule_id);
    test_data->disbalance = max_disbalance(all_devices, dev_num, blocks_num);
            //test_remap(map, extra_node_map, extra_device_map, blocks_num )
            crush_destroy(map);
    //TODO: destroy maps for remapping
}

int test_write(struct crush_map* map, int *all_devices, int blocks_num) {
    long long start_time = get_time();
    int i, j;

    int result[REPLICAS];
    for (i = 0; i < blocks_num; i++) {
        cs_map_input(map, i, RULE_ID, REPLICAS, result);
        //count mapped block for each device
        for (j = 0; j < REPLICAS; j++) {
            all_devices[result[j]]++;
        }
    }
    long long mapping_time = get_time() - start_time;
    int avg_time = (int) mapping_time / blocks_num;
    //printf("Map time %d\n", avg_time);
    return avg_time;
}

void test_remap( int blocks_num, struct map_test_data* test_data) {
    int i;
    int replica_num = REPLICAS;
    for (i = 0; i < blocks_num; i++) {
        estimate_migration(test_data->map, test_data -> extra_node_map, i,replica_num,
                test_data->extra_node_migration);
        estimate_migration(test_data->map, test_data -> extra_device_map,i,replica_num,
                test_data->extra_device_migration);
    }
}

void estimate_migration(struct crush_map * m1, struct crush_map *m2, int block_id,
        int replica_num, struct migration_data* test_data) {
    int result1[replica_num];
    int result2[replica_num];
    //distribute replicas
    cs_map_input(m1, block_id, RULE_ID, replica_num, result1);
    cs_map_input(m2, block_id, RULE_ID, replica_num, result2);
    estimate_specific_migration( m1, m2, block_id, replica_num, test_data, result1, result2);
}

void estimate_specific_migration(struct crush_map * m1, struct crush_map *m2, int block_id,
        int replica_num, struct migration_data* test_data, int *result1,
        int *result2)
{
        //try to find all replicas on second distribution, lying in the same node buckets,
    //and all lying in other buckets
    int remaped_in_node = 0;
    int remaped_in_root = replica_num;
    int i, j;
    int is_not_remaped[replica_num]; //replica in second distribution is on same device
    int is_remaped_within_node[replica_num]; //replica in second distribution is in same node bucket
    memset(is_not_remaped, 0, replica_num * sizeof (int));
    memset(is_remaped_within_node, 0, replica_num * sizeof (int));
    for (i = 0; i < replica_num; i++) {
        for (j = 0; j < replica_num; j++) {
            //same device
            if (result1[i] == result2[j]) {
                if (is_remaped_within_node[j]) {
                    is_remaped_within_node[j] = 0;
                    remaped_in_node--;
                } else {
                    remaped_in_root--;
                }
                is_not_remaped[j] = 1;
                assert(remaped_in_root >= 0);
            }                //same bucket
            else if (m1->device_parents[result1[i]] == m2->device_parents[result2[j]]) {
                //if not marked already
                if (!is_remaped_within_node[j] && !is_not_remaped[j]) {
                    is_remaped_within_node[j] = 1;
                    remaped_in_root--;
                    remaped_in_node++;
                    assert(remaped_in_root >= 0 && remaped_in_node <= replica_num);
                }
            }
        }
    }
    test_data->emigrants = remaped_in_root;
    test_data->migrants = remaped_in_node;
}

int max_disbalance(int *all_devices, int dev_num, int blocks_num) {
    int min = blocks_num;
    int max = 0;
    int i;
    for (i = 0; i < dev_num; i++) {
        min = min > all_devices[i] ? all_devices[i] : min;
        max = max < all_devices[i] ? all_devices[i] : max;
    }
    return ((max - min) * 100) / blocks_num;
}

void print_device_status(int *status, int n) {
    int i;
    for (i = 0; i < n; i++) {
        printf("Device %d: %d blocks\n", i, status[i]);
    }
}

inline long long get_time() {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (long long) tv.tv_sec * 1000000LL + (long long) tv.tv_usec;
}


