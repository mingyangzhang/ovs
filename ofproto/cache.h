#include <config.h>

#include "flow.h"
#include "dp-packet.h"
#include <inttypes.h>

uint32_t cache_enqueue(struct flow *flow, const struct dp_packet *packet);
struct dp_packet* cache_pop(uint32_t queue_id);
void print_table_info();
int num_of_queue();