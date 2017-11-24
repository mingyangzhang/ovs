#include <config.h>
#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>

#include "cache.h"
#include "flow.h"
#include "dpif.h"
#include "dp-packet.h"
#include "packets.h"

#define BOOL int
#define TRUE 1
#define FALSE 0

uint32_t QUEUE_ID = 0;

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
    union flow_in_port in_port; /* Input port.*/
    struct eth_addr dl_dst;     /* Ethernet destination address. */
    struct eth_addr dl_src;     /* Ethernet source address. */
    ovs_be32 nw_src;            /* IPv4 source address. */
    ovs_be32 nw_dst;            /* IPv4 destination address. */
    ovs_be16 tp_src;            /* TCP/UDP/SCTP source port/ICMP type. */
    ovs_be16 tp_dst;            /* TCP/UDP/SCTP destination port/ICMP code. */
};

struct cache_table_head {
    uint32_t queue_id;
    int num_of_packet;
    struct cache_table_head *next;
    struct cache_key *key;
    struct cache_node *queue_head;
    struct cache_node *queue_tail;
};

struct cache_node {
    struct cache_node *next;
    struct dp_packet *pckt;
};

typedef struct cache_node* Node;
typedef struct cache_table_head* QueueHead;

BOOL compare_eth_addr(const struct eth_addr* eth_addr1, const struct eth_addr* eth_addr2){
    BOOL eth_addr_equal = TRUE;
    int i;
    for(i=0;i<3;i++){
        if(eth_addr1->be16[i] != eth_addr2->be16[i]){
            eth_addr_equal = FALSE;
            break;
        }
    }
    return eth_addr_equal;
}

BOOL compare_cache_key(const struct cache_key *key1, const struct cache_key *key2){
    // if(key1->in_port.ofp_port != key2->in_port.ofp_port)
    //     return FALSE;
    if(compare_eth_addr(&key1->dl_dst, &key2->dl_dst)!=TRUE)
        return FALSE;
    if(compare_eth_addr(&key1->dl_src, &key2->dl_src)!=TRUE)
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

uint32_t cache_enqueue(struct flow *flow, const struct dp_packet *packet){
    struct cache_table_head *head = table.head;
    struct cache_key *upcall_key = (struct cache_key *)malloc(sizeof(struct cache_key));
    upcall_key->in_port = flow->in_port;
    upcall_key->dl_dst = flow->dl_dst;
    upcall_key->dl_src = flow->dl_src;
    upcall_key->nw_src = flow->nw_src;
    upcall_key->nw_dst = flow->nw_dst;
    upcall_key->tp_src = flow->tp_src;
    upcall_key->tp_dst = flow->tp_dst;

    while(head != NULL){
       if(compare_cache_key(head->key, upcall_key)==TRUE){
            Node node = (Node)malloc(sizeof(struct cache_node));
            node->next = NULL;
            node->pckt = packet;
            if(head->queue_head==NULL){
                head->queue_head = node;
                head->queue_tail = node;
                head->num_of_packet = 1;
                return UINT32_MAX;
            }
            head->queue_tail->next = node;
            head->queue_tail = node;
            head->num_of_packet++;
            return UINT32_MAX;
       }
       head = head->next;
    }

    QueueHead qhead = (QueueHead)malloc(sizeof(struct cache_table_head));
    qhead->key = upcall_key;
    print_flow_key(upcall_key);
    Node node = (Node)malloc(sizeof(struct cache_node));
    node->next = NULL;
    node->pckt = packet;
    qhead->queue_head = node;
    qhead->queue_tail = qhead->queue_head;
    qhead->queue_id = QUEUE_ID++;
    qhead->next = NULL;
    qhead->num_of_packet = 1;
    if(table.head == NULL){
       table.head = qhead;
       table.tail = table.head;
       table.num_of_queue++;
       return qhead->queue_id;
    }

    table.tail->next = qhead;
    table.tail = qhead;
    table.num_of_queue++;
    return qhead->queue_id;
}

struct dp_packet* cache_pop(uint32_t queue_id){
    QueueHead qhead = table.head;
    while(qhead!=NULL && qhead->queue_id!=queue_id){
        qhead = qhead->next;
    }
    if(qhead==NULL || qhead->queue_head==NULL){
        return NULL;
    }
    Node p = qhead->queue_head;
    qhead->queue_head = p->next;
    struct dp_packet * packet = p->pckt;
    free(p);
    qhead->num_of_packet--;
    return packet;
}

uint32_t lookup_in_queue(struct flow *flow){
    struct cache_key *key = (struct cache_key *)malloc(sizeof(struct cache_key));
    key->in_port = flow->in_port;
    key->dl_dst = flow->dl_dst;
    key->dl_src = flow->dl_src;
    key->nw_src = flow->nw_src;
    key->nw_dst = flow->nw_dst;
    key->tp_src = flow->tp_src;
    key->tp_dst = flow->tp_dst;
    printf("\nlook up in queue");
    print_flow_key(key);
    struct cache_table_head *head = table.head;
    uint32_t queue_id = UINT32_MAX;
    while(head != NULL){
       if(compare_cache_key(head->key, key)==TRUE && head->queue_head!=NULL){
            queue_id = head->queue_id;
            break;
       }
       head = head->next;
    }
    free(key);
    return queue_id;
}

int num_of_queue(){
    return table.num_of_queue;
}


void print_table_info(){
    printf("\n****************Info about cache table****************\n");
    printf("queue number: %d\n", table.num_of_queue);
    QueueHead head = table.head;
    while(head!=NULL){
        printf("queue id: % " PRIu32 "\n", head->queue_id);
        printf("packet number: %d\n", head->num_of_packet);
        print_flow_key(head->key);
        head = head->next;
    }
    printf("\n*****************************************************\n");
}

void print_flow_key(struct cache_key* key){
    printf("\n--------Info about flow key--------\n");
    printf("nw_src: %" PRIu32 "\n", key->nw_src);
    printf("nw_dst: %" PRIu32 "\n", key->nw_dst);
    printf("tp_src: %" PRIu16 "\n", key->tp_src);
    printf("tp_dst: %" PRIu16 "\n", key->tp_dst);

    printf("dl_src: ");
    int i;
    for(i=0;i<3;i++){
        printf("%" PRIu16 ".", key->dl_src.be16[i]);
    }
    printf("\n");
    printf("dl_dst: ");
    for(i=0;i<3;i++){
        printf("%" PRIu16 ".", key->dl_dst.be16[i]);
    }
    printf("\n");
}
