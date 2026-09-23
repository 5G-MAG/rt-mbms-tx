/**
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "rf_zmq_imp_trx.h"
#include <inttypes.h>
#include <srsran/config.h>
#include <srsran/phy/utils/vector.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zmq.h>

int rf_zmq_tx_open(rf_zmq_tx_t* q, rf_zmq_opts_t opts, void* zmq_ctx, char* sock_args)
{
  int ret = SRSRAN_ERROR;

  if (q) {
    // Zero object
    bzero(q, sizeof(rf_zmq_tx_t));

    // Copy id
    strncpy(q->id, opts.id, ZMQ_ID_STRLEN - 1);
    q->id[ZMQ_ID_STRLEN - 1] = '\0';

    // Create socket
    q->sock = zmq_socket(zmq_ctx, opts.socket_type);
    if (!q->sock) {
      fprintf(stderr, "[zmq] Error: creating transmitter socket\n");
      goto clean_exit;
    }
    q->socket_type   = opts.socket_type;
    q->sample_format = opts.sample_format;
    q->frequency_mhz = opts.frequency_mhz;
    q->sample_offset = opts.sample_offset;

    /* ZMQ_PUB's default send high-water-mark is 1000 messages: if a
     * subscriber (the modem's zmqrx bridge) ever falls behind by more than
     * that many queued messages - even briefly, e.g. a scheduling hiccup -
     * ZMQ silently DROPS the backlog rather than blocking this publisher.
     * A bounded-but-larger HWM absorbs that kind of transient stall without
     * the silent drop.
     *
     * Deliberately NOT unlimited (0): the live-measured ~91%-of-nominal raw
     * throughput ceiling on this ZMQ loopback rig turns out to be a
     * PERSISTENT, chronic deficit, not just an occasional stall - live
     * memory-growth risk analysis (2026-07-18) showed that with HWM=0 this
     * queue would grow roughly unboundedly for as long as the deficit
     * persists, since no finite buffer can absorb a SUSTAINED rate mismatch,
     * only a transient one. That's harmless at a decimated (ratio>1)
     * receive rate (the consumer's actual need is below what's delivered
     * even with the deficit, so the queue doesn't grow there at all) but a
     * real, unbounded-memory-growth risk at ratio=1 (a signalled
     * pmch-Bandwidth-r17 wideband PMCH needing the full native rate, where
     * the deficit is never absorbed and an unlimited HWM just delays,
     * rather than prevents, eventual exhaustion). A large bounded value
     * keeps the transient-stall protection this was originally added for
     * while capping the worst case; it doesn't fix the ratio=1 throughput
     * deficit itself (still open, see SIB13_MBSFN_TEST_RESULTS.md), which
     * needs a real throughput fix, not more buffering. */
    if (opts.socket_type == ZMQ_PUB) {
      int sndhwm = 50000;
      if (zmq_setsockopt(q->sock, ZMQ_SNDHWM, &sndhwm, sizeof(sndhwm)) == -1) {
        fprintf(stderr, "Error: setting send HWM on tx socket\n");
        goto clean_exit;
      }
    }

    rf_zmq_info(q->id, "Binding transmitter: %s\n", sock_args);

    ret = zmq_bind(q->sock, sock_args);
    if (ret) {
      fprintf(stderr, "Error: binding transmitter socket (%s): %s\n", sock_args, zmq_strerror(zmq_errno()));
      goto clean_exit;
    }

    if (opts.trx_timeout_ms) {
      int timeout = opts.trx_timeout_ms;
      if (zmq_setsockopt(q->sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        fprintf(stderr, "Error: setting receive timeout on tx socket\n");
        goto clean_exit;
      }

      if (zmq_setsockopt(q->sock, ZMQ_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
        fprintf(stderr, "Error: setting send timeout on tx socket\n");
        goto clean_exit;
      }

      timeout = 0;
      if (zmq_setsockopt(q->sock, ZMQ_LINGER, &timeout, sizeof(timeout)) == -1) {
        fprintf(stderr, "Error: setting linger timeout on tx socket\n");
        goto clean_exit;
      }
    }

    if (pthread_mutex_init(&q->mutex, NULL)) {
      fprintf(stderr, "Error: creating mutex\n");
      goto clean_exit;
    }

    q->temp_buffer_convert = srsran_vec_malloc(ZMQ_MAX_BUFFER_SIZE);
    if (!q->temp_buffer_convert) {
      fprintf(stderr, "Error: allocating rx buffer\n");
      goto clean_exit;
    }

    q->zeros = srsran_vec_malloc(ZMQ_MAX_BUFFER_SIZE);
    if (!q->zeros) {
      fprintf(stderr, "Error: allocating zeros\n");
      goto clean_exit;
    }
    bzero(q->zeros, ZMQ_MAX_BUFFER_SIZE);

    q->running = true;

    ret = SRSRAN_SUCCESS;
  }

clean_exit:
  return ret;
}

static int _rf_zmq_tx_baseband(rf_zmq_tx_t* q, cf_t* buffer, uint32_t nsamples)
{
  int n = SRSRAN_ERROR;

  while (n < 0 && q->running) {
    // Receive Transmit request is socket type is REPLY
    if (q->socket_type == ZMQ_REP) {
      uint8_t dummy;
      n = zmq_recv(q->sock, &dummy, sizeof(dummy), 0);
      if (n < 0) {
        if (rf_zmq_handle_error(q->id, "tx request receive")) {
          n = SRSRAN_ERROR;
          goto clean_exit;
        }
      } else {
        // Tx request received successful
        rf_zmq_info(q->id, " - tx request received\n");
        rf_zmq_info(q->id, " - sending %d samples (%d B)\n", nsamples, NSAMPLES2NBYTES(nsamples));
      }
    } else {
      n = 1;
    }

    // convert samples if necessary
    void*    buf       = (buffer) ? buffer : q->zeros;
    uint32_t sample_sz = sizeof(cf_t);

    if (q->sample_format == ZMQ_TYPE_SC16) {
      buf       = q->temp_buffer_convert;
      sample_sz = 2 * sizeof(short);
      srsran_vec_convert_fi((float*)buffer, INT16_MAX, (short*)q->temp_buffer_convert, 2 * nsamples);
    }

    // Send base-band if request was received
    if (n > 0) {
      n = zmq_send(q->sock, buf, (size_t)sample_sz * nsamples, 0);
      if (n < 0) {
        if (rf_zmq_handle_error(q->id, "tx baseband send")) {
          n = SRSRAN_ERROR;
          goto clean_exit;
        }
      } else if (n != NSAMPLES2NBYTES(nsamples)) {
        rf_zmq_error(q->id,
                     "[zmq] Error: transmitter expected %d bytes and sent %d. %s.\n",
                     NSAMPLES2NBYTES(nsamples),
                     n,
                     strerror(zmq_errno()));
        n = SRSRAN_ERROR;
        goto clean_exit;
      }
    }

    // If failed to receive request or send base-band, keep trying
  }

  // Increment sample counter
  q->nsamples += nsamples;
  n = nsamples;

  /* TX_SEND_RATE_DIAG: directly measure the actual TX send rate (samples/sec
   * actually pushed to zmq_send()), decoupled from the RX-side pacing loop
   * measured separately -- part of the n_prb=25 1.25kHz FeMBMS throughput-
   * shortfall investigation (RX side sees only ~20% of nominal raw
   * throughput; CPU confirmed idle). See fembms-mbsfn-cfo-investigation
   * memory. Logs once per ~1s of accumulated samples at base rate. */
  if (getenv("TX_SEND_RATE_DIAG")) {
    static uint64_t total_samples    = 0;
    static struct timespec last_log  = {0, 0};
    total_samples += nsamples;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (last_log.tv_sec == 0) {
      last_log = now;
    }
    double elapsed = (now.tv_sec - last_log.tv_sec) + (now.tv_nsec - last_log.tv_nsec) / 1e9;
    if (elapsed >= 1.0) {
      fprintf(stderr, "TX_SEND_RATE_DIAG samples_per_sec=%.0f total_samples=%llu elapsed=%.3f\n",
              total_samples / elapsed, (unsigned long long)total_samples, elapsed);
      total_samples = 0;
      last_log      = now;
    }
  }

clean_exit:
  return n;
}

int rf_zmq_tx_align(rf_zmq_tx_t* q, uint64_t ts)
{
  pthread_mutex_lock(&q->mutex);

  int64_t nsamples = (int64_t)ts - (int64_t)q->nsamples;

  if (nsamples > 0) {
    rf_zmq_info(q->id, " - Detected Tx gap of %d samples.\n", nsamples);
    _rf_zmq_tx_baseband(q, q->zeros, (uint32_t)nsamples);
  }

  pthread_mutex_unlock(&q->mutex);

  return (int)nsamples;
}

int rf_zmq_tx_baseband(rf_zmq_tx_t* q, cf_t* buffer, uint32_t nsamples)
{
  int n;

  pthread_mutex_lock(&q->mutex);

  if (q->sample_offset > 0) {
    _rf_zmq_tx_baseband(q, q->zeros, (uint32_t)q->sample_offset);
    q->sample_offset = 0;
  } else if (q->sample_offset < 0) {
    n = SRSRAN_MIN(-q->sample_offset, nsamples);
    buffer += n;
    nsamples -= n;
    q->sample_offset += n;
    if (nsamples == 0) {
      return n;
    }
  }

  n = _rf_zmq_tx_baseband(q, buffer, nsamples);

  pthread_mutex_unlock(&q->mutex);

  return n;
}

int rf_zmq_tx_get_nsamples(rf_zmq_tx_t* q)
{
  pthread_mutex_lock(&q->mutex);
  int ret = q->nsamples;
  pthread_mutex_unlock(&q->mutex);
  return ret;
}

int rf_zmq_tx_zeros(rf_zmq_tx_t* q, uint32_t nsamples)
{
  pthread_mutex_lock(&q->mutex);

  rf_zmq_info(q->id, " - Tx %d Zeros.\n", nsamples);
  _rf_zmq_tx_baseband(q, q->zeros, (uint32_t)nsamples);

  pthread_mutex_unlock(&q->mutex);

  return (int)nsamples;
}

bool rf_zmq_tx_match_freq(rf_zmq_tx_t* q, uint32_t freq_hz)
{
  bool ret = false;
  if (q) {
    ret = (q->frequency_mhz == 0 || q->frequency_mhz == freq_hz);
  }
  return ret;
}

void rf_zmq_tx_close(rf_zmq_tx_t* q)
{
  pthread_mutex_lock(&q->mutex);
  q->running = false;
  pthread_mutex_unlock(&q->mutex);

  pthread_mutex_destroy(&q->mutex);

  if (q->zeros) {
    free(q->zeros);
  }

  if (q->temp_buffer_convert) {
    free(q->temp_buffer_convert);
  }

  if (q->sock) {
    zmq_close(q->sock);
    q->sock = NULL;
  }
}

bool rf_zmq_tx_is_running(rf_zmq_tx_t* q)
{
  if (!q) {
    return false;
  }

  bool ret = false;
  pthread_mutex_lock(&q->mutex);
  ret = q->running;
  pthread_mutex_unlock(&q->mutex);

  return ret;
}
