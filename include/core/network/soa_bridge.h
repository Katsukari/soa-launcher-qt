#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Logging bridge (Swift -> C++)
// Swift to log through the same spdlog sinks as everything else
void soa_log(int level, const char* message);

void soa_ping(void);

typedef struct soa_downloader soa_downloader;

typedef void (*soa_progress_cb)(const char* message,
                                int         percent,
                                uint64_t    received,
                                uint64_t    total,
                                uint64_t    throughput,
                                void*       ctx);

typedef void (*soa_done_cb)(bool        ok,
                            const char* message,
                            void*       ctx);

soa_downloader* soa_downloader_create(const char*     cdn_base_url,
                                      soa_progress_cb on_progress,
                                      soa_done_cb     on_done,
                                      void*           ctx);
void soa_downloader_destroy(soa_downloader* d);


void soa_integrity_check(soa_downloader* d, const char* install_path);
void soa_update_check  (soa_downloader* d, const char* install_path);
void soa_update        (soa_downloader* d, const char* install_path);

// Request cancellation of the in-flight operation.
void soa_cancel(soa_downloader* d);

#ifdef __cplusplus
}
#endif