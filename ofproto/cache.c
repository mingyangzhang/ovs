#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>

#include "flow.h"
#include "dp-packet.h"

#define BOOL int
#define TRUE 1
#define FALSE 0

struct cache_table {
    int num_of_queue;
    struct cache_table_head *head;
    struct cache_table_head *tail;

} table = {
        .num_of_queue = 0,
        .head = NULL,
        .tail = NULL,
};

struct cache_key {
    union flow_in_port in_port;    /* Input port.*/
    struct eth_addr dl_dst;     /* Ethernet destination address. */
    struct eth_addr dl_src;     /* Ethernet source address. */
    ovs_be32 nw_src;     /* IPv4 source address. */
    ovs_be32 nw_dst;     /* IPv4 destination address. */
    ovs_be16 tp_src;     /* TCP/UDP/SCTP source port/ICMP type. */
    ovs_be16 tp_dst;     /* TCP/UDP/SCTP destination port/ICMP code. */
};

struct cache_table_head {
    int queue_id;
    struct cache_table_head *next;
    struct cache_key *key;
    struct cache_node *queue_head;
    struct cache_node *queue_tail;
};

struct cache_node {
    struct cache_node *next;
    dp_packet *pckt;
};

typedef struct cache_node* Node;
typedef struct cache_table_head* QueueHead;

BOOL compare_cache_key(struct cache_key *key1, struct cache_key *key2){
    if(key1->in_port != key2->in_port)
        return FALSE;
    if(key1->dl_dst != key2->dl_dst)
        return FALSE;
    if(key1->dl_src != key2->dl_src)
        return FALSE;
    if(key1->nw_src != key2->nw_src)
        return FALSE;
    if(key1->nw_dst != key2->nw_dst)
        return FALSE;
    if(key1->tp_src != key2->tp_src)
        return FALSE;
    if(key1->tp_dst != key2->tp_dst)
        return FALSE;
    return TRUE;
}

void cache_enqueue(const struct flow *flow, const struct dp_packet *packet){
    printf("\n\n\n\ncache enqueque\n\n\n\n");
//    struct cache_table_head *head = table.head;
//    struct cache_key upcall_key = {
//        .in_port = flow->in_port,
//        .dl_dst = flow->dl_dst,
//        .dl_src = flow->dl_src,
//        .nw_src = flow->nw_src,
//        .nw_dst = flow->nw_dst,
//        .tp_src = flow->tp_src,
//        .tp_dst = flow->tp_dst,
//    };
//    while(head != NULL){
//        if(compare_cache_key(head->key, &upcall_key)){
//            Node node = (Node)malloc(sizeof(struct cache_node));
//            node->next = NULL;
//            node->pckt = packet;
//            head->queue_tail->next = node;
//            head->queue_tail = node;
//            return;
//        }
//        head = head->next;
//    }
//    QueueHead qhead = (QueueHead)malloc(sizeof(struct cache_table_head));
//    qhead->key = upcall_key;
//    Node node = (Node)malloc(sizeof(struct cache_node));
//    node->next = NULL;
//    node->pckt = packet;
//    qhead->queue_head = node;
//    qhead->queue_tail = qhead->queue_head;
//    qhead->queue_id = table.num_of_queue;
//    qhead->next = NULL;
//    if(table.head == NULL){
//        table.head = qhead;
//        table.tail = table.head;
//        table.num_of_queue++;
//        return;
//    }
//
//    table.tail->next = qhead;
//    table.tail = qhead;
//    table.num_of_queue++;
}

dp_packet* cache_pop(int queue_id){
    if(queue_id<0 || queue_id>table.num_of_queue-1){
        return NULL;
    }
    QueueHead qhead = table.head;
    while(queue_id-->0){
        qhead = qhead->next;
    }
    if(qhead->queue_head==NULL){
        return NULL;
    }
    Node p = qhead->queue_head;
    qhead->queue_head = p->next;
    dp_packet * packet = p->pckt;
    free(p);
    return packet;
}
