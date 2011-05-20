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
void test_map(int blocks_num, struct map_test_data *test_data );
int test_write(struct crush_map* map, int *all_devices, int blocks_num);
void test_remap( int blocks_num, struct map_test_data* test_data);
void print_device_status(int *result, int replica_num);
void estimate_migration(struct crush_map * m1, struct crush_map *m2, int block_id,
        int replica_num, struct migration_data* test_data);
void estimate_specific_migration(struct crush_map * m1, struct crush_map *m2, int block_id,
        int replica_num, struct migration_data* test_data, int *result1,
        int *result2);
int max_disbalance(int *all_devices, int dev_num, int blocks_num);
void create_extra_maps( int root_bucket, int nodes,
        int node_lvl_bucket, int devs_per_node, struct map_test_data* test_data);
struct map_test_data * init_test_data(struct crush_map *map);
void free_test_data(struct map_test_data* test_data);
void print_test_results( int root_bucket,
    int node_bucket, int nodes, int devs_per_node, struct map_test_data *test_data);

void test_all(int min_nodes, int max_nodes, int min_dpn, int max_dpn, int blocks_num)
{
    int node_bucket, root_bucket, nodes, devs_per_node;
    for(root_bucket = CRUSH_BUCKET_LIST; root_bucket < CRUSH_NO_BUCKETS; root_bucket++)
    {
        for(node_bucket = CRUSH_BUCKET_LIST; node_bucket < CRUSH_NO_BUCKETS; node_bucket++)
        {
            for(nodes = min_nodes; nodes < max_nodes; nodes++)
            {
                //TODO:
                for(devs_per_node = min_dpn; devs_per_node < max_dpn; devs_per_node++)
                {
                    struct crush_map *map = create_map(root_bucket, nodes, node_bucket, devs_per_node );
                    struct map_test_data *test_data = init_test_data(map);
                    create_extra_maps( root_bucket, nodes, node_bucket, devs_per_node, test_data);
                    //test_map(blocks_num, test_data);
                    print_test_results( root_bucket, nodes, node_bucket, devs_per_node, test_data);
                    free_test_data(test_data);
                }
            }
        }
    }
}

void test_map(int blocks_num, struct map_test_data *test_data )
{
    int dev_num = test_data->map->max_devices;
    int all_devices[dev_num];
    memset(all_devices, 0, sizeof (all_devices));
    //writing
    test_data->write_time = test_write( test_data->map, all_devices, blocks_num);
    test_data->disbalance = max_disbalance(all_devices, dev_num, blocks_num);
    //estimate redistribution expenses due to cluster changes

    test_remap( blocks_num, test_data);
}

struct map_test_data *init_test_data(struct crush_map *map)
{
    struct migration_data *extra_node_migration = malloc(sizeof(*extra_node_migration));
    memset(extra_node_migration, 0, sizeof(*extra_node_migration));
    struct migration_data *extra_device_migration = malloc(sizeof(*extra_device_migration));
    memset(extra_device_migration, 0, sizeof(*extra_device_migration));
    struct map_test_data *test_data = malloc(sizeof(*test_data));
    memset(test_data, 0 , sizeof(*test_data));
    test_data->extra_device_migration = extra_device_migration;
    test_data->extra_node_migration = extra_node_migration;
    test_data->map = map;
    return test_data;
}

void create_extra_maps( int root_bucket, int nodes,
        int node_lvl_bucket, int devs_per_node,  struct map_test_data* test_data)
{
    test_data->extra_node_map = create_map( root_bucket, nodes + 1,
            node_lvl_bucket, devs_per_node);
    test_data->extra_device_map = create_map(root_bucket, nodes,
            node_lvl_bucket, devs_per_node + 1);
}

int test_write(struct crush_map* map, int *all_devices, int blocks_num) {
    long long start_time = get_time();
    int i, j;

    int result[REPLICAS];
    for (i = 0; i < blocks_num; i++) {
        printf("block %i", i);
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
    test_data ->extra_device_migration->emigrants *= 100/blocks_num;
    test_data ->extra_device_migration->migrants *= 100/blocks_num;
    test_data ->extra_node_migration->emigrants *= 100/blocks_num;
    test_data ->extra_node_migration->migrants *= 100/blocks_num;
}

float get_presentage(int partp, int whole)
{
    return partp * 100/whole;
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
    memset(is_not_remaped, 0, sizeof (is_not_remaped));
    memset(is_remaped_within_node, 0, sizeof (is_remaped_within_node));
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
    test_data -> emigrants += remaped_in_root ;
    test_data -> migrants += remaped_in_node;
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

void free_test_data(struct map_test_data* test_data)
{
    crush_destroy(test_data->map);
    crush_destroy(test_data->extra_device_map);
    crush_destroy(test_data->extra_node_map);
    free(test_data->extra_device_migration);
    free(test_data->extra_node_migration);
    free(test_data);
}

void assign_type(int bucket, char *type);

void print_test_results( int root_bucket,
    int node_bucket, int nodes, int devs_per_node, struct map_test_data *test_data)
{
    char root_type[20], node_type[20];
    assign_type(root_bucket, root_type);
    assign_type(node_bucket, node_type);

    printf("Map with %s at root, %s at nodes, with %d nodes and %d devices per node\n",
        root_type, node_type, nodes, devs_per_node);
    printf("    Write time: %d, disbalance: %f\n", test_data->write_time, test_data -> disbalance);
    printf("    Migration after node addition: %f%% within bucket, %f%% within node",
        test_data->extra_node_migration->migrants, test_data->extra_node_migration->emigrants);
    printf("    Migration after device addition: %f%% within bucket, %f%% within node",
        test_data->extra_device_migration->migrants, test_data->extra_device_migration->emigrants);
}

void assign_type(int bucket, char *type)
{
    switch( bucket)
    {
            case CRUSH_BUCKET_UNIFORM: type = "UNIFORM BUCKET"; break;
            case CRUSH_BUCKET_LIST: type = "LIST_BUCKET"; break;
            case CRUSH_BUCKET_TREE: type = "TREE_BUCKET"; break;
            case CRUSH_BUCKET_STRAW: type = "STRAW_BUCKET"; break;
    }
}

