/* 
 * File:   cs_map_creator.h
 * Author: kirill
 *
 * Created on 17 Май 2011 г., 17:05
 */

#ifndef CS_MAP_CREATOR_H
#define	CS_MAP_CREATOR_H

#ifdef	__cplusplus
extern "C" {
#endif


#define ROOT 2
#define NODE 1

#define REPLICATION 1

    struct crush_map* create_map(int devs_per_node, int root_bucket, int nodes, int node_lvl_bucket);
    void create_rules(struct crush_map* map, int root_id);

#ifdef	__cplusplus
}
#endif

#endif	/* CS_MAP_CREATOR_H */

