#include "dfs-master.h"
#include "dfs-cmd.h"
#include "nrf-router.h"
#include "nrf-interrupt.h" 
#include "dfs-common.h"
#include "exception-handlers.h"
#include "preemptive-thread.h"
#include "thread-safe-io.h"

static int fake_rand = -1;

int dfs_init_master(dfs_master_t *m, nrf_conf_t c) {
    // extern uint32_t default_interrupt_vector[];
    // int_vec_init((void *)default_interrupt_vector);
    // if (!cpsr_int_enabled()) {
    //     cpsr_int_enable();
    // }

    m->nrf = dfs_nrf_init(c, m->addr);
    if (c.id == NRF_LEFT)
        set_nrf_left(m->nrf);
    else
        set_nrf_right(m->nrf);
    

    // init master filesystem
    m->fs = kmalloc(sizeof(master_fat32_fs_t));
    master_fat32_fs_t fs = master_fat32_init();
    memcpy(m->fs, &fs, sizeof(master_fat32_fs_t));

    // init chunkserver state
    m->cur_hb_id = 0;
    m->num_act_server = 0;
    m->chunkservers = (dfs_chunkserver_status*)kmalloc(DFS_MAX_CHUNK_SERVER * sizeof(dfs_chunkserver_status));
    memset(m->chunkservers, 0, DFS_MAX_CHUNK_SERVER * sizeof(dfs_chunkserver_status));
    m->chunkservers[0].addr = dfs_chunk_addr1;
    m->chunkservers[1].addr = dfs_chunk_addr2;
    m->chunkservers[2].addr = dfs_chunk_addr3;

    // init ports
    memset(m->client_addrs, 0, sizeof(uint32_t) & DFS_MAX_CONN);

    return 0;
}

static int try_assign_port(dfs_master_t* master, uint32_t client_addr) {
    int first_idle = -1;
    for (int i = 0; i < DFS_MAX_CONN; i++) {
        if (master->client_addrs[i] == client_addr) {
            dfs_master_trace("client=%x already assigned to port=%d, reusing\n", client_addr, i);
            return i;
        } else if (master->client_addrs[i] == 0 && first_idle < 0) {
            first_idle = i;
        }
    }

    if (first_idle == -1) {
        assert(master->nrf->n_conns == DFS_MAX_CONN);
        dfs_master_trace("All ports occupied\n");
        return -1;
    }

    // gpio_set_off(master->config.ce_pin);
    // nrf_enable_rx(master, first_idle + 2);
    // gpio_set_on(master->config.ce_pin);
    // delay_us(130);
    master->client_addrs[first_idle] = client_addr;
    master->nrf->n_conns++;
    return first_idle;
}

int dfs_master_heartbeat_daemon_start(dfs_master_t *master) {
    for (int i = 0; i < dfs_master_lifecycle; i++) {
        dfs_hb_trace("MASTER HEARTBEAT CYCLE %d \n", i);
        hb_light_blink(master->hb_pin);
        delay_us(dfs_hb_window);
        
        // send one heartbeat packet to all chunkservers
        master->cur_hb_id++;
        for (int i = 0; i < master->num_chunkserver; i++) {
            cpsr_int_disable();

            dfs_chunkserver_status* chunkserver = &(master->chunkservers[i]);

            // first check whether the latest heartbeat of this chunkserver matches master's
            if (master->cur_hb_id - chunkserver->cur_hb_id > DFS_HB_MAX_LAG) {
                // this chunkserver didn't reply to the last heartbeat within the heartbeat window, should mark dead
                dfs_hb_trace("Mark chunkserver=%x as down\n", chunkserver->addr);
                chunkserver->status = CHUNK_DOWN;
                master->num_act_server--;
                // FIXME: should we still send heartbeat to dead server?
            } else {
                // if previously dead, should mark ok, TODO: any recovery work?
                if (chunkserver->status == CHUNK_DOWN) {
                    dfs_hb_trace("Mark chunkserver=%x as active\n", chunkserver->addr);
                    chunkserver->status = CHUNK_OK;
                    master->num_act_server++;
                }
            }

            dfs_hb_trace("Sending heartbeat to chunkserver=%x\n", chunkserver->addr);
            dfs_pkt hb_pkt;
            hb_pkt.op_code = DFS_HB;
            hb_pkt.t_pkt.hb_pkt.addr = master->addr;
            hb_pkt.t_pkt.hb_pkt.hb_id = master->cur_hb_id;
            uint32_t chunk_server_addr = chunkserver->addr;

            cpsr_int_enable();

            
            int ret = nrf_send_noack(master->nrf, chunk_server_addr, (void *)&hb_pkt, dfs_get_pkt_bytes());
            if (ret == -1) {
                dfs_hb_trace("Error sending heartbeat to chunkserver=%x failed\n", chunk_server_addr);
            } else if (ret != dfs_get_pkt_bytes()) {
                dfs_hb_trace("Send incomplete data\n");
            } 
        }
    }

    return 0;
}

void master_heartbeat(void *arg) {
    dfs_master_t *master = arg;
    _thread_safe_printk("start master hearbeat \n");
    dfs_master_heartbeat_daemon_start(master);
    _thread_safe_printk("end master heartbeat \n");
}

// return server id
int dfs_master_get_server_for_new_chunk(dfs_master_t* master) {
    if (master->num_act_server == 0) {
        dfs_master_trace("No active chunk server\n");
        return -1;
    }
    // select the server with the least usage
    // TODO: more eff data structure e.g. min heap
    float min_usage = 0;
    int server_id = 0;
    for (int i = 0; i < master->num_chunkserver; i++) {
        dfs_chunkserver_status* chunkserver =&(master->chunkservers[i]);
        if (chunkserver->status == CHUNK_OK) {
            // assert(chunkserver->storage > 0);
            // float cur_usage = chunkserver->usage * 1.f / chunkserver->storage;
            // if (cur_usage < min_usage) {
            //     min_usage = cur_usage;
            //     server_id = i;
            // }
            if (i == fake_rand) {
                fake_rand = -1;
                continue;
            } else {
                fake_rand = i;
            }
            server_id = i;
        }
    }

    return server_id;
}

int is_server_active_by_addr(dfs_master_t *master, uint32_t chunk_server_addr) {
    for (int j = 0; j < master->num_chunkserver; j++) {
        if (master->chunkservers[j].addr == chunk_server_addr) {
            if (master->chunkservers[j].status == CHUNK_OK) {
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

// event loop
int dfs_master_main_start(dfs_master_t *master) {
    // TODO: what should the master check on init? Ping chunkserver?

    // TODO: should keep listening? use interrupts?
    int shutdown = 0;
    for (int i = 0; i < dfs_master_lifecycle; i++){
        if (shutdown)
            return 0;
        // dfs_master_trace("MASTER MAIN CYCLE %d \n", i);
        char data[dfs_get_pkt_bytes()];
        int ret = nrf_read_exact_timeout(master->nrf, data, dfs_get_pkt_bytes(), dfs_master_ack_timeout_us);
        if (ret == -1) {
            dfs_master_trace("Receive time out\n");
            continue;
        } else if (ret != dfs_get_pkt_bytes()) {
            dfs_master_trace("Received incomplete data\n");
            continue;
        }
        // dfs_master_trace("received a packet \n");
        dfs_pkt *recv_pkt = (dfs_pkt *)data;

        switch (recv_pkt->op_code) {
            case DFS_CNCT: {
                // open a connection for this client if available
                uint32_t client_addr = recv_pkt->t_pkt.conn_pkt.addr;
                dfs_master_trace("Received connection request from client=%x\n", client_addr);

                int port_id = try_assign_port(master, client_addr);

                dfs_pkt send_pkt;
                if (port_id == -1) {
                    dfs_master_trace("Failed to assign port to client=%x\n", client_addr);

                    send_pkt.op_code = DFS_REJ;
                    int ret = nrf_send_noack(master->nrf, recv_pkt->t_pkt.conn_pkt.addr, (void *)&send_pkt, dfs_get_pkt_bytes());
                    if (ret == -1) {
                        dfs_master_trace("Error replying DFS_REJ to client=%x\n", client_addr);
                    } else if (ret != dfs_get_pkt_bytes()) {
                        dfs_master_trace("Send incomplete data\n");
                    } 
                } else {
                    dfs_master_trace("Assigned port=%d to client=%x\n", port_id, client_addr);
                    // send this address back to client and wait for ack
                    send_pkt.op_code = DFS_ACP;
                    send_pkt.port_id = port_id;
                    // send_pkt.t_pkt.conn_pkt.addr = ports[port_id];
                    int ret = nrf_send_noack(master->nrf, recv_pkt->t_pkt.conn_pkt.addr, (void *)&send_pkt, dfs_get_pkt_bytes());
                    if (ret == -1) {
                        dfs_master_trace("Error replying DFS_ACP to client=%x\n", client_addr);
                    } else if (ret != dfs_get_pkt_bytes()) {
                        dfs_master_trace("Send incomplete data\n");
                    } else {
                        client_fs[client_num] = master_fat32_client_init(client_num+1);
                        // client_num ++;
                    }
                }
                
                break;
            }
            case DFS_HB: {
                // chunk server sent hearbeat back, update state
                dfs_hb_trace("Received DFS_HB");

                cpsr_int_disable();
                uint32_t chunkserver_id = recv_pkt->port_id;  // NOTE: chunk server use this field to specify their server id
                dfs_hb_pkt hb_pkt = ((dfs_pkt *)data)->t_pkt.hb_pkt;
                dfs_chunkserver_status *chunkserver = &(master->chunkservers[chunkserver_id]);

                if (chunkserver->addr != hb_pkt.addr) {
                    dfs_hb_trace("Chunk server addr=%x mistach actual_addr=%x for server_id=%d\n", hb_pkt.addr, chunkserver->addr, chunkserver_id);
                    cpsr_int_enable();
                    continue;
                }

                // if (hb_pkt.hb_id > chunkserver->cur_hb_id) {
                //     dfs_master_trace("Received outdated heartbeat by=%d from chunkserver=%x, ignoring\n", chunserver->cur_hb_id chunkserver->addr);
                //     cpsr_int_enable();
                //     continue;
                // }

                dfs_hb_trace("Updating chunkserver=%x heartbeat id from %u to %u\n", chunkserver->addr, chunkserver->cur_hb_id, hb_pkt.hb_id);
                chunkserver->cur_hb_id = hb_pkt.hb_id;
                chunkserver->usage = hb_pkt.usage;
                chunkserver->storage = hb_pkt.storage;

                cpsr_int_enable();
                break;
            }
            case DFS_LS: {
                // get port_id, dispatch to a thread that handles that connection
                // get dirname from metadata
                // read FAT for all dirents in dirname
                // send all dirents back to client
                dfs_pkt send_pkt;

                // get port id to identify user 
                if (recv_pkt->port_id >= DFS_MAX_CONN) {
                    dfs_master_trace("Bad port id\n");
                    send_pkt.t_pkt.active_pkt.status = STATUS_FAILED;
                    // FIXME: don't know user address, can't reply
                    break;
                }

                uint32_t client_addr = master->client_addrs[recv_pkt->port_id];
                dfs_master_trace("Received DFS_LS from client=%x\n", client_addr);

                // char data[8192];
                // memset(data, 0, 8192);
                char *data = kmalloc(8192);
                int res = master_fat32_listdir(recv_pkt->t_pkt.placeholder, data); // fixme: hardcode client

                // for (int i = 0; i < files.ndirents; i++) {
                //     if (i == 0) {
                //         send_pkt.t_pkt.active_pkt.payload.data.offset = files.ndirents;
                //     } else if (i == files.ndirents-1) {
                //         send_pkt.op_code = DFS_END_LS;
                //     } else {
                //         send_pkt.op_code = DFS_LS;
                //     }   

                //     send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;
                //     memcpy(send_pkt.t_pkt.active_pkt.payload.data.data, files.dirents[i].name, strlen(files.dirents[i].name));
                //     int ret = nrf_send_ack(master->nrf, client_addr, (void*)&send_pkt, dfs_get_pkt_bytes());
                //     if (ret == -1) {
                //         dfs_master_trace("Error replying DFS_LS to client=%x\n", client_addr);
                //         break;
                //     } else if (ret != dfs_get_pkt_bytes()) {
                //         dfs_master_trace("Send incomplete data\n");
                //         break;
                //     } 
                // }

                unsigned len = strlen(data);
                unsigned idx = 0;
                if (len == 0 || res == 0) {
                    if (res != 0) {
                        send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;
                    } else {
                        send_pkt.t_pkt.active_pkt.status = STATUS_FAILED;
                    }
                    send_pkt.op_code = DFS_END_LS;
                    send_pkt.t_pkt.placeholder[0] = '\0';
                    int ret = nrf_send_noack(master->nrf, client_addr, (void*)&send_pkt, dfs_get_pkt_bytes());
                    if (ret == -1) {
                        dfs_master_trace("Error replying DFS_LS to client=%x\n", client_addr);
                        break;
                    } else if (ret != dfs_get_pkt_bytes()) {
                        dfs_master_trace("Send incomplete data\n");
                        break;
                    } 
                } else {
                    while (idx < len) {
                        send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;
                        if (idx + 27 >= len) {
                            send_pkt.op_code = DFS_END_LS;
                            memcpy(send_pkt.t_pkt.placeholder, data + idx, len - idx);
                            send_pkt.t_pkt.placeholder[len - idx] = '\0';
                            // _thread_safe_printk("end: %s\n", send_pkt.t_pkt.placeholder);
                        } else {
                            send_pkt.op_code = DFS_LS;
                            memcpy(send_pkt.t_pkt.placeholder, data + idx, 27);
                            send_pkt.t_pkt.placeholder[27] = '\0';
                            // _thread_safe_printk("not end: %s\n", send_pkt.t_pkt.placeholder);
                        }
                        
                        int ret = nrf_send_noack(master->nrf, client_addr, (void*)&send_pkt, dfs_get_pkt_bytes());
                        if (ret == -1) {
                            dfs_master_trace("Error replying DFS_LS to client=%x\n", client_addr);
                            break;
                        } else if (ret != dfs_get_pkt_bytes()) {
                            dfs_master_trace("Send incomplete data\n");
                            break;
                        } 
                        idx += 27;
                        delay_ms(20);
                    }
                }
                break;
            }
            case DFS_CD: {
                dfs_pkt send_pkt;
                send_pkt.op_code = DFS_CD;
                // get port id to identify user 
                if (recv_pkt->port_id >= DFS_MAX_CONN) {
                    dfs_master_trace("Bad port id\n");
                    break;
                }

                uint32_t client_addr = master->client_addrs[recv_pkt->port_id];
                dfs_master_trace("Received DFS_CD from client=%x\n", client_addr);

                // dfs_active_pkt* pkt = &(recv_pkt->t_pkt.active_pkt);
                // char *dir_name = pkt->payload.metadata.file_name;
                if (master_fat32_changedir(recv_pkt->t_pkt.placeholder)) {
                    send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;
                } else {
                    send_pkt.t_pkt.active_pkt.status = STATUS_FAILED;
                }

                int ret = nrf_send_ack(master->nrf, client_addr, (void*)&send_pkt, dfs_get_pkt_bytes());
                if (ret == -1) {
                    dfs_master_trace("Error replying DFS_CD to client=%x\n", client_addr);
                } else if (ret != dfs_get_pkt_bytes()) {
                    dfs_master_trace("Send incomplete data\n");
                }

                break;
            }
            case DFS_RD: {
                dfs_pkt send_pkt;
                send_pkt.op_code = DFS_RD;

                // get port id to identify user 
                if (recv_pkt->port_id >= DFS_MAX_CONN) {
                    dfs_master_trace("Bad port id\n");
                    send_pkt.t_pkt.active_pkt.status = STATUS_FAILED;
                    // FIXME: don't know user address, can't reply
                    break;
                }

                uint32_t client_addr = master->client_addrs[recv_pkt->port_id];
                dfs_master_trace("Received DFS_RD from client=%x\n", client_addr);

                dfs_active_pkt* pkt = &(recv_pkt->t_pkt.active_pkt);
                dfs_master_trace("Client read file: %s\n", pkt->payload.metadata.file_name);
                uint32_t chunk_num = master_fat32_get_chunk_num(pkt->payload.metadata.file_name);
                if (pkt->chunk_id_sqn >= chunk_num) {
                    if (pkt->chunk_id_sqn == 0) {
                        send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;
                        send_pkt.t_pkt.active_pkt.payload.metadata.nbytes = 0;
                    } else {
                        dfs_master_trace("Client requested invalid chunk sqn\n");
                        send_pkt.t_pkt.active_pkt.status = STATUS_FAILED;
                    }
                } else {
                    chunk_map_t handle = master_fat32_get_chunks(pkt->payload.metadata.file_name, pkt->chunk_id_sqn);
                
                    int chunk_server_addr = -1;
                    // for (int i = 0; i < handle.num_replica; i++) {
                    //     // TODO: can we switch to store server ID? so that this is O(1)
                    //     if (is_server_active_by_addr(master, handle.chunk_server_addrs[i])) {
                    //         chunk_server_addr = handle.chunk_server_addrs[i]; 
                    //         break;
                    //     }
                    // }
                    chunk_server_addr = handle.chunk_server_addrs[0]; 

                    // send read handle back
                    if (chunk_server_addr == -1) {
                        dfs_master_trace("No active chunk server available for chunk_sqn=%d\n", pkt->chunk_id_sqn);
                        send_pkt.t_pkt.active_pkt.status = STATUS_FAILED;
                    } else {
                        send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;
                        send_pkt.t_pkt.active_pkt.chunk_id_sqn = handle.chunk_id;
                        send_pkt.t_pkt.active_pkt.payload.metadata.chunk_server_addr = chunk_server_addr;
                        memcpy(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, pkt->payload.metadata.file_name, MAX_FILE_NAME); 
                        send_pkt.t_pkt.active_pkt.payload.metadata.nbytes = 1;                   
                    }
                
                }
                    
                int ret = nrf_send_ack(master->nrf, client_addr, (void*)&send_pkt, dfs_get_pkt_bytes());
                if (ret == -1) {
                    dfs_master_trace("Error replying DFS_RD to client=%x\n", client_addr);
                } else if (ret != dfs_get_pkt_bytes()) {
                    dfs_master_trace("Send incomplete data\n");
                } 

                break;
            }
            case DFS_END_RD:
            case DFS_RETRY_RD:
            case DFS_CR: {
                dfs_pkt send_pkt;
                send_pkt.op_code = DFS_CR;

                // get port id to identify user 
                if (recv_pkt->port_id >= DFS_MAX_CONN) {
                    dfs_master_trace("Bad port id\n");
                    send_pkt.t_pkt.active_pkt.status = STATUS_FAILED;
                    // FIXME: don't know user address, can't reply
                    break;
                }

                uint32_t client_addr = master->client_addrs[recv_pkt->port_id];
                dfs_master_trace("Received DFS_CR from client=%x\n", client_addr);

                dfs_active_pkt* pkt = &(recv_pkt->t_pkt.active_pkt);
                dfs_master_trace("Client create file: %s\n", pkt->payload.metadata.file_name);

                // FIXME: should not immediately create entry, only write to FAT on commit
                char filename[MAX_FILE_NAME + 1];
                memset(filename, 0, MAX_FILE_NAME + 1);  // zero out for null termination
                memcpy(filename, pkt->payload.metadata.file_name, pkt->payload.metadata.nbytes);

                if (strchr(filename, '.') == NULL) {
                    master_fat32_create(filename, 1); // FIXME: hardcoded client id
                } else {
                    master_fat32_create(filename, 0);  // FIXME: hardcoded client id
                }

                // send read handle back
                send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;
                memcpy(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, pkt->payload.metadata.file_name, MAX_FILE_NAME);
                int ret = nrf_send_ack(master->nrf, client_addr, (void*)&send_pkt, dfs_get_pkt_bytes());
                if (ret == -1) {
                    dfs_master_trace("Error replying DFS_CR to client=%x\n", client_addr);
                } else if (ret != dfs_get_pkt_bytes()) {
                    dfs_master_trace("Send incomplete data\n");
                } 
    
                break;
            }
            case DFS_RM: {
                dfs_pkt send_pkt;
                send_pkt.op_code = DFS_RM;

                // get port id to identify user 
                if (recv_pkt->port_id >= DFS_MAX_CONN) {
                    dfs_master_trace("Bad port id\n");
                    send_pkt.t_pkt.active_pkt.status = STATUS_FAILED;
                    // FIXME: don't know user address, can't reply
                    break;
                }

                uint32_t client_addr = master->client_addrs[recv_pkt->port_id];
                dfs_master_trace("Received DFS_CR from client=%x\n", client_addr);

                dfs_active_pkt* pkt = &(recv_pkt->t_pkt.active_pkt);
                dfs_master_trace("Client create file: %s\n", pkt->payload.metadata.file_name);

                // FIXME: should not immediately create entry, only write to FAT on commit
                char filename[MAX_FILE_NAME + 1];
                memset(filename, 0, MAX_FILE_NAME + 1);  // zero out for null termination
                memcpy(filename, pkt->payload.metadata.file_name, pkt->payload.metadata.nbytes);

                if (strchr(filename, '.') == NULL) {
                    master_fat32_delete(filename); // FIXME: hardcoded client id
                } else {
                    master_fat32_delete(filename);  // FIXME: hardcoded client id
                }

                // send read handle back
                send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;
                memcpy(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, pkt->payload.metadata.file_name, MAX_FILE_NAME);
                int ret = nrf_send_ack(master->nrf, client_addr, (void*)&send_pkt, dfs_get_pkt_bytes());
                if (ret == -1) {
                    dfs_master_trace("Error replying DFS_RM to client=%x\n", client_addr);
                } else if (ret != dfs_get_pkt_bytes()) {
                    dfs_master_trace("Send incomplete data\n");
                } 
                break;
            }
            case DFS_WR: {
                dfs_pkt send_pkt;
                send_pkt.op_code = DFS_WR;

                // get port id to identify user 
                if (recv_pkt->port_id >= DFS_MAX_CONN) {
                    dfs_master_trace("Bad port id\n");
                    send_pkt.t_pkt.active_pkt.status = STATUS_FAILED;
                    // FIXME: don't know user address, can't reply
                    break;
                }

                uint32_t client_addr = master->client_addrs[recv_pkt->port_id];
                dfs_master_trace("Received DFS_WR from client=%x\n", client_addr);

                dfs_active_pkt* pkt = &(recv_pkt->t_pkt.active_pkt);
                dfs_master_trace("Client write to file: %s\n", pkt->payload.metadata.file_name);

                uint32_t chunk_sqn = pkt->chunk_id_sqn;
                uint32_t chunk_num = master_fat32_get_chunk_num(pkt->payload.metadata.file_name);
                uint32_t chunk_id = 0;
                int chunk_server_addr = -1;

                if (chunk_sqn < chunk_num) {
                    chunk_id = chunk_num;
                    chunk_map_t handle = master_fat32_get_chunks(pkt->payload.metadata.file_name, pkt->chunk_id_sqn);
                    // for (int i = 0; i < handle.num_replica; i++) {
                    //     // TODO: can we switch to store server ID? so that this is O(1)
                    //     if (is_server_active_by_addr(master, handle.chunk_server_addrs[i])) {
                    //         chunk_server_addr = handle.chunk_server_addrs[i]; 
                    //         break;
                    //     }
                    // }
                    chunk_server_addr = handle.chunk_server_addrs[0]; 
                    send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;
                } else if (chunk_sqn == chunk_num) {
                    // chunk_id = chunk_num + 1;  // FIXME: duplicate chunk id for different files?

                    int server_id = dfs_master_get_server_for_new_chunk(master);
                    if (server_id == -1) {
                        send_pkt.t_pkt.active_pkt.status = STATUS_FAILED;
                        dfs_master_trace("failed to get valid server id \n");
                    } else {
                        chunk_id = client_fs[client_num].chunk_id;
                        dfs_master_trace("chunk id is %d \n", chunk_id);
                        send_pkt.t_pkt.active_pkt.status = STATUS_SUCCESS;
                        chunk_server_addr = master->chunkservers[server_id].addr;
                        master_fat32_add_chunk_server(pkt->payload.metadata.file_name, chunk_id, chunk_num, chunk_server_addr);
                        client_fs[client_num].chunk_id++;
                    }
                } else {
                    dfs_master_trace("Invalid chunk sqn");
                    send_pkt.t_pkt.active_pkt.status = STATUS_FAILED;
                }

                // send read handle back
                if (send_pkt.t_pkt.active_pkt.status == STATUS_SUCCESS) {
                    send_pkt.t_pkt.active_pkt.chunk_id_sqn = chunk_id;
                    send_pkt.t_pkt.active_pkt.payload.metadata.chunk_server_addr = chunk_server_addr;
                    memcpy(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, pkt->payload.metadata.file_name, MAX_FILE_NAME);
                }

                int ret = nrf_send_ack(master->nrf, client_addr, (void*)&send_pkt, dfs_get_pkt_bytes());
                if (ret == -1) {
                    dfs_master_trace("Error replying DFS_RD to client=%x\n", client_addr);
                } else if (ret != dfs_get_pkt_bytes()) {
                    dfs_master_trace("Send incomplete data\n");
                } else {
                    dfs_master_trace("Send success\n");
                }

                break;
            }
            case DFS_END_WR:
                break;
            case DFS_RETRY_WR:
                break;
            case DFS_COMMIT_WR:
                break;
            case DFS_SD:
                shutdown = 1;
                dfs_master_trace("received shutdown\n");
                break;
            default:
                not_reached();
        }
    }
}

void master_main_loop(void *arg) {
    dfs_master_t *master = arg;
    _thread_safe_printk("start master loop\n");
    dfs_master_main_start(master);
    _thread_safe_printk("end master loop\n");
}

void dfs_master_start(dfs_master_t *master) {
    // dfs_init_master(master_init->master, master_init->config);
    pre_fork(master_main_loop, (void *)(master), 1, 1);
    pre_fork(master_heartbeat, (void *)(master), 1, 1);
}
