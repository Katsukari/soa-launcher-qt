#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void soa_log(int level, const char* message);

void soa_ping(void);

typedef struct courier courier;

typedef enum
{
    courier_phase_preparing   = 0,
    courier_phase_checking    = 1,
    courier_phase_downloading = 2,
    courier_phase_verifying   = 3
} courier_phase;

typedef void (*courier_progress_cb)(courier_phase phase,
                                const char* message,
                                int         percent,
                                uint64_t    received,
                                uint64_t    total,
                                uint64_t    throughput,
                                void*       ctx);

typedef void (*courier_done_cb)(bool        ok,
                            const char* message,
                            void*       ctx);

courier* courier_create(const char*     cdn_base_url,
                                      courier_progress_cb on_progress,
                                      courier_done_cb     on_done,
                                      void*           ctx);
void courier_destroy(courier* d);

void courier_integrity_check(courier* d, const char* install_path);
void courier_update_check  (courier* d, const char* install_path);
void courier_update        (courier* d, const char* install_path);

void courier_cancel(courier* d);

#ifdef __cplusplus
}
#endif