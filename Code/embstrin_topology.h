#ifndef EMBSTRIN_TOPOLOGY_H
#define EMBSTRIN_TOPOLOGY_H

#include <stdint.h>
#include <stdbool.h>
#include "embstrin_types.h"

// ---- Link type enum ----
typedef enum {
    EMBSTRIN_LINK_USB  = 0,
    EMBSTRIN_LINK_GPIO = 1,
    EMBSTRIN_LINK_UART = 2
} EMBSTRIN_LinkType;

// ---- Lightweight graph node ---- 
typedef struct {
    uint32_t device_id;
    uint32_t device_type;
} EMBSTRIN_API_Device;

// ---- Edge between two devices ----
typedef struct {
    EMBSTRIN_API_Device* a;              // one end
    EMBSTRIN_API_Device* b;              // other end
    EMBSTRIN_LinkType    link_type;      // USB, GPIO, UART
    uint32_t             baud_rate;      // bits per second — fixed at creation
    uint32_t             max_capacity;   // max simultaneous packets — fixed at creation
    uint32_t             tx_packet_count;// packets actively transmitting → feeds tx_stress
    uint32_t             rx_packet_count;// packets actively receiving → feeds rx_stress
    uint32_t             queue_depth;    // packets waiting → feeds latency
    uint32_t             error_rate;     // errors per 1000 packets — updated periodically
    bool                 active;         // is this link currently up?
} EMBSTRIN_API_Link;

// ---- Adjacency list entry ----
typedef struct {
    EMBSTRIN_API_Device*  device;
    EMBSTRIN_API_Link**   links;
    uint32_t              link_count;
    uint32_t              link_capacity;
} EMBSTRIN_AdjEntry; 

    
// ---- Full graph ----
typedef struct {
    EMBSTRIN_AdjEntry*  entries;
    uint32_t            entry_count;
    uint32_t            entry_capacity;
} EMBSTRIN_Graph;

// ---- Graph management ----
EMBSTRIN_Graph*      EMBSTRIN_TOPO_create_graph(uint32_t capacity);
void                 EMBSTRIN_TOPO_destroy_graph(EMBSTRIN_Graph* graph);

int32_t              EMBSTRIN_TOPO_add_device(EMBSTRIN_Graph* graph,
                                               EMBSTRIN_API_Device* device);

int32_t              EMBSTRIN_TOPO_remove_device(EMBSTRIN_Graph* graph,
                                                  uint32_t device_id);

// ---- Link management ----
EMBSTRIN_API_Link*   EMBSTRIN_TOPO_add_link(EMBSTRIN_Graph* graph,
                                              EMBSTRIN_API_Device* a,
                                              EMBSTRIN_API_Device* b,
                                              EMBSTRIN_LinkType link_type,
                                              uint32_t baud_rate,
                                              uint32_t max_capacity);

int32_t              EMBSTRIN_TOPO_remove_link(EMBSTRIN_Graph* graph,
                                                EMBSTRIN_API_Device* a,
                                                EMBSTRIN_API_Device* b);

int32_t              EMBSTRIN_TOPO_set_link_active(EMBSTRIN_API_Link* link,
                                                    bool active);

// ---- Counter updates (called by Communication team) ----
int32_t              EMBSTRIN_TOPO_update_link_counters(EMBSTRIN_API_Link* link,
                                                         int32_t tx_delta,
                                                         int32_t rx_delta,
                                                         int32_t queue_delta);

int32_t              EMBSTRIN_TOPO_update_error_rate(EMBSTRIN_API_Link* link,
                                                      uint32_t error_rate);

// ---- Computed metrics (not stored, calculated on demand) ----
float                EMBSTRIN_TOPO_compute_tx_stress(EMBSTRIN_API_Link* link);
float                EMBSTRIN_TOPO_compute_rx_stress(EMBSTRIN_API_Link* link);
float                EMBSTRIN_TOPO_compute_latency_ms(EMBSTRIN_API_Link* link,
                                                       uint32_t packet_size_bits);

// ---- Path finding (implementation decided later) ----
EMBSTRIN_API_Device** EMBSTRIN_TOPO_find_path(EMBSTRIN_Graph* graph,
                                                EMBSTRIN_API_Device* src,
                                                EMBSTRIN_API_Device* dst,
                                                uint32_t* path_length);

#endif  