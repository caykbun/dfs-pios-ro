#include "dfs-client.h"
#include "dfs-cmd.h"
#include "dfs-common.h"
#include "nrf-interrupt.h"

int dfs_init_client(dfs_client_t * client, nrf_conf_t c) {
    // nrf_t *n = kmalloc(sizeof *n);
    // n->config = c;
    // nrf_stat_start(n);
    // n->spi = pin_init(c.ce_pin, c.spi_chip);

    // nrf_set_pwrup_off(n);

    // // setup rx pipe 1 for receiving register packets and pipe 0 for auto acking
    // nrf_put8(n, NRF_EN_RXADDR, 0b11); 
    // nrf_put8(n, NRF_EN_AA, 0b11);
    // n->rxaddr[1] = client_addr;
    // nrf_set_addr(n, NRF_RX_ADDR_P1, n->rxaddr[1], nrf_default_addr_nbytes);
    // nrf_put8(n, NRF_RX_PW_P1, n->config.nbytes);

    // nrf_rt_init_global(n);

    client->addr = client_addr;
    client->nrf = dfs_nrf_init(c, client_addr);
    if (c.id == NRF_LEFT)
        set_nrf_left(client->nrf);
    else
        set_nrf_right(client->nrf);
    
    return 0;
}

int dfs_client_shutdown_server(dfs_connection_t *conn) {
    // send connect packet
    dfs_pkt send_pkt;
    send_pkt.op_code = DFS_SD;
    send_pkt.t_pkt.conn_pkt.addr = conn->client->addr;
    int ret = nrf_send_noack(conn->client->nrf, conn->server_addr, (void *)&send_pkt, dfs_get_pkt_bytes());
    if (ret == -1) {
        dfs_client_trace("Error sending DFS_SD to server=%x\n", conn->server_addr);
        return -1;
    } 

    return 0;
} 

int dfs_client_try_connect(dfs_connection_t *conn) {
    // send connect packet
    dfs_pkt send_pkt;
    send_pkt.op_code = DFS_CNCT;
    send_pkt.t_pkt.conn_pkt.addr = conn->client->addr;
    dfs_client_trace("trying to connect to addr %x self %x\n", conn->server_addr, conn->client->addr);
    nrf_send_noack(conn->client->nrf, conn->server_addr, (void *)&send_pkt, dfs_get_pkt_bytes());

    int ret = -1;
    while (ret == -1) {
        // wait for reply
        char data[dfs_get_pkt_bytes()];
        ret = nrf_read_exact_timeout(conn->client->nrf, data, dfs_get_pkt_bytes(), 200000);
        // if (ret == -1) {
        //     dfs_client_trace("Connect attempt time out to server=%x\n", conn->server_addr);
        //     return -1;
        // } else if (ret != dfs_get_pkt_bytes()) {
        //     dfs_client_trace("Received incomplete data\n");
        //     return -1;
        // }
        if (ret != -1) {
            dfs_pkt *recv_pkt = (dfs_pkt *)data;
            if (recv_pkt->op_code == DFS_ACP) {
                conn->port_id = recv_pkt->port_id;
                // conn->server_addr = recv_pkt->t_pkt.conn_pkt.addr;
                dfs_client_trace("Connection established with server=%x\n", conn->server_addr);
                return 0;
            }
        }
        dfs_client_trace("trying to connect to addr %x self %x\n", conn->server_addr, conn->client->addr);
        nrf_send_noack(conn->client->nrf, conn->server_addr, (void *)&send_pkt, dfs_get_pkt_bytes());
    }
    return 0;
} 

uint32_t dfs_client_create(dfs_connection_t *conn, const char* filename, uint32_t name_len) {
    assert(name_len <= 12);

    dfs_pkt send_pkt;
    send_pkt.op_code = DFS_CR;
    send_pkt.port_id = conn->port_id;
    memset(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, 0, MAX_FILE_NAME);
    memcpy(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, filename, name_len);
    send_pkt.t_pkt.active_pkt.payload.metadata.nbytes = name_len;

    dfs_client_trace("DFS_CR -- client sending ack \n");
    int ret = nrf_send_noack(conn->client->nrf, conn->server_addr, (void *)&send_pkt, dfs_get_pkt_bytes());
    if (ret == -1) {
        dfs_client_trace("Error sending DFS_CR to server=%x\n", conn->server_addr);
        return -1;
    }

    dfs_client_trace("DFC_CR -- client waiting for reply \n");
    char *data = kmalloc(dfs_get_pkt_bytes());
    ret = nrf_read_exact_timeout(conn->client->nrf, data, dfs_get_pkt_bytes(), dfs_client_connect_timeout_us);
    if (ret == -1) {
        dfs_client_trace("DFC_CR -- Error receiving chunk metadata from server=%x\n", conn->server_addr);
        return -1;
    } else if (ret != dfs_get_pkt_bytes()) {
        dfs_client_trace("DFC_CR -- Received incomplete data\n");
        return -1;
    } 

    dfs_pkt *recv_pkt = (dfs_pkt *)data;
    if (recv_pkt->op_code != DFS_CR || recv_pkt->t_pkt.active_pkt.status == STATUS_FAILED) {
        return -1;
    }
    return recv_pkt->t_pkt.active_pkt.chunk_id_sqn;
}

uint32_t dfs_client_delete(dfs_connection_t *conn, const char* filename, uint32_t name_len) {
    assert(name_len <= 12);

    dfs_pkt send_pkt;
    send_pkt.op_code = DFS_RM;
    send_pkt.port_id = conn->port_id;
    memset(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, 0, MAX_FILE_NAME);
    memcpy(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, filename, name_len);
    send_pkt.t_pkt.active_pkt.payload.metadata.nbytes = name_len;

    dfs_client_trace("DFS_RM -- client sending ack \n");
    int ret = nrf_send_noack(conn->client->nrf, conn->server_addr, (void *)&send_pkt, dfs_get_pkt_bytes());
    if (ret == -1) {
        dfs_client_trace("Error sending DFS_CR to server=%x\n", conn->server_addr);
        return -1;
    }

    dfs_client_trace("DFS_RM -- client waiting for reply \n");
    char *data = kmalloc(dfs_get_pkt_bytes());
    ret = nrf_read_exact_timeout(conn->client->nrf, data, dfs_get_pkt_bytes(), dfs_client_connect_timeout_us);
    if (ret == -1) {
        dfs_client_trace("DFS_RM -- Error receiving chunk metadata from server=%x\n", conn->server_addr);
        return -1;
    } else if (ret != dfs_get_pkt_bytes()) {
        dfs_client_trace("DFS_RM -- Received incomplete data\n");
        return -1;
    } 

    dfs_pkt *recv_pkt = (dfs_pkt *)data;
    if (recv_pkt->op_code != DFS_RM || recv_pkt->t_pkt.active_pkt.status == STATUS_FAILED) {
        return -1;
    }
    return recv_pkt->t_pkt.active_pkt.chunk_id_sqn;
}

int dfs_client_change_directory(dfs_connection_t *conn, const char *dirname) {
    dfs_pkt send_pkt;
    send_pkt.op_code = DFS_CD;
    send_pkt.port_id = conn->port_id;
    // memset(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, 0, MAX_FILE_NAME);
    // memcpy(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, dir_name, name_len);
    memcpy(send_pkt.t_pkt.placeholder, dirname, strlen(dirname) + 1);

    dfs_client_trace("DFS_CD -- client sending ack \n");
    int ret = nrf_send_noack(conn->client->nrf, conn->server_addr, (void *)&send_pkt, dfs_get_pkt_bytes());
    if (ret == -1) {
        dfs_client_trace("Error sending DFS_CD to server=%x\n", conn->server_addr);
        return 0;
    }

    // wait for reply
    char* data = kmalloc(dfs_get_pkt_bytes());
    dfs_client_trace("DFS_CD -- client reading \n");
    ret = nrf_read_exact_timeout(conn->client->nrf, data, dfs_get_pkt_bytes(), dfs_client_connect_timeout_us);
    if (ret == -1) {
        dfs_client_trace("DFS_CD -- Error receiving reply from server=%x\n", conn->server_addr);
        return 0;
    } else if (ret != dfs_get_pkt_bytes()) {
        dfs_client_trace("DFS_CD -- Received incomplete data\n");
        return 0;
    } 
    
    dfs_pkt *recv_pkt = (dfs_pkt *)data;
    if (recv_pkt->op_code == DFS_CD) {
        if (recv_pkt->t_pkt.active_pkt.status == STATUS_SUCCESS)
            return 1;
        else 
            return 2;
    }

    return 0;
}

int dfs_client_listdir(dfs_connection_t *conn, const char *dirname) {
    dfs_pkt send_pkt;
    send_pkt.op_code = DFS_LS;
    send_pkt.port_id = conn->port_id;
    memcpy(send_pkt.t_pkt.placeholder, dirname, strlen(dirname) + 1);
    dfs_client_trace("DFS_LS -- client sending ack \n");
    int ret = nrf_send_noack(conn->client->nrf, conn->server_addr, (void *)&send_pkt, dfs_get_pkt_bytes());
    if (ret == -1) {
        dfs_client_trace("Error sending DFS_WR to server=%x\n", conn->server_addr);
        return 0;
    }

    // wait for reply
    char* data = kmalloc(dfs_get_pkt_bytes());
    dfs_client_trace("DFS_LS -- client waiting for listdir \n");
    // ret = nrf_read_exact_timeout(conn->client->nrf, data, dfs_get_pkt_bytes(), dfs_client_connect_timeout_us);
    // if (ret == -1) {
    //     dfs_client_trace("DFS_LS -- Error receiving chunk metadata from server=%x\n", conn->server_addr);
    //     return 0;
    // } else if (ret != dfs_get_pkt_bytes()) {
    //     dfs_client_trace("DFS_LS -- Received incomplete data\n");
    //     return 0;
    // } 
    
    dfs_pkt *recv_pkt = (dfs_pkt *)data;
    // char *ls_res;
    // if (recv_pkt->op_code == DFS_LS) {
    //     ls_res = kmalloc((MAX_FILE_NAME + 1) * sizeof(recv_pkt->t_pkt.active_pkt));
    // } else {
    //     dfs_client_trace("DFS_LS -- Received invalid data\n");
    //     return 0;
    // }

    do {
        ret = nrf_read_exact_timeout(conn->client->nrf, data, dfs_get_pkt_bytes(), dfs_client_connect_timeout_us);
        if (ret == -1) {
            dfs_client_trace("DFS_LS -- Error receiving chunk metadata from server=%x\n", conn->server_addr);
            return 0;
        } else if (ret != dfs_get_pkt_bytes()) {
            dfs_client_trace("DFS_LS -- Received incomplete data\n");
            return 0;
        } 
        
        dfs_pkt *recv_pkt = (dfs_pkt *)data;
        if (recv_pkt->op_code == DFS_LS) {
            _thread_safe_printk("%s", recv_pkt->t_pkt.placeholder);
        } else if (recv_pkt->op_code == DFS_END_LS) {
            _thread_safe_printk("%s", recv_pkt->t_pkt.placeholder);
            if (recv_pkt->t_pkt.active_pkt.status == STATUS_SUCCESS)
                return 1;
            else 
                return 2;
        }
    } while (1);
    return 0;
}

dfs_pkt* dfs_client_write_chunk_meta(dfs_connection_t *conn, const char *filename, uint32_t name_len, uint32_t chunk_sqn) {
    assert(name_len <= 12);
    
    dfs_pkt send_pkt;
    send_pkt.op_code = DFS_WR;
    send_pkt.port_id = conn->port_id;
    send_pkt.t_pkt.active_pkt.chunk_id_sqn = chunk_sqn;
    memset(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, 0, MAX_FILE_NAME);
    memcpy(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, filename, name_len);
    send_pkt.t_pkt.active_pkt.payload.metadata.nbytes = name_len;

    int ret = -1;
    do {
        ret = nrf_send_noack(conn->client->nrf, conn->server_addr, (void *)&send_pkt, dfs_get_pkt_bytes());

    } while (ret == -1);
    

    // wait for reply
    char* data = kmalloc(dfs_get_pkt_bytes());
    dfs_client_trace("DFS_WR -- client reading \n");
    ret = nrf_read_exact_timeout(conn->client->nrf, data, dfs_get_pkt_bytes(), dfs_client_connect_timeout_us);
    if (ret == -1) {
        dfs_client_trace("DFS_WR -- Error receiving chunk metadata from server=%x\n", conn->server_addr);
        return NULL;
    } else if (ret != dfs_get_pkt_bytes()) {
        dfs_client_trace("DFS_WR -- Received incomplete data\n");
        return NULL;
    } 
    
    dfs_pkt *recv_pkt = (dfs_pkt *)data;
    if (recv_pkt->op_code != DFS_WR || recv_pkt->t_pkt.active_pkt.status == STATUS_FAILED) {
        dfs_client_trace("DFS_WR -- Status failed or op_code not DFS_WR\n");
        return NULL;
    }
    if (recv_pkt->op_code == DFS_WR) {
        dfs_client_trace("DFS_WR -- Successfully received data\n");
        return recv_pkt;
    }

    return recv_pkt;
}

int dfs_client_write_chunk_data(dfs_connection_t *conn, uint32_t chunk_id, char *chunk_data, uint32_t nbytes) {
    // TODO: support writing from non-zero offset
    uint32_t bytes_sent = 0;
    uint32_t pkt_nbytes = 0;
    while (bytes_sent < nbytes) {
        dfs_pkt send_pkt;
        send_pkt.op_code = DFS_WR;
        send_pkt.port_id = conn->port_id;
        send_pkt.t_pkt.active_pkt.chunk_id_sqn = chunk_id;
        send_pkt.t_pkt.active_pkt.payload.data.offset = bytes_sent;
        pkt_nbytes = CHUNK_SERVER_PKT_SIZE;
        if (nbytes - bytes_sent <= CHUNK_SERVER_PKT_SIZE) {
            pkt_nbytes = nbytes - bytes_sent;
            send_pkt.op_code = DFS_END_WR;
        }
        memset(send_pkt.t_pkt.active_pkt.payload.data.data, 0, CHUNK_SERVER_PKT_SIZE);
        memcpy(send_pkt.t_pkt.active_pkt.payload.data.data, chunk_data + bytes_sent, pkt_nbytes);
        int ret = nrf_send_noack(conn->client->nrf, conn->server_addr, (void *)&send_pkt, dfs_get_pkt_bytes());
        if (ret == -1) {
            dfs_client_trace("CLIENT WR -- Error sending DFS_WR to chunkserver=%x\n", conn->server_addr);
            return -1;
        } else if (ret != dfs_get_pkt_bytes()) {
            dfs_client_trace("CLIENT WR -- Sent incomplete data, aborting\n");
            return -1;
        }
        bytes_sent += CHUNK_SERVER_PKT_SIZE;
    }

    dfs_client_trace("Client done writing -- waiting for END_WR ... \n");
    
    char* data = kmalloc(dfs_get_pkt_bytes());
    int ret;
    do {
        ret = nrf_read_exact_timeout(conn->client->nrf, data, dfs_get_pkt_bytes(), dfs_client_connect_timeout_us);
        if (ret != dfs_get_pkt_bytes()) {
            dfs_client_trace("Received incomplete data\n");
            return -1;
        }
        dfs_pkt *recv_pkt = (dfs_pkt *)data;
        if (recv_pkt->op_code == DFS_END_WR) {
            dfs_client_trace("Client received END_WR ... \n");
            return 0;
        }
    } while (1);
    

    return 0;
}

dfs_pkt* dfs_client_read_chunk_meta(dfs_connection_t *conn, const char* filename, uint32_t name_len, uint32_t chunk_sqn) {

    dfs_pkt send_pkt;
    send_pkt.op_code = DFS_RD;
    send_pkt.port_id = conn->port_id;
    send_pkt.t_pkt.active_pkt.chunk_id_sqn = chunk_sqn;
    
    assert(name_len <= 12);
    memset(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, 0, MAX_FILE_NAME);
    memcpy(send_pkt.t_pkt.active_pkt.payload.metadata.file_name, filename, name_len);
    send_pkt.t_pkt.active_pkt.payload.metadata.nbytes = name_len;

    dfs_client_trace("DFS_RD -- client sending ack \n");
    int ret = nrf_send_noack(conn->client->nrf, conn->server_addr, (void *)&send_pkt, dfs_get_pkt_bytes());
    if (ret == -1) {
        dfs_client_trace("Error sending DFS_CNCT to server=%x\n", conn->server_addr);
        return NULL;
    }

    // wait for reply
    char* data = kmalloc(dfs_get_pkt_bytes());
    dfs_client_trace("client reading \n");
    ret = nrf_read_exact_timeout(conn->client->nrf, data, dfs_get_pkt_bytes(), dfs_client_connect_timeout_us);
    if (ret == -1) {
        dfs_client_trace("Error receiving chunk metadata from server=%x\n", conn->server_addr);
        return NULL;
    } else if (ret != dfs_get_pkt_bytes()) {
        dfs_client_trace("Received incomplete data\n");
        return NULL;
    } 

    dfs_pkt *recv_pkt = (dfs_pkt *)data;
    if (recv_pkt->op_code != DFS_RD || recv_pkt->t_pkt.active_pkt.status == STATUS_FAILED) {
        return NULL;
    }

    return recv_pkt;
}

// dfs_client_query_chunk_data(conn, chunkserver_addr, chunk_id, chunk_data);

int dfs_client_query_chunk_data(dfs_connection_t *conn, uint32_t chunk_id, char* chunk_data) {
    dfs_pkt send_pkt;
    send_pkt.op_code = DFS_RD;
    send_pkt.port_id = conn->port_id;
    send_pkt.t_pkt.active_pkt.chunk_id_sqn = chunk_id;
    send_pkt.t_pkt.active_pkt.payload.dataquery.offset = 0;
    send_pkt.t_pkt.active_pkt.payload.dataquery.nbytes = CHUNK_SIZE;

    int ret = nrf_send_noack(conn->client->nrf, conn->server_addr, (void *)&send_pkt, dfs_get_pkt_bytes());
    if (ret == -1) {
        dfs_client_trace("Error sending DFS_CNCT to chunkserver=%x\n", conn->server_addr);
        return -1;
    } 

    uint32_t offset = 0;
    uint32_t cnt = 0;

    do {
        char data[dfs_get_pkt_bytes()];
        ret = nrf_read_exact_timeout(conn->client->nrf, data, dfs_get_pkt_bytes(), dfs_client_connect_timeout_us);
        if (ret == -1) {
            dfs_client_trace("client receive from chunk id = %x, chunk server %x time out \n", chunk_id, conn->server_addr);
            return -1;
        }
        dfs_pkt *recv_pkt = (dfs_pkt *)data;
        if (recv_pkt->t_pkt.active_pkt.status == STATUS_FAILED) {
            dfs_client_trace("STATUS_FAILED \n");
            return -1;
        }
        
        if (recv_pkt->op_code == DFS_RD) {
            offset = recv_pkt->t_pkt.active_pkt.payload.data.offset;
            memcpy(chunk_data+cnt, recv_pkt->t_pkt.active_pkt.payload.data.data, CHUNK_SERVER_PKT_SIZE);
            cnt += CHUNK_SERVER_PKT_SIZE;
            dfs_client_trace("write -- offset %d, cnt %d \n", offset, cnt);
            // if (offset != cnt) return -1;
        }

        if (recv_pkt->op_code == DFS_END_RD) {
            memcpy(chunk_data+cnt, recv_pkt->t_pkt.active_pkt.payload.data.data, CHUNK_SERVER_PKT_SIZE);
            dfs_client_trace("received data from chunk server %x: %s\n", chunk_data);
            return 0;
        }
    } while (1);
    
    return -1;
}