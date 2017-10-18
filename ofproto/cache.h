#include "flow.h"
#include "dp-packet.h"

void cache_enqueue(const struct flow *flow, const struct dp_packet *packet);