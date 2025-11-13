#ifndef METRICS_H
#define METRICS_H

#include "p2pipe/pipe.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CSV_HEADER "id,packets_sent,packets_received,packets_lost,packets_duplicate,packets_discarded,acks_sent,acks_received,acks_lost,retransmits,sender_paused,start,end,buffer_capacity,payload_len,type\n"
#define METRICS_FMT "%s,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%lu,%lu,%zu,%zu,%s\n"
#define METRICS_ARGS(m) \
    (m).id, \
    (m).packets_sent, \
    (m).packets_received, \
    (m).packets_lost, \
    (m).packets_duplicate, \
    (m).packets_discarded, \
    (m).acks_sent, \
    (m).acks_received, \
    (m).acks_lost, \
    (m).retransmits, \
    (m).sender_paused, \
    (unsigned long)(m).start, \
    (unsigned long)(m).end, \
    (m).buffer_capacity, \
    (m).payload_len, \
    (m).type == MODE_SND ? "SND" : "RCV"

#define METRICS_FILE "metrics.csv"

typedef struct {
    char* id;               
    size_t packets_sent;    
    size_t packets_received;
    size_t packets_lost;    
    size_t packets_duplicate;
    size_t packets_discarded;
    size_t acks_sent;
    size_t acks_received;
    size_t acks_lost;       
    size_t retransmits;
    size_t sender_paused;
    uint64_t start;         
    uint64_t end;           

    size_t buffer_capacity;
    size_t payload_len;
    PipeMode type;
} Metrics;

bool metrics_init(Metrics* metrics, const char* path); 
void metrics_reset(Metrics* metrics);
bool metrics_write(const Metrics* metrics, const char* path); 
void metrics_start(Metrics* metrics); 
void metrics_end(Metrics* metrics); 
void metrics_free(Metrics* metrics); 
void metrics_print(const Metrics* metrics); 

#ifndef METRICS_ENABLED
    #define METRICS_INCR(field)
    #define METRICS_ADD(field, num)
    #define METRICS_SET(field, value)
#else
    #define METRICS_INCR(field) metrics.field++
    #define METRICS_ADD(field, num) metrics.field += (num)
    #define METRICS_SET(field, value) metrics.field = (value)
#endif // METRICS_ENABLED


extern Metrics metrics;

#endif // METRICS_H
