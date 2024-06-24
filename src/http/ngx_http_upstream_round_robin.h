
/*
 * Copyright (C) 2023-2024 Web Server LLC
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_UPSTREAM_ROUND_ROBIN_H_INCLUDED_
#define _NGX_HTTP_UPSTREAM_ROUND_ROBIN_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define NGX_HTTP_UPSTREAM_SID_LEN   32


#if (NGX_API && NGX_HTTP_UPSTREAM_ZONE)

typedef struct {
    ngx_atomic_uint_t  keepalive;
} ngx_http_upstream_stats_t;


typedef struct {
    uint64_t                        requests;
    uint64_t                        fails;
    uint64_t                        unavailable;
    uint64_t                       *responses;
    uint64_t                        sent;
    uint64_t                        received;
    time_t                          selected;

    uint64_t                        downtime;
    uint64_t                        downstart;
} ngx_http_upstream_peer_stats_t;

#endif


typedef struct ngx_http_upstream_rr_peer_s   ngx_http_upstream_rr_peer_t;
typedef struct ngx_http_upstream_rr_peers_s  ngx_http_upstream_rr_peers_t;


#if (NGX_HTTP_UPSTREAM_ZONE)

typedef struct {
    ngx_event_t                     event;    /* must be first */
    ngx_uint_t                      worker;
    ngx_str_t                       name;
    ngx_str_t                       service;
    ngx_http_upstream_rr_peers_t   *peers;
    ngx_http_upstream_rr_peer_t    *peer;
} ngx_http_upstream_host_t;

#endif


struct ngx_http_upstream_rr_peer_s {
    struct sockaddr                *sockaddr;
    socklen_t                       socklen;
    ngx_str_t                       name;
    ngx_str_t                       server;

    ngx_int_t                       current_weight;
    ngx_int_t                       effective_weight;
    ngx_int_t                       weight;

    ngx_uint_t                      conns;
    ngx_uint_t                      max_conns;

    ngx_uint_t                      fails;
    time_t                          accessed;
    time_t                          checked;

    ngx_uint_t                      max_fails;
    time_t                          fail_timeout;
    ngx_msec_t                      slow_start;
    ngx_msec_t                      slow_time;

    ngx_uint_t                      down;

#if (NGX_HTTP_UPSTREAM_SID)
    ngx_str_t                       sid;
#endif

#if (NGX_HTTP_SSL || NGX_COMPAT)
    void                           *ssl_session;
    int                             ssl_session_len;
#endif

#if (NGX_HTTP_UPSTREAM_ZONE)
    ngx_atomic_t                    lock;
#endif

    ngx_http_upstream_rr_peer_t    *next;

#if (NGX_HTTP_UPSTREAM_ZONE)
    ngx_uint_t                      refs;
    ngx_http_upstream_host_t       *host;

#if (NGX_API)
    ngx_http_upstream_peer_stats_t  stats;
#endif

    unsigned                        zombie:1;
#endif

    NGX_COMPAT_BEGIN(32)
    NGX_COMPAT_END
};


struct ngx_http_upstream_rr_peers_s {
    ngx_uint_t                      number;

#if (NGX_HTTP_UPSTREAM_ZONE)
    ngx_slab_pool_t                *shpool;
    ngx_atomic_t                    rwlock;
    ngx_http_upstream_rr_peers_t   *zone_next;
#endif

    ngx_uint_t                      total_weight;
    ngx_uint_t                      tries;

    unsigned                        single:1;
    unsigned                        weighted:1;

    ngx_str_t                      *name;

    ngx_http_upstream_rr_peers_t   *next;

    ngx_http_upstream_rr_peer_t    *peer;

#if (NGX_HTTP_UPSTREAM_ZONE)
    ngx_uint_t                     *generation;
    ngx_http_upstream_rr_peer_t    *resolve;

#if (NGX_API)
    ngx_http_upstream_stats_t       stats;
#endif

    ngx_uint_t                      zombies;
#endif
};


#if (NGX_HTTP_UPSTREAM_ZONE)

#define ngx_http_upstream_rr_peers_rlock(peers)                               \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_rlock(&peers->rwlock);                                     \
    }

#define ngx_http_upstream_rr_peers_wlock(peers)                               \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_wlock(&peers->rwlock);                                     \
    }

#define ngx_http_upstream_rr_peers_unlock(peers)                              \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_unlock(&peers->rwlock);                                    \
    }


#define ngx_http_upstream_rr_peer_lock(peers, peer)                           \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_wlock(&peer->lock);                                        \
    }

#define ngx_http_upstream_rr_peer_unlock(peers, peer)                         \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_unlock(&peer->lock);                                       \
    }

#define ngx_http_upstream_rr_peer_ref(peers, peer)                            \
    (peer)->refs++;


static ngx_inline void
ngx_http_upstream_rr_peer_free_locked(ngx_http_upstream_rr_peers_t *peers,
    ngx_http_upstream_rr_peer_t *peer)
{
    if (peer->refs) {
        peer->zombie = 1;
        peers->zombies++;
        return;
    }

    ngx_slab_free_locked(peers->shpool, peer->sockaddr);
    ngx_slab_free_locked(peers->shpool, peer->name.data);
#if (NGX_API)
    ngx_slab_free_locked(peers->shpool, peer->stats.responses);
#endif

    if (peer->server.data) {
        ngx_slab_free_locked(peers->shpool, peer->server.data);
    }

#if (NGX_HTTP_UPSTREAM_SID)
    if (peer->sid.data
        && (peer->host == NULL
            || peer->host->peer == peer
            || peer->host->peer->sid.len == 0))
    {
        ngx_slab_free_locked(peers->shpool, peer->sid.data);
    }
#endif

#if (NGX_HTTP_SSL)
    if (peer->ssl_session) {
        ngx_slab_free_locked(peers->shpool, peer->ssl_session);
    }
#endif

    ngx_slab_free_locked(peers->shpool, peer);
}


static ngx_inline void
ngx_http_upstream_rr_peer_free(ngx_http_upstream_rr_peers_t *peers,
    ngx_http_upstream_rr_peer_t *peer)
{
    ngx_shmtx_lock(&peers->shpool->mutex);
    ngx_http_upstream_rr_peer_free_locked(peers, peer);
    ngx_shmtx_unlock(&peers->shpool->mutex);
}


static ngx_inline ngx_int_t
ngx_http_upstream_rr_peer_unref(ngx_http_upstream_rr_peers_t *peers,
    ngx_http_upstream_rr_peer_t *peer)
{
    peer->refs--;

    if (peers->shpool == NULL) {
        return NGX_OK;
    }

    if (peer->refs == 0 && peer->zombie) {
        ngx_http_upstream_rr_peer_free(peers, peer);
        peers->zombies--;

        return NGX_DONE;
    }

    return NGX_OK;
}

#else

#define ngx_http_upstream_rr_peers_rlock(peers)
#define ngx_http_upstream_rr_peers_wlock(peers)
#define ngx_http_upstream_rr_peers_unlock(peers)
#define ngx_http_upstream_rr_peer_lock(peers, peer)
#define ngx_http_upstream_rr_peer_unlock(peers, peer)
#define ngx_http_upstream_rr_peer_ref(peers, peer)
#define ngx_http_upstream_rr_peer_unref(peers, peer)  NGX_OK

#endif


typedef struct {
    ngx_uint_t                      generation;
    ngx_http_upstream_rr_peers_t   *peers;
    ngx_http_upstream_rr_peer_t    *current;
    uintptr_t                      *tried;
    uintptr_t                       data;
} ngx_http_upstream_rr_peer_data_t;


ngx_int_t ngx_http_upstream_init_round_robin(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us);
void ngx_http_upstream_set_round_robin_single(ngx_http_upstream_srv_conf_t *us);
ngx_int_t ngx_http_upstream_init_round_robin_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);
ngx_int_t ngx_http_upstream_create_round_robin_peer(ngx_http_request_t *r,
    ngx_http_upstream_resolved_t *ur);
ngx_int_t ngx_http_upstream_get_round_robin_peer(ngx_peer_connection_t *pc,
    void *data);
void ngx_http_upstream_free_round_robin_peer(ngx_peer_connection_t *pc,
    void *data, ngx_uint_t state);

#if (NGX_HTTP_SSL)
ngx_int_t
    ngx_http_upstream_set_round_robin_peer_session(ngx_peer_connection_t *pc,
    void *data);
void ngx_http_upstream_save_round_robin_peer_session(ngx_peer_connection_t *pc,
    void *data);
#endif


#if (NGX_HTTP_UPSTREAM_SID)
void ngx_http_upstream_rr_peer_init_sid(ngx_http_upstream_rr_peer_t *peer);
#endif


static ngx_inline ngx_uint_t
ngx_http_upstream_throttle_peer(ngx_http_upstream_rr_peer_t *peer)
{
    ngx_uint_t  factor;

    if (peer->slow_time) {
        factor = (ngx_current_msec - peer->slow_time) * 100 / peer->slow_start;

        if (factor < 100) {
            return factor;
        }

        peer->slow_time = 0;
    }

    return 100;
}


#endif /* _NGX_HTTP_UPSTREAM_ROUND_ROBIN_H_INCLUDED_ */
