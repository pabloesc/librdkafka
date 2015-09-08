/*
 * librdkafka - Apache Kafka C library
 *
 * Copyright (c) 2012-2013, Magnus Edenhill
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once


#ifndef _MSC_VER
#define _GNU_SOURCE  /* for strndup() */
#include <syslog.h>
#else
typedef int mode_t;
#endif
#include <fcntl.h>


#include "rdsysqueue.h"

#include "rdkafka.h"
#include "rd.h"
#include "rdlog.h"
#include "rdtime.h"
#include "rdaddr.h"

#include "rdkafka_transport.h"
#include "rdkafka_timer.h"


#define RD_POLL_INFINITE  -1
#define RD_POLL_NOWAIT     0

#if WITH_SSL
#include <openssl/ssl.h>
#endif


/*
 * Portability
 */

/* MacOSX does not have strndupa() */
#ifndef strndupa
#define strndupa(PTR,LEN) ({ int _L = (LEN); char *_x = rd_alloca(_L+1); \
      memcpy(_x, (PTR), _L); *(_x+_L) = 0; _x;})
#endif

#ifndef strdupa
#define strdupa(PTR) ({ const char *_P = (PTR); int _L = strlen(_P); \
      char *_x = rd_alloca(_L+1); memcpy(_x, _P, _L); *(_x+_L) = 0; _x;})
#endif

/* Some versions of MacOSX dont have IOV_MAX */
#ifndef IOV_MAX
#define IOV_MAX 1024
#endif


#define rd_kafka_assert(rk, cond) do {                                  \
                if (unlikely(!(cond)))                                  \
                        rd_kafka_crash(__FILE__,__LINE__, __FUNCTION__, \
                                       (rk), "assert: " # cond);        \
        } while (0)

void
RD_NORETURN
rd_kafka_crash (const char *file, int line, const char *function,
                rd_kafka_t *rk, const char *reason);



/**
 * Flags used by:
 *   - rd_kafka_op_t.rko_flags
 *   - rd_kafka_buf_t.rkbuf_flags
 */
#define RD_KAFKA_OP_F_FREE        0x1  /* rd_free payload when done with it */
#define RD_KAFKA_OP_F_FLASH       0x2  /* Internal: insert at head of queue */
#define RD_KAFKA_OP_F_NO_RESPONSE 0x4  /* rkbuf: Not expecting a response */
#define RD_KAFKA_OP_F_CRC         0x8  /* rkbuf: Perform CRC calculation */

#include "rdkafka_msg.h"
#include "rdkafka_proto.h"
#include "rdkafka_buf.h"


/**
 * Protocol level sanity
 */
#define RD_KAFKAP_BROKERS_MAX     1000
#define RD_KAFKAP_TOPICS_MAX      10000
#define RD_KAFKAP_PARTITIONS_MAX  10000


#define RD_KAFKA_OFFSET_ERROR    -1001
#define RD_KAFKA_OFFSET_IS_LOGICAL(OFF)  ((OFF) < 0)

/* Forward declarations */
struct rd_kafka_s;
struct rd_kafka_toppar_s;
struct rd_kafka_topic_s;
struct rd_kafka_msg_s;
struct rd_kafka_broker_s;
struct rd_kafka_conf_s;
struct rd_kafka_topic_conf_s;



/**
 * MessageSet compression codecs
 */
typedef enum {
	RD_KAFKA_COMPRESSION_NONE,
	RD_KAFKA_COMPRESSION_GZIP,
	RD_KAFKA_COMPRESSION_SNAPPY,
} rd_kafka_compression_t;


typedef enum {
	RD_KAFKA_PROTO_PLAINTEXT,
#if WITH_SSL
	RD_KAFKA_PROTO_SSL,
#endif
	RD_KAFKA_PROTO_NUM,
} rd_kafka_secproto_t;

/**
 * Optional configuration struct passed to rd_kafka_new*().
 *
 * The struct is populated ted through string properties
 * by calling rd_kafka_conf_set().
 *
 */
struct rd_kafka_conf_s {
	/*
	 * Generic configuration
	 */
	int     max_msg_size;
        int     recv_max_msg_size;
	int     metadata_request_timeout_ms;
	int     metadata_refresh_interval_ms;
	int     metadata_refresh_fast_cnt;
	int     metadata_refresh_fast_interval_ms;
        int     metadata_refresh_sparse;
	int     debug;
	int     broker_addr_ttl;
        int     broker_addr_family;
	int     socket_timeout_ms;
	int     socket_blocking_max_ms;
	int     socket_sndbuf_size;
	int     socket_rcvbuf_size;
        int     socket_keepalive;
        int     socket_max_fails;
	char   *client_id_str;
        rd_kafkap_str_t *client_id;
	char   *brokerlist;
	int     stats_interval_ms;
	int     term_sig;
	rd_kafka_secproto_t security_protocol;
	struct {
#if WITH_SSL
		SSL_CTX *ctx;
#endif
		char *cipher_suites;
		char *enabled_protocols;
		char *key_location;
		char *key_password;
		char *cert_location;
		char *ca_location;
	} ssl;
	
	/*
	 * Consumer configuration
	 */
	int    queued_min_msgs;
        int    queued_max_msg_kbytes;
        int64_t queued_max_msg_bytes;
	int    fetch_wait_max_ms;
        int    fetch_msg_max_bytes;
	int    fetch_min_bytes;
	int    fetch_error_backoff_ms;
	/* Pre-built Fetch request header. */
	struct rd_kafkap_FetchRequest FetchRequest;
        char  *group_id_str;
        rd_kafkap_str_t   *group_id;    /* Consumer group id */




	/*
	 * Producer configuration
	 */
	int    queue_buffering_max_msgs;
	int    buffering_max_ms;
	int    max_retries;
	int    retry_backoff_ms;
	int    batch_num_messages;
	rd_kafka_compression_t compression_codec;
	int    dr_err_only;

	/* Message delivery report callback.
	 * Called once for each produced message, either on
	 * successful and acknowledged delivery to the broker in which
	 * case 'err' is 0, or if the message could not be delivered
	 * in which case 'err' is non-zero (use rd_kafka_err2str()
	 * to obtain a human-readable error reason).
	 *
	 * If the message was produced with neither RD_KAFKA_MSG_F_FREE
	 * or RD_KAFKA_MSG_F_COPY set then 'payload' is the original
	 * pointer provided to rd_kafka_produce().
	 * rdkafka will not perform any further actions on 'payload'
	 * at this point and the application may rd_free the payload data
	 * at this point.
	 *
	 * 'opaque' is 'conf.opaque', while 'msg_opaque' is
	 * the opaque pointer provided in the rd_kafka_produce() call.
	 */
	void (*dr_cb) (rd_kafka_t *rk,
		       void *payload, size_t len,
		       rd_kafka_resp_err_t err,
		       void *opaque, void *msg_opaque);

        void (*dr_msg_cb) (rd_kafka_t *rk, const rd_kafka_message_t *rkmessage,
                           void *opaque);

	/* Error callback */
	void (*error_cb) (rd_kafka_t *rk, int err,
			  const char *reason, void *opaque);

	/* Stats callback */
	int (*stats_cb) (rd_kafka_t *rk,
			 char *json,
			 size_t json_len,
			 void *opaque);

        /* Log callback */
        void (*log_cb) (const rd_kafka_t *rk, int level,
                        const char *fac, const char *buf);
        int    log_level;

        /* Socket creation callback */
        int (*socket_cb) (int domain, int type, int protocol, void *opaque);

		/* File open callback */
        int (*open_cb) (const char *pathname, int flags, mode_t mode,
                        void *opaque);

	/* Opaque passed to callbacks. */
	void  *opaque;
};

int rd_kafka_socket_cb_linux (int domain, int type, int protocol, void *opaque);
int rd_kafka_socket_cb_generic (int domain, int type, int protocol,
                                void *opaque);
#ifndef _MSC_VER
int rd_kafka_open_cb_linux (const char *pathname, int flags, mode_t mode,
                            void *opaque);
#endif
int rd_kafka_open_cb_generic (const char *pathname, int flags, mode_t mode,
                              void *opaque);



struct rd_kafka_topic_conf_s {
	int     required_acks;
        int     enforce_isr_cnt;
	int32_t request_timeout_ms;
	int     message_timeout_ms;

	int32_t (*partitioner) (const rd_kafka_topic_t *rkt,
				const void *keydata, size_t keylen,
				int32_t partition_cnt,
				void *rkt_opaque,
				void *msg_opaque);

        int     produce_offset_report;

        char   *group_id_str;
        rd_kafkap_str_t *group_id;    /* Consumer group id in protocol format */

        int     consume_callback_max_msgs;
	int     auto_commit;
	int     auto_commit_interval_ms;
	int     auto_offset_reset;
	char   *offset_store_path;
	int     offset_store_sync_interval_ms;
        enum {
                RD_KAFKA_OFFSET_METHOD_FILE,
                RD_KAFKA_OFFSET_METHOD_BROKER,
        } offset_store_method;

	/* Application provided opaque pointer (this is rkt_opaque) */
	void   *opaque;
};



typedef struct rd_kafka_avg_s {
        struct {
                int64_t maxv;
                int64_t minv;
                int64_t avg;
                int64_t sum;
                int     cnt;
                rd_ts_t start;
        } ra_v;
        mtx_t ra_lock;
        enum {
                RD_KAFKA_AVG_GAUGE,
                RD_KAFKA_AVG_COUNTER,
        } ra_type;
} rd_kafka_avg_t;


/**
 * Add timestamp 'ts' to averager 'ra'.
 */
static RD_UNUSED void rd_kafka_avg_add (rd_kafka_avg_t *ra, int64_t v) {
        mtx_lock(&ra->ra_lock);
	if (v > ra->ra_v.maxv)
		ra->ra_v.maxv = v;
	if (ra->ra_v.minv == 0 || v < ra->ra_v.minv)
		ra->ra_v.minv = v;
	ra->ra_v.sum += v;
	ra->ra_v.cnt++;
        mtx_unlock(&ra->ra_lock);
}

/**
 * Rolls over statistics in 'src' and stores the average in 'dst'.
 * 'src' is cleared and ready to be reused.
 */
static RD_UNUSED void rd_kafka_avg_rollover (rd_kafka_avg_t *dst,
					     rd_kafka_avg_t *src) {
        rd_ts_t now = rd_clock();

        mtx_lock(&src->ra_lock);
        dst->ra_type = src->ra_type;
	dst->ra_v    = src->ra_v;
	memset(&src->ra_v, 0, sizeof(src->ra_v));
        src->ra_v.start = now;
        mtx_unlock(&src->ra_lock);

        if (dst->ra_type == RD_KAFKA_AVG_GAUGE) {
                if (dst->ra_v.cnt)
                        dst->ra_v.avg = dst->ra_v.sum / dst->ra_v.cnt;
                else
                        dst->ra_v.avg = 0;
        } else {
                rd_ts_t elapsed = now - dst->ra_v.start;

                if (elapsed)
                        dst->ra_v.avg = (dst->ra_v.sum * 1000000llu) / elapsed;
                else
                        dst->ra_v.avg = 0;

                dst->ra_v.start = elapsed;
        }
}


/**
 * Initialize an averager
 */
static RD_UNUSED void rd_kafka_avg_init (rd_kafka_avg_t *ra, int type) {
        rd_kafka_avg_t dummy;
        memset(ra, 0, sizeof(*ra));
        mtx_init(&ra->ra_lock, 0);
        ra->ra_type = type;

        rd_kafka_avg_rollover(&dummy, ra);
}

/**
 * Destroy averager
 */
static RD_UNUSED void rd_kafka_avg_destroy (rd_kafka_avg_t *ra) {
        mtx_destroy(&ra->ra_lock);
}







typedef struct rd_kafka_q_s {
	mtx_t rkq_lock;
	cnd_t  rkq_cond;
	struct rd_kafka_q_s *rkq_fwdq; /* Forwarded/Routed queue.
					* Used in place of this queue
					* for all operations. */
	TAILQ_HEAD(, rd_kafka_op_s) rkq_q;
	rd_atomic32_t rkq_qlen;      /* Number of entries in queue */
	rd_atomic64_t rkq_qsize;     /* Size of all entries in queue */
	rd_atomic32_t rkq_refcnt;
        int             rkq_flags;
#define RD_KAFKA_Q_F_ALLOCATED  0x1  /* Allocated: rd_free on destroy */
} rd_kafka_q_t;


typedef enum {
	RD_KAFKA_OP_FETCH,    /* Kafka thread -> Application */
	RD_KAFKA_OP_ERR,      /* Kafka thread -> Application */
	RD_KAFKA_OP_DR,       /* Kafka thread -> Application
			       * Produce message delivery report */
	RD_KAFKA_OP_STATS,    /* Kafka thread -> Application */

	RD_KAFKA_OP_METADATA_REQ,  /* any -> Broker thread: request metadata */
        RD_KAFKA_OP_OFFSET_COMMIT, /* any -> toppar's Broker thread */
        RD_KAFKA_OP_NODE_UPDATE,   /* any -> Broker thread: node update */

        RD_KAFKA_OP_REPLY,    /* generic replyq op */
        RD_KAFKA_OP_XMIT_BUF, /* transmit buffer: any -> broker thread */
        RD_KAFKA_OP_RECV_BUF, /* received response buffer: broker thr -> any */
        RD_KAFKA_OP_FETCH_START, /* Application -> toppar's Broker thread */
        RD_KAFKA_OP_FETCH_STOP,  /* Application -> toppar's Broker thread */
        RD_KAFKA_OP__END
} rd_kafka_op_type_t;

typedef struct rd_kafka_op_s {
	TAILQ_ENTRY(rd_kafka_op_s) rko_link;

	rd_kafka_op_type_t rko_type;
	int       rko_flags;      /* See RD_KAFKA_OP_F_... above */

        /* Generic fields */
	rd_kafka_msgq_t rko_msgq;
        rd_kafka_q_t   *rko_replyq;    /* Indicates request: enq reply
                                        * on this queue. */
        int             rko_intarg;    /* Generic integer argument */
        void           *rko_opaque;

	/* For PRODUCE */
	rd_kafka_msg_t *rko_rkm;

	/* For ERR */
#define rko_err     rko_rkmessage.err
#define rko_payload rko_rkmessage.payload
#define rko_len     rko_rkmessage.len

	/* For FETCH & REPLY */
	rd_kafka_message_t rko_rkmessage;
	rd_kafka_buf_t    *rko_rkbuf;

	/* For METADATA */
#define rko_rkt         rko_rkmessage.rkt
#define rko_all_topics  rko_intarg
#define rko_reason      rko_rkmessage.payload
        struct rd_kafka_metadata *rko_metadata;

	/* For STATS */
#define rko_json      rko_rkmessage.payload
#define rko_json_len  rko_rkmessage.len

        /* For OFFSET_COMMIT, FETCH_START */
        struct rd_kafka_toppar_s *rko_rktp;
#define rko_offset    rko_rkmessage.offset

        /* For FETCH_START */
#define rko_version   rko_intarg

        /* For BROKER_UPDATE */
#define rko_nodename  rko_rkmessage.payload
#define rko_nodeid    rko_rkmessage.partition
} rd_kafka_op_t;

TAILQ_HEAD(rd_kafka_op_head_s, rd_kafka_op_s);







typedef enum {
	RD_KAFKA_CONFIGURED,
	RD_KAFKA_LEARNED,
	RD_KAFKA_INTERNAL,
} rd_kafka_confsource_t;


#define RD_KAFKA_NODENAME_SIZE  128

typedef struct rd_kafka_broker_s {
	TAILQ_ENTRY(rd_kafka_broker_s) rkb_link;

	int32_t             rkb_nodeid;
#define RD_KAFKA_NODEID_UA -1

	rd_sockaddr_list_t *rkb_rsal;
	time_t              rkb_t_rsal_last;
	rd_kafka_transport_t *rkb_transport;

	uint32_t            rkb_corrid;

	rd_kafka_q_t        rkb_ops;

	TAILQ_HEAD(, rd_kafka_toppar_s) rkb_toppars;
	rwlock_t    rkb_toppar_lock;
	int                 rkb_toppar_cnt;

	rd_ts_t             rkb_ts_fetch_backoff;
	int                 rkb_fetching;

#define rd_kafka_broker_toppars_rdlock(rkb) \
	rwlock_rdlock(&(rkb)->rkb_toppar_lock)
#define rd_kafka_broker_toppars_wrlock(rkb) \
	rwlock_wrlock(&(rkb)->rkb_toppar_lock)
#define rd_kafka_broker_toppars_rdunlock(rkb) \
	rwlock_rdunlock(&(rkb)->rkb_toppar_lock)
#define rd_kafka_broker_toppars_wrunlock(rkb) \
	rwlock_wrunlock(&(rkb)->rkb_toppar_lock)

	enum {
		RD_KAFKA_BROKER_STATE_INIT,
		RD_KAFKA_BROKER_STATE_DOWN,
		RD_KAFKA_BROKER_STATE_CONNECT,
		RD_KAFKA_BROKER_STATE_UP,
                RD_KAFKA_BROKER_STATE_UPDATE,
	} rkb_state;

        rd_ts_t             rkb_ts_state;        /* Timestamp of last
                                                  * state change */

	rd_kafka_confsource_t  rkb_source;
	struct {
		rd_atomic64_t tx_bytes;
		rd_atomic64_t tx;    /* Kafka-messages (not payload msgs) */
		rd_atomic64_t tx_err;
		rd_atomic64_t tx_retries;
		rd_atomic64_t req_timeouts;  /* Accumulated value */

		rd_atomic64_t rx_bytes;
		rd_atomic64_t rx;    /* Kafka messages (not payload msgs) */
		rd_atomic64_t rx_err;
		rd_atomic64_t rx_corrid_err; /* CorrId misses */
		rd_atomic64_t rx_partial;    /* Partial messages received
					 * and dropped. */
	} rkb_c;

        int                 rkb_req_timeouts;  /* Current value */

	rd_ts_t             rkb_ts_metadata_poll; /* Next metadata poll time */
	int                 rkb_metadata_fast_poll_cnt; /* Perform fast
							 * metadata polls. */
	mtx_t     rkb_lock;
	thrd_t           rkb_thread;

	rd_atomic32_t         rkb_refcnt;

	struct rd_kafka_s  *rkb_rk;

	struct {
		char msg[512];
		int  err;  /* errno */
	} rkb_err;

	rd_kafka_buf_t     *rkb_recv_buf;

	rd_kafka_bufq_t     rkb_outbufs;
	rd_atomic32_t       rkb_outbuf_msgcnt;
	rd_kafka_bufq_t     rkb_waitresps;
	rd_kafka_bufq_t     rkb_retrybufs;

	rd_kafka_avg_t      rkb_avg_rtt;        /* Current averaging period */

	char                rkb_name[RD_KAFKA_NODENAME_SIZE];  /* Displ name */
	char                rkb_nodename[RD_KAFKA_NODENAME_SIZE]; /* host:port*/
	rd_kafka_secproto_t rkb_proto;


} rd_kafka_broker_t;

#define rd_kafka_broker_keep(rkb) (void)rd_atomic32_add(&(rkb)->rkb_refcnt, 1)
#define rd_kafka_broker_lock(rkb)   mtx_lock(&(rkb)->rkb_lock)
#define rd_kafka_broker_unlock(rkb) mtx_unlock(&(rkb)->rkb_lock)

/* rd_kafka_topic_t */
struct rd_kafka_topic_s {
	TAILQ_ENTRY(rd_kafka_topic_s) rkt_link;

	rd_atomic32_t         rkt_refcnt;

	rwlock_t   rkt_lock;
	rd_kafkap_str_t   *rkt_topic;
	struct rd_kafka_toppar_s  *rkt_ua;  /* unassigned partition */
	struct rd_kafka_toppar_s **rkt_p;
	int32_t            rkt_partition_cnt;
	TAILQ_HEAD(, rd_kafka_toppar_s) rkt_desp; /* Desired partitions
						   * that are not yet seen
						   * in the cluster. */

	rd_ts_t            rkt_ts_metadata; /* Timestamp of last metadata
					     * update for this topic. */

	enum {
		RD_KAFKA_TOPIC_S_UNKNOWN,   /* No cluster information yet */
		RD_KAFKA_TOPIC_S_EXISTS,    /* Topic exists in cluster */
		RD_KAFKA_TOPIC_S_NOTEXISTS, /* Topic is not known in cluster */
	} rkt_state;

        int                rkt_flags;
#define RD_KAFKA_TOPIC_F_LEADER_QUERY  0x1 /* There is an outstanding
                                            * leader query for this topic */
	struct rd_kafka_s *rkt_rk;

	rd_kafka_topic_conf_t rkt_conf;
};

#define rd_kafka_topic_rdlock(rkt)     rwlock_rdlock(&(rkt)->rkt_lock)
#define rd_kafka_topic_wrlock(rkt)     rwlock_wrlock(&(rkt)->rkt_lock)
#define rd_kafka_topic_rdunlock(rkt)   rwlock_rdunlock(&(rkt)->rkt_lock)
#define rd_kafka_topic_wrunlock(rkt)   rwlock_wrunlock(&(rkt)->rkt_lock)

/**
 * Topic + Partition combination
 */
typedef struct rd_kafka_toppar_s {
	TAILQ_ENTRY(rd_kafka_toppar_s) rktp_rklink;  /* rd_kafka_t link */
	TAILQ_ENTRY(rd_kafka_toppar_s) rktp_rkblink; /* rd_kafka_broker_t link*/
	TAILQ_ENTRY(rd_kafka_toppar_s) rktp_rktlink; /* rd_kafka_topic_t link */
	rd_kafka_topic_t  *rktp_rkt;
	int32_t            rktp_partition;
	rd_kafka_broker_t *rktp_leader;  /* Leader broker */
	rd_atomic32_t      rktp_refcnt;
	mtx_t    rktp_lock;

	rd_kafka_msgq_t    rktp_msgq;      /* application->rdkafka queue.
					    * protected by rktp_lock */
	rd_kafka_msgq_t    rktp_xmit_msgq; /* internal broker xmit queue */

	rd_ts_t            rktp_ts_last_xmit;

        struct rd_kafka_metadata_partition rktp_metadata; /* Last known metadata
                                                           * for this toppar. */
	/* Consumer */
	rd_kafka_q_t       rktp_fetchq;          /* Queue of fetched messages
						  * from broker.
                                                  * Broker thread -> App */
        rd_kafka_q_t       rktp_ops;             /* App -> Broker thread */

        rd_atomic32_t      rktp_version;         /* Latest op version.
                                                  * Authoritative (app thread)*/
        int32_t            rktp_op_version;      /* Latest op version.
                                                  * Slave (broker thread)*/
        int32_t            rktp_fetch_version;   /* Op version of curr fetch */

	enum {
		RD_KAFKA_TOPPAR_FETCH_NONE = 0,
                RD_KAFKA_TOPPAR_FETCH_COORD_QUERY,
                RD_KAFKA_TOPPAR_FETCH_COORD_WAIT,
		RD_KAFKA_TOPPAR_FETCH_OFFSET_QUERY,
		RD_KAFKA_TOPPAR_FETCH_OFFSET_WAIT,
		RD_KAFKA_TOPPAR_FETCH_ACTIVE,
	} rktp_fetch_state;           /* Broker thread's state */


	rd_ts_t            rktp_ts_offset_req_next;
	int64_t            rktp_query_offset;    /* Offset to query broker for*/
	int64_t            rktp_next_offset;     /* Next offset to fetch */
	int64_t            rktp_app_offset;      /* Last offset delivered to
						  * application */
	int64_t            rktp_stored_offset;   /* Last stored offset, but
						  * maybe not commited yet. */
        int64_t            rktp_committing_offset; /* Offset currently being
                                                    * commited */
	int64_t            rktp_commited_offset; /* Last commited offset */
	rd_ts_t            rktp_ts_commited_offset; /* Timestamp of last
						     * commit */
	int64_t            rktp_eof_offset;      /* The last offset we reported
						  * EOF for. */
        int64_t            rktp_lo_offset;       /* Current broker low offset */
        int64_t            rktp_hi_offset;       /* Current broker hi offset */
        rd_ts_t            rktp_ts_offset_lag;

	char              *rktp_offset_path;     /* Path to offset file */
	FILE              *rktp_offset_fp;       /* Offset file pointer */
	rd_kafka_timer_t   rktp_offset_commit_tmr; /* Offste commit timer */
	rd_kafka_timer_t   rktp_offset_sync_tmr; /* Offset file sync timer */

	int                rktp_flags;
#define RD_KAFKA_TOPPAR_F_DESIRED  0x1      /* This partition is desired
					     * by a consumer. */
#define RD_KAFKA_TOPPAR_F_UNKNOWN  0x2      /* Topic is (not yet) seen on
					     * a broker. */
#define RD_KAFKA_TOPPAR_F_OFFSET_STORE 0x4  /* Offset store is active */
	struct {
		rd_atomic64_t tx_msgs;
		rd_atomic64_t tx_bytes;
                rd_atomic64_t msgs;
                rd_atomic64_t rx_ver_drops;
	} rktp_c;

} rd_kafka_toppar_t;

#define rd_kafka_toppar_keep(rktp) (void)rd_atomic32_add(&(rktp)->rktp_refcnt, 1)
#define rd_kafka_toppar_destroy(rktp) do {				\
	if (rd_atomic32_sub(&(rktp)->rktp_refcnt, 1) == 0)		\
		rd_kafka_toppar_destroy0(rktp);				\
	} while (0)
#define rd_kafka_toppar_lock(rktp)     mtx_lock(&(rktp)->rktp_lock)
#define rd_kafka_toppar_unlock(rktp)   mtx_unlock(&(rktp)->rktp_lock)

static const char *rd_kafka_toppar_name (const rd_kafka_toppar_t *rktp)
	RD_UNUSED;
static const char *rd_kafka_toppar_name (const rd_kafka_toppar_t *rktp) {
	static RD_TLS char ret[256];

	rd_snprintf(ret, sizeof(ret), "%.*s [%"PRId32"]",
		    RD_KAFKAP_STR_PR(rktp->rktp_rkt->rkt_topic),
		    rktp->rktp_partition);

	return ret;
}


/**
 * Kafka handle. (rd_kafka_t)
 */
struct rd_kafka_s {
	rd_kafka_q_t rk_rep;   /* kafka -> application reply queue */

	TAILQ_HEAD(, rd_kafka_broker_s) rk_brokers;
	rd_atomic32_t              rk_broker_cnt;
	rd_atomic32_t              rk_broker_down_cnt;
	rd_kafka_broker_t         *rk_internal_rkb;

	TAILQ_HEAD(, rd_kafka_topic_s)  rk_topics;
	int              rk_topic_cnt;

	char             rk_name[128];
	rd_kafkap_str_t *rk_clientid;
	rd_kafka_conf_t  rk_conf;
	int              rk_flags;
	rd_atomic32_t    rk_terminate;
	rwlock_t rk_lock;
	rd_atomic32_t    rk_refcnt;
	rd_kafka_type_t  rk_type;
	struct timeval   rk_tv_state_change;
	union {
		struct {
			char    *topic;
			int32_t  partition;
			uint64_t offset;
			uint64_t app_offset;
		} consumer;
		struct {
			rd_atomic32_t msg_cnt;  /* current message count */
		} producer;
	} rk_u;
#define rk_consumer rk_u.consumer
#define rk_producer rk_u.producer

	TAILQ_HEAD(, rd_kafka_timer_s) rk_timers;
	mtx_t                rk_timers_lock;
	cnd_t                rk_timers_cond;

	thrd_t rk_thread;
};

#define rd_kafka_wrlock(rk)    rwlock_wrlock(&(rk)->rk_lock)
#define rd_kafka_rdlock(rk)    rwlock_rdlock(&(rk)->rk_lock)
#define rd_kafka_rdunlock(rk)    rwlock_rdunlock(&(rk)->rk_lock)
#define rd_kafka_wrunlock(rk)    rwlock_wrunlock(&(rk)->rk_lock)















/**
 * Debug contexts
 */
#define RD_KAFKA_DBG_GENERIC    0x1
#define RD_KAFKA_DBG_BROKER     0x2
#define RD_KAFKA_DBG_TOPIC      0x4
#define RD_KAFKA_DBG_METADATA   0x8
#define RD_KAFKA_DBG_PRODUCER   0x10
#define RD_KAFKA_DBG_QUEUE      0x20
#define RD_KAFKA_DBG_MSG        0x40
#define RD_KAFKA_DBG_PROTOCOL   0x80
#define RD_KAFKA_DBG_ALL        0xff


void rd_kafka_log_buf (const rd_kafka_t *rk, int level,
		       const char *fac, const char *buf);
void rd_kafka_log0(const rd_kafka_t *rk, const char *extra, int level,
	const char *fac, const char *fmt, ...)	RD_FORMAT(printf, 5, 6);

#define rd_kafka_log(rk,level,fac,...) rd_kafka_log0(rk,NULL,level,fac,__VA_ARGS__)
#define rd_kafka_dbg(rk,ctx,fac,...) do {				  \
		if (unlikely((rk)->rk_conf.debug & RD_KAFKA_DBG_ ## ctx)) \
			rd_kafka_log0(rk,NULL,LOG_DEBUG,fac,__VA_ARGS__); \
	} while (0)

#define rd_rkb_log(rkb,level,fac,...)					\
	rd_kafka_log0((rkb)->rkb_rk, (rkb)->rkb_name, level, fac, __VA_ARGS__)

#define rd_rkb_dbg(rkb,ctx,fac,...) do {				\
		if (unlikely((rkb)->rkb_rk->rk_conf.debug &		\
			     RD_KAFKA_DBG_ ## ctx))			\
			rd_kafka_log0((rkb)->rkb_rk, (rkb)->rkb_name,	\
				      LOG_DEBUG, fac, __VA_ARGS__);		\
	} while (0)


void rd_kafka_q_init (rd_kafka_q_t *rkq);
rd_kafka_q_t *rd_kafka_q_new (void);
int rd_kafka_q_destroy (rd_kafka_q_t *rkq);
#define rd_kafka_q_keep(rkq) ((void)rd_atomic32_add(&(rkq)->rkq_refcnt, 1))

int rd_kafka_q_serve (rd_kafka_q_t *rkq, int timeout_ms,
		      int max_cnt,
		      void (*callback) (rd_kafka_op_t *rko,
					void *opaque),
		      void *opaque);
/**
 * Reset a queue.
 * WARNING: All messages will be lost and leaked.
 */
static __inline RD_UNUSED
void rd_kafka_q_reset (rd_kafka_q_t *rkq) {
	TAILQ_INIT(&rkq->rkq_q);
	rd_atomic32_set(&rkq->rkq_qlen, 0);
	rd_atomic64_set(&rkq->rkq_qsize, 0);
}

/**
 * Enqueue the 'rko' op at the tail of the queue 'rkq'.
 *
 * Locality: any thread.
 */
static __inline RD_UNUSED
void rd_kafka_q_enq (rd_kafka_q_t *rkq, rd_kafka_op_t *rko) {
	mtx_lock(&rkq->rkq_lock);
	if (!rkq->rkq_fwdq) {
		TAILQ_INSERT_TAIL(&rkq->rkq_q, rko, rko_link);
		(void)rd_atomic32_add(&rkq->rkq_qlen, 1);
		(void)rd_atomic64_add(&rkq->rkq_qsize, rko->rko_len);
		cnd_signal(&rkq->rkq_cond);
	} else
		rd_kafka_q_enq(rkq->rkq_fwdq, rko);

	mtx_unlock(&rkq->rkq_lock);
}


/**
 * Dequeue 'rko' from queue 'rkq'.
 *
 * NOTE: rkq_lock MUST be held
 * Locality: any thread
 */
static __inline RD_UNUSED
void rd_kafka_q_deq0 (rd_kafka_q_t *rkq, rd_kafka_op_t *rko) {
	TAILQ_REMOVE(&rkq->rkq_q, rko, rko_link);
	rd_atomic32_sub(&rkq->rkq_qlen, 1);
	rd_atomic64_sub(&rkq->rkq_qsize, rko->rko_len);
}

/**
 * Concat all elements of 'srcq' onto tail of 'rkq'.
 * 'rkq' will be be locked (if 'do_lock'==1), but 'srcq' will not.
 * NOTE: 'srcq' is not in a usable state after this call.
 *
 * Locality: any thread.
 */
static __inline RD_UNUSED
void rd_kafka_q_concat0 (rd_kafka_q_t *rkq, rd_kafka_q_t *srcq,
			 int do_lock) {
	if (do_lock)
		mtx_lock(&rkq->rkq_lock);
	if (!rkq->rkq_fwdq && !srcq->rkq_fwdq) {
		TAILQ_CONCAT(&rkq->rkq_q, &srcq->rkq_q, rko_link);
		(void)rd_atomic32_add(&rkq->rkq_qlen, rd_atomic32_get(&srcq->rkq_qlen));
		(void)rd_atomic64_add(&rkq->rkq_qsize, rd_atomic64_get(&srcq->rkq_qsize));
		cnd_signal(&rkq->rkq_cond);
	} else
		rd_kafka_q_concat0(rkq->rkq_fwdq ? rkq->rkq_fwdq : rkq,
				   srcq->rkq_fwdq ? srcq->rkq_fwdq : srcq,
				   rkq->rkq_fwdq ? do_lock : 0);
	if (do_lock)
		mtx_unlock(&rkq->rkq_lock);
}

#define rd_kafka_q_concat(dstq,srcq) rd_kafka_q_concat0(dstq,srcq,1/*lock*/)


/**
 * Prepend all elements of 'srcq' onto head of 'rkq'.
 * 'rkq' will be be locked (if 'do_lock'==1), but 'srcq' will not.
 *
 * NOTE: 'srcq' is not in a usable state after this call.
 *
 * Locality: any thread.
 */
static __inline RD_UNUSED
void rd_kafka_q_prepend0 (rd_kafka_q_t *rkq, rd_kafka_q_t *srcq,
                          int do_lock) {
	if (do_lock)
		mtx_lock(&rkq->rkq_lock);
	if (!rkq->rkq_fwdq && !srcq->rkq_fwdq) {
                /* Concat rkq on srcq */
                TAILQ_CONCAT(&srcq->rkq_q, &rkq->rkq_q, rko_link);
                /* Move srcq to rkq */
                TAILQ_MOVE(&rkq->rkq_q, &srcq->rkq_q, rko_link);
		rd_atomic32_add(&srcq->rkq_qlen,
                                rd_atomic32_get(&rkq->rkq_qlen));
		rd_atomic64_add(&srcq->rkq_qsize,
                                rd_atomic64_get(&rkq->rkq_qsize));
	} else
		rd_kafka_q_prepend0(rkq->rkq_fwdq ? rkq->rkq_fwdq : rkq,
                                    srcq->rkq_fwdq ? srcq->rkq_fwdq : srcq,
                                    rkq->rkq_fwdq ? do_lock : 0);
	if (do_lock)
		mtx_unlock(&rkq->rkq_lock);
}

#define rd_kafka_q_prepend(dstq,srcq) rd_kafka_q_prepend0(dstq,srcq,1/*lock*/)


/* Returns the number of elements in the queue */
static __inline RD_UNUSED
int rd_kafka_q_len (rd_kafka_q_t *rkq) {
	int qlen;
	mtx_lock(&rkq->rkq_lock);
	if (!rkq->rkq_fwdq)
		qlen = rd_atomic32_get(&rkq->rkq_qlen);
	else
		qlen = rd_kafka_q_len(rkq->rkq_fwdq);
	mtx_unlock(&rkq->rkq_lock);
	return qlen;
}

/* Returns the total size of elements in the queue */
static __inline RD_UNUSED
uint64_t rd_kafka_q_size (rd_kafka_q_t *rkq) {
	uint64_t sz;
	mtx_lock(&rkq->rkq_lock);
	if (!rkq->rkq_fwdq)
		sz = rd_atomic64_get(&rkq->rkq_qsize);
	else
		sz = rd_kafka_q_size(rkq->rkq_fwdq);
	mtx_unlock(&rkq->rkq_lock);
	return sz;
}


rd_kafka_op_t *rd_kafka_q_pop (rd_kafka_q_t *rkq, int timeout_ms,
                               int32_t version);

void rd_kafka_q_purge (rd_kafka_q_t *rkq);

int rd_kafka_q_move_cnt (rd_kafka_q_t *dstq, rd_kafka_q_t *srcq,
			 int cnt, int do_locks);


struct rd_kafka_queue_s {
	rd_kafka_q_t rkqu_q;
};


const char *rd_kafka_op2str (rd_kafka_op_type_t type);
void rd_kafka_op_destroy (rd_kafka_op_t *rko);
rd_kafka_op_t *rd_kafka_op_new (rd_kafka_op_type_t type);
void rd_kafka_op_reply2 (rd_kafka_t *rk, rd_kafka_op_t *rko);
void rd_kafka_op_reply0 (rd_kafka_t *rk, rd_kafka_op_t *rko,
			 rd_kafka_op_type_t type,
			 rd_kafka_resp_err_t err,
			 void *payload, int len);
void rd_kafka_op_reply (rd_kafka_t *rk,
			rd_kafka_op_type_t type,
			rd_kafka_resp_err_t err,
			void *payload, int len);
void rd_kafka_op_err (rd_kafka_t *rk, rd_kafka_resp_err_t err,
		      const char *fmt, ...);

#define rd_kafka_keep(rk) (void)rd_atomic32_add(&(rk)->rk_refcnt, 1)
void rd_kafka_destroy0 (rd_kafka_t *rk);

typedef	enum {
	_RK_GLOBAL = 0x1,
	_RK_PRODUCER = 0x2,
	_RK_CONSUMER = 0x4,
	_RK_TOPIC = 0x8
} rd_kafka_conf_scope_t;

void rd_kafka_anyconf_destroy (int scope, void *conf);

extern rd_atomic32_t rd_kafka_thread_cnt_curr;

extern char RD_TLS rd_kafka_thread_name[64];




void
RD_NORETURN
rd_kafka_crash (const char *file, int line, const char *function,
                rd_kafka_t *rk, const char *reason);

int rd_kafka_path_is_dir (const char *path);
