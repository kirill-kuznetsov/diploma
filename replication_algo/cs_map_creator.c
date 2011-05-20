#include "crush.h"
#include "builder.h"
#include "asm-generic/errno-base.h"
#include "hash.h"
#include "cs_map_creator.h"

void create_rules(struct crush_map *map, int root_id);
struct crush_map *create_map(int root_bucket, int nodes,
        int node_lvl_bucket, int devs_per_node)
{

    struct crush_map *map = crush_create();
    int i, j;
    int root_weights[nodes];
    int root_items[nodes];
    int dev_id = 0, id = 0; //zero is for auto id generation
    int root_id;

    if (nodes > 1) {
        for (i = 0; i < nodes; i++) {

            int items[devs_per_node], weights[devs_per_node];
            j = 0;
            root_weights[i] = 0;
            for (j = 0; j < devs_per_node; j++, dev_id++) {
                if (dev_id == devs_per_node * nodes) break;
                items[j] = dev_id;
                weights[j] = 0x10000;
                //w[j] = weights[o] ? (0x10000 - (int)(weights[o] * 0x10000)):0x10000;
                //rweights[i] += w[j];
                root_weights[i] += 0x10000;
            }

            struct crush_bucket *node = crush_make_bucket(node_lvl_bucket, CRUSH_HASH_DEFAULT, NODE, j, items, weights);
            root_items[i] = crush_add_bucket(map, id, node);
        }

        // root
        struct crush_bucket *root = crush_make_bucket(root_bucket, CRUSH_HASH_DEFAULT, ROOT, nodes, root_items, root_weights);
        root_id = crush_add_bucket(map, id, root);
    } else {
        // one bucket
        int items[devs_per_node];
        int weights[devs_per_node];
        for (i = 0; i < devs_per_node; i++) {
            items[i] = i;
            weights[i] = 0x10000;
        }
        struct crush_bucket *b = crush_make_bucket(root_bucket, CRUSH_HASH_DEFAULT, ROOT, devs_per_node, items, weights);
        root_id = crush_add_bucket(map, 0, b);
    }
    create_rules(map, root_id);
    crush_finalize(map);
    return map;
}

void create_rules(struct crush_map *map, int root_id)
{
    int i;
    int rulesets = 1;
    int length = 3;
    int min_rep = 1;
    int max_rep = map->max_devices;
    int nodes_replica = 4;

    for (i = 0; i < rulesets; i++) {
        struct crush_rule *rule = crush_make_rule(length, i, REPLICATION, min_rep, max_rep);
        crush_rule_set_step(rule, 0, CRUSH_RULE_TAKE, root_id, 0);
        crush_rule_set_step(rule, 1, CRUSH_RULE_CHOOSE_LEAF_FIRSTN, CRUSH_CHOOSE_N, 1); // choose N nodes
        crush_rule_set_step(rule, 2, CRUSH_RULE_EMIT, 0, 0);
        int rno = crush_add_rule(map, rule, -1);
    }
    //one bucket
    /*
            for (map<int,const char*>::iterator p = rulesets.begin(); p != rulesets.end(); p++) {
              int ruleset = p->first;
              crush_rule *rule = crush_make_rule(3, ruleset, CEPH_PG_TYPE_REP, g_conf.osd_min_rep, maxrep);
              crush_rule_set_step(rule, 0, CRUSH_RULE_TAKE, rootid, 0);
              crush_rule_set_step(rule, 1, CRUSH_RULE_CHOOSE_FIRSTN, CRUSH_CHOOSE_N, 0);
              crush_rule_set_step(rule, 2, CRUSH_RULE_EMIT, 0, 0);
              int rno = crush_add_rule(crush.crush, rule, -1);
              crush.set_rule_name(rno, p->second);
            }
          }
     */
}
