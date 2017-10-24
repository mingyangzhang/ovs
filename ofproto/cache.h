#include <config.h>

#include "flow.h"
#include "dp-packet.h"

int cache_enqueue(const struct flow *flow, const struct dp_packet *packet);