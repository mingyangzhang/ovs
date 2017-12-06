#include <config.h>
#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>

#include "cache.h"
#include "flow.h"
#include "dpif.h"
#include "dp-packet.h"
#include "packets.h"
#include "settings.h"

#define BOOL int
#define TRUE 1
#define FALSE 0

uint32_t QUEUE_ID;

/*packet from this port should be locked in initialization*/

struct cache_table {
    int num_of_queue;
    struct cache_table_head *head;
    struct cache_table_head *tail;
    struct cache_table_head *locked_queue;

} table = {
        .num_of_queue = 0,
        .head = NULL,
        .tail = NULL,
        .locked_queue = NULL,
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
    BOOL pckt_in;
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

BOOL is_flood(const struct eth_addr* eth_addr){
    BOOL is_flood = TRUE;
    int i;
    for(i=0;i<3;i++){
        if(eth_addr->be16[i] != 65535){
            is_flood = FALSE;
            break;
        }
    }
    return is_flood;
}

uint32_t queue_id(QueueHead head){
    if(head->pckt_in==TRUE){
        head->pckt_in = FALSE;
        return head->queue_id;
    }
    return UINT32_MAX;
}

void unlocked_queue(){
    if(table.locked_queue != NULL && table.locked_queue->pckt_in == FALSE) {
        printf("unlock queue %d\n", table.locked_queue->queue_id);
        table.locked_queue->pckt_in = TRUE;
        lock_port = 0;
        lock_ip = 0;
    }
}

BOOL compare_cache_key(const struct cache_key *key1, const struct cache_key *key2, BOOL inport){
    if(key1->in_port.ofp_port != key2->in_port.ofp_port && inport==TRUE)
        return FALSE;
    if(compare_eth_addr(&key1->dl_dst, &key2->dl_dst)!=TRUE)
        return FALSE;
    if(compare_eth_addr(&key1->dl_src, &key2->dl_src)!=TRUE)
        return FALSE;
/*    if(key1->nw_src != key2->nw_src)
        return FALSE;
    if(key1->nw_dst != key2->nw_dst)
        return FALSE;
    if(key1->tp_src != key2->tp_src)
        return FALSE;
    if(key1->tp_dst != key2->tp_dst)
        return FALSE; */
    return TRUE;
}

uint32_t cache_enqueue(struct flow *flow, const struct dp_packet *packet){
    if(is_flood(&flow->dl_dst)==TRUE){
        // if it is a flood packet, do not enqueue
        return UINT32_MAX - 1;
    }
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
       if(compare_cache_key(head->key, upcall_key, TRUE)==TRUE){
            Node node = (Node)malloc(sizeof(struct cache_node));
            node->next = NULL;
            node->pckt = packet;
            if(head->queue_head==NULL){
                head->queue_head = node;
                head->queue_tail = node;
                head->num_of_packet = 1;
                return queue_id(head);
            }
            head->queue_tail->next = node;
            head->queue_tail = node;
            head->num_of_packet++;
            return queue_id(head);
       }
       head = head->next;
    }

    QueueHead qhead = (QueueHead)malloc(sizeof(struct cache_table_head));
    qhead->key = upcall_key;
    qhead->pckt_in = TRUE;
    Node node = (Node)malloc(sizeof(struct cache_node));
    node->next = NULL;
    node->pckt = packet;
    qhead->queue_head = node;
    qhead->queue_tail = qhead->queue_head;
    qhead->queue_id = QUEUE_ID++;
    qhead->next = NULL;
    qhead->num_of_packet = 1;
    if(upcall_key->nw_src == lock_ip){
        qhead->pckt_in = FALSE;
        printf("lock queue %d\n", qhead->queue_id);
        table.locked_queue = qhead;
    }
    if(table.head == NULL){
       table.head = qhead;
       table.tail = table.head;
       table.num_of_queue++;
       return queue_id(qhead);
    }

    table.tail->next = qhead;
    table.tail = qhead;
    table.num_of_queue++;
    return queue_id(qhead);
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
    // printf("\nlook up in queue");
    // print_flow_key(key);
    struct cache_table_head *head = table.head;
    uint32_t queue_id = UINT32_MAX;
    while(head != NULL){
       if(compare_cache_key(head->key, key, FALSE)==TRUE && head->queue_head!=NULL){
            //printf("packet number: %d\n", head->num_of_packet);
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
    printf("in_port: %" PRIu32 "\n", key->in_port.ofp_port);
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
