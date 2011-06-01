#ifndef CS_CRUSH_TESTER_H
#define	CS_CRUSH_TESTER_H

#ifdef	__cplusplus
extern "C" {
#endif

    //TODO:
#define REPLICAS 3
#define RULE_ID  0

#define MAX_NODES 256
#define MAX_DEVS 256

void print_device_status(int *status, int n);
void test_all(int min_nodes, int max_nodes, int min_dpn, int max_dpn, int blocks_num);

inline long long get_time();
struct map_test_data
{
    int write_time;
    float disbalance;
    struct migration_data *extra_node_migration;
    struct migration_data *extra_device_migration;

    struct crush_map *map;
    struct crush_map *extra_node_map;
    struct crush_map *extra_device_map;
};

struct migration_data
{
    float migrants;// % of blocks migrated inside node
    float emigrants;// % of blocks migrated between node
};

#ifdef	__cplusplus
}
#endif

#endif	/* CS_CRUSH_TESTER_H */

