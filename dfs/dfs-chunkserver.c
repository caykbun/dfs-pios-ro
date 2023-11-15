#include "dfs-chunkserver.h"
#include "dfs-common.h"
#include "nrf-interrupt.h"
#include "thread-safe-io.h"
#include "dfs-cmd.h"
#include "nrf-interrupt.h" 
#include "dfs-common.h"
#include "exception-handlers.h"
#include "preemptive-thread.h"
#include "thread-safe-io.h"

int dfs_init_chunkserver(dfs_chunkserver_t *cserver, nrf_conf_t c) {
    // nrf_t *n = kmalloc(sizeof *n);
    // n->config = c;
    // nrf_stat_start(n);
    // n->spi = pin_init(c.ce_pin, c.spi_chip);

    // nrf_set_pwrup_off(n);

    // // setup rx pipe 1 for receiving register packets and pipe 0 for auto acking
    // nrf_put8(n, NRF_EN_RXADDR, 0b11); 
    // nrf_put8(n, NRF_EN_AA, 0b111111);
    // nrf_put8(n, NRF_FEATURE, 0b01); // enable no ack
    // n->rxaddr[1] = cserver_addr;
    // nrf_set_addr(n, NRF_RX_ADDR_P1, n->rxaddr[1], nrf_default_addr_nbytes);
    // nrf_put8(n, NRF_RX_PW_P1, n->config.nbytes);

    // nrf_put8(n, NRF_SETUP_RETR, nrf_default_retran_attempts + ((20000 / 250 - 1)<<4));
    // dfs_chunkserver_t* cserver = kmalloc(sizeof(dfs_chunkserver_t));
    // nrf_rt_init_global(n);
    // extern uint32_t default_interrupt_vector[];
    // int_vec_init((void *)default_interrupt_vector);
    // cpsr_int_enable();

    cserver->nrf = dfs_nrf_init(c, cserver->addr);
    // set_server(cserver->nrf);
    if (c.id == NRF_LEFT)
        set_nrf_left(cserver->nrf);
    else
        set_nrf_right(cserver->nrf);
    // cserver->nrf = n;
    
    // init fs
    cserver->fs = kmalloc(sizeof(chunkserver_fat32_fs_t));
    chunkserver_fat32_fs_t fs = chunkserver_fat32_init();
    memcpy(cserver->fs, &fs, sizeof(chunkserver_fat32_fs_t));

    // init ports
    memset(cserver->client_addrs, 0, sizeof(uint32_t) * DFS_MAX_CONN);

    return 0;
}

static int try_assign_port(dfs_chunkserver_t* cserver, uint32_t client_addr) {
    int first_idle = -1;
    for (int i = 0; i < DFS_MAX_CONN; i++) {
        if (cserver->client_addrs[i] == client_addr) {
            dfs_chunk_trace("client=%x already assigned to port=%d, reusing\n", client_addr, i);
            return i;
        } else if (cserver->client_addrs[i] == 0 && first_idle < 0) {
            first_idle = i;
        }
    }

    if (first_idle == -1) {
        assert(cserver->nrf->n_conns == DFS_MAX_CONN);
        dfs_chunk_trace("All ports occupied\n");
        return -1;
    }

    // gpio_set_off(master->config.ce_pin);
    // nrf_enable_rx(master, first_idle + 2);
    // gpio_set_on(master->config.ce_pin);
    // delay_us(130);
    cserver->client_addrs[first_idle] = client_addr;
    cserver->nrf->n_conns++;
    return first_idle;
}

/* initilize the distributed file system and starts receiving*/
int dfs_chunkserver_main_start(dfs_chunkserver_t *cserver) {
    
    // FIXME: should keep listening? use interrupts?
    for (int i = 0; i < dfs_chunk_lifecycle; i++){
        dfs_chunk_trace("CHUNK MAIN CYCLE %d \n", i);
        // if (i % 3 == 0 && i != 0) {
        //     dfs_chunk_trace("Pausing Chunk server------------------------\n", i);
        //     delay_us(5*10000000);
        // } 
        char data[dfs_get_pkt_bytes()];
        int ret = nrf_read_exact_timeout(cserver->nrf, data, dfs_get_pkt_bytes(), dfs_chunk_ack_timeout_us);
        if (ret == -1) {
            dfs_chunk_trace("Receive time out\n");
            continue;
        } else if (ret != dfs_get_pkt_bytes()) {
            dfs_chunk_trace("Received incomplete data\n");
            continue;
        }
        // dfs_chunk_trace("received a packet \n");
        dfs_pkt *recv_pkt = (dfs_pkt *)data;

        switch (recv_pkt->op_code) {
            case DFS_CNCT: {
                // open a connection for this client if available
                // TODO: use ack payload?
                uint32_t client_addr = recv_pkt->t_pkt.conn_pkt.addr;
                dfs_chunk_trace("Received connection request from client=%x\n", client_addr);

                int port_id = try_assign_port(cserver, client_addr);

                dfs_pkt send_pkt;
                if (port_id == -1) {
                    dfs_chunk_trace("Failed to assign port to client=%x\n", client_addr);

                    send_pkt.op_code = DFS_REJ;
                    int ret = nrf_send_noack(cserver->nrf, recv_pkt->t_pkt.conn_pkt.addr, (void *)&send_pkt, dfs_get_pkt_bytes());
                    if (ret == -1) {
                        dfs_chunk_trace("Error replying DFS_REJ to client=%x\n", client_addr);
                    } else if (ret != dfs_get_pkt_bytes()) {
                        dfs_chunk_trace("Send incomplete data\n");
                    } 
                } else {
                    dfs_chunk_trace("Assigned port=%d to client=%x\n", port_id, client_addr);
                    // send this address back to client and wait for ack
                    send_pkt.op_code = DFS_ACP;
                    send_pkt.port_id = port_id;
                    // send_pkt.t_pkt.conn_pkt.addr = ports[port_id];
                    int ret = nrf_send_noack(cserver->nrf, recv_pkt->t_pkt.conn_pkt.addr, (void *)&send_pkt, dfs_get_pkt_bytes());
                    if (ret == -1) {
                        dfs_chunk_trace("Error replying DFS_ACP to client=%x\n", client_addr);
                    } else if (ret != dfs_get_pkt_bytes()) {
                        dfs_chunk_trace("Send incomplete data\n");
                    } 
                }
                
                break;
            }
            case DFS_HB: {
                dfs_hb_trace("Chunkserver id=%d addr=%x received DFS_HB", cserver->id, cserver->addr);
                hb_light_blink(cserver->hb_pin);

                dfs_hb_pkt* hb_pkt = &(recv_pkt->t_pkt.hb_pkt);
                dfs_pkt send_pkt;
                send_pkt.op_code = DFS_HB;
                send_pkt.port_id = cserver->id;  // use this field to specify server id
                send_pkt.t_pkt.hb_pkt.hb_id = hb_pkt->hb_id;
                send_pkt.t_pkt.hb_pkt.addr = cserver->addr; 
                send_pkt.t_pkt.hb_pkt.usage = 0;  // TODO: fat should provide this interface
                send_pkt.t_pkt.hb_pkt.storage = 32 * 1024; // FIXME: hardcode 32 MB

                int ret = nrf_send_noack(cserver->nrf, recv_pkt->t_pkt.hb_pkt.addr, (void *)&send_pkt, dfs_get_pkt_bytes());
                if (ret == -1) {
                    dfs_hb_trace("Error replying to heartbeat id=%u\n", hb_pkt->hb_id);
                } else if (ret != dfs_get_pkt_bytes()) {
                    dfs_hb_trace("Send incomplete data\n");
                }
                break;
            }
            case DFS_RD: {

                // get port id to identify user 
                if (recv_pkt->port_id >= DFS_MAX_CONN) {
                    dfs_chunk_trace("Bad port id\n");
                    // FIXME: don't know user address, can't reply
                    break;
                }

                uint32_t client_addr = cserver->client_addrs[recv_pkt->port_id];
                dfs_chunk_trace("Received DFS_RD from client=%x\n", client_addr);

                dfs_active_pkt* pkt = &(recv_pkt->t_pkt.active_pkt);
                dfs_chunk_trace("Client read chunk_id=%x\n", pkt->chunk_id_sqn);

                pi_file_t *chunk = chunkserver_read(cserver->fs, pkt->chunk_id_sqn);

                // send chunk data back
                dfs_data_query* query = &(pkt->payload.dataquery);
                uint32_t bytes_to_send =  query->nbytes;
                if (query->offset >= chunk->n_data) {
                    dfs_chunk_trace("Invalid offset, ignored\n");
                } else if (query->offset + bytes_to_send > chunk->n_data) {
                    dfs_chunk_trace("Client specified nbytes too large, sending upto chunk boundary\n");
                    bytes_to_send = chunk->n_data - query->offset;
                }

                uint32_t bytes_sent = 0;
                while (bytes_sent < bytes_to_send) {
                    dfs_pkt send_pkt;
                    send_pkt.op_code = DFS_RD;

                    send_pkt.t_pkt.active_pkt.chunk_id_sqn = pkt->chunk_id_sqn;
                    send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;

                    uint32_t cur_offset = query->offset + bytes_sent;
                    send_pkt.t_pkt.active_pkt.payload.data.offset = cur_offset;

                    uint32_t nbytes = CHUNK_SERVER_PKT_SIZE;
                    if (bytes_to_send -  bytes_sent <= CHUNK_SERVER_PKT_SIZE) {
                        nbytes = bytes_to_send -  bytes_sent;
                        send_pkt.op_code = DFS_END_RD;
                    }
                    memset(send_pkt.t_pkt.active_pkt.payload.data.data, 0, CHUNK_SERVER_PKT_SIZE);
                    memcpy(send_pkt.t_pkt.active_pkt.payload.data.data, chunk->data + cur_offset, nbytes);

                    int ret = nrf_send_ack(cserver->nrf, client_addr, (void*)&send_pkt, dfs_get_pkt_bytes());
                    if (ret == -1) {
                        dfs_chunk_trace("Error sending chunk_id=%x offset=%x to client=%x, aborting\n", pkt->chunk_id_sqn, cur_offset, client_addr);
                        break;
                    } else if (ret != dfs_get_pkt_bytes()) {
                        dfs_chunk_trace("Send incomplete data, aborting\n");
                        break;
                    }

                    bytes_sent += CHUNK_SERVER_PKT_SIZE;
                }                
                break;
            }
            case DFS_RM:
            case DFS_WR: {
                dfs_chunk_trace("Chunkserver received WR rqst \n");
                uint32_t chunk_id = recv_pkt->t_pkt.active_pkt.chunk_id_sqn;
                if (recv_pkt->t_pkt.active_pkt.payload.data.offset == 0) {
                    chunkserver_create_chunk(cserver->fs, chunk_id);
                }
                char *data = kmalloc(CHUNK_SERVER_PKT_SIZE+1);
                memset(data, 0, CHUNK_SERVER_PKT_SIZE);
                memcpy(data, recv_pkt->t_pkt.active_pkt.payload.data.data, CHUNK_SERVER_PKT_SIZE);
                chunkserver_append_chunk(cserver->fs, chunk_id, data);
                                
                break;
            }
            case DFS_END_WR: {
                dfs_chunk_trace("Chunkserver received END_WR rqst \n");
                uint32_t chunk_id = recv_pkt->t_pkt.active_pkt.chunk_id_sqn;
                if (recv_pkt->t_pkt.active_pkt.payload.data.offset == 0) {
                    chunkserver_create_chunk(cserver->fs, chunk_id);
                }
                char *data = kmalloc(CHUNK_SERVER_PKT_SIZE + 1);
                memset(data, 0, CHUNK_SERVER_PKT_SIZE);
                memcpy(data, recv_pkt->t_pkt.active_pkt.payload.data.data, CHUNK_SERVER_PKT_SIZE);
                chunkserver_append_chunk(cserver->fs, chunk_id, data);

                dfs_pkt send_pkt;
                send_pkt.op_code = DFS_END_WR;
                send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;
                int ret = nrf_send_ack(cserver->nrf, client_addr, (void*)&send_pkt, dfs_get_pkt_bytes());
                if (ret == -1) {
                    dfs_chunk_trace("Error sending END_WR, aborting\n");
                    break;
                } else if (ret != dfs_get_pkt_bytes()) {
                    dfs_chunk_trace("Send incomplete data, aborting\n");
                    break;
                }

                break;
            }
            default:
                not_reached();
        }
    }

    return 0;
}

void chunkserver_main_loop(void *arg) {
    dfs_chunkserver_t *chunkserver = arg;
    _thread_safe_printk("start chunkserver loop\n");
    dfs_chunkserver_main_start(chunkserver);
    _thread_safe_printk("end chunkserver loop\n");
}

void dfs_chunkserver_start(dfs_chunkserver_t *chunkserver) {
    pre_fork(chunkserver_main_loop, (void *)(chunkserver), 1, 1);
}
