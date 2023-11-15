#include "dfs-client-command.h"
#include "rpi.h"
#include "preemptive-thread.h"
#include "thread-safe-io.h"

static char *s;

void parse_input(void *arg) {
    dfs_client_conns_t *conns = arg;
    char input[1000];
    memset(input, '\0', 1000);

    _thread_safe_printk("(base) 140E@OS %s ", s);
    _thread_safe_scank("%s", input);
    _thread_safe_printk("\n");

    // unsigned idx = 0;
    // _thread_safe_printk("\n");
    // while (input[idx] != '\0') {
    //     _thread_safe_printk("%d ", input[idx]);
    //     ++idx;
    // }
    // _thread_safe_printk("\n");
    // idx = 0;
    // while (input[idx] != '\0') {
    //     _thread_safe_printk("%c ", input[idx]);
    //     ++idx;
    // }
    // _thread_safe_printk("\n");

    client_parse(input, conns);
}

void client_shell_start(void *arg) {
    dfs_client_init_t *client_init = arg;
    dfs_client_t *client = client_init->client;
    dfs_init_client(client, client_init->config);

    _thread_safe_printk("Client initiation done, trying to connect to server ...\n");

    dfs_connection_t *master_conn = kmalloc(sizeof(dfs_connection_t));
    master_conn->client = client;
    master_conn->server_addr = dfs_master_addr;
    if (dfs_client_try_connect(master_conn) == -1) {
        panic("Failed to connect to server");
    }

    _thread_safe_printk("Successfully connected to server at address %x \n", master_conn->server_addr);

    dfs_connection_t *chunk_conn = kmalloc(sizeof(dfs_connection_t));
    chunk_conn->client = client;

    dfs_client_conns_t *client_conns = kmalloc(sizeof(dfs_client_conns_t));
    client_conns->chunk_conn = chunk_conn;
    client_conns->master_conn = master_conn;

    s = kmalloc(100);
    memset(s, 0, 100);
    
    pre_fork(parse_input, (void *)(client_conns), 1, 1);
}


int client_parse(char *input, dfs_client_conns_t *client_conns){
    dfs_connection_t *master_conn = client_conns->master_conn; 
    dfs_connection_t *chunk_conn = client_conns->chunk_conn;

    const char *touch = "touch";
    const char *rm = "rm";
    const char *mkdir = "mkdir";
    const char *rd = "cat";
    const char *wr = "echo";
    const char *cd = "cd";
    const char *ls = "ls";
    const char *quit = "q";
    // const char *empty = "";

    // char *arg2 = strchr(input, ' ');
    // if (arg2 != NULL) {
    //     arg2 = arg2 + 1;
    // } else {
    //     arg2 = kmalloc(1);
    //     memcpy(arg2, empty, 1);
    // }
    char *arg2 = strchr(input, ' ') + 1;
    if (strncmp(input, touch, strlen(touch)) == 0 || strncmp(input, mkdir, strlen(mkdir)) == 0) {

        _thread_safe_printk("Received client CREATE request on %s, trying to handle ...\n", arg2);
        dfs_client_create(master_conn, arg2, strlen(arg2));

    } else if (strncmp(input, rm, strlen(rm)) == 0) {

        _thread_safe_printk("Received client DELETE request on %s, trying to handle ...\n", arg2);
        dfs_client_delete(master_conn, arg2, strlen(arg2));

    } else if (strncmp(input, rd, strlen(rd)) == 0) {

        _thread_safe_printk("Received client READ request on %s, trying to handle ...\n", arg2);
        dfs_pkt* pkt = dfs_client_read_chunk_meta(master_conn, arg2, strlen(arg2), 0);
        if (!pkt) {
            _thread_safe_printk("Client failed to request file meta data from master server\n");
            goto CONTINUE;
        };
        if (pkt->t_pkt.active_pkt.payload.metadata.nbytes == 0) {
            printk("\n");
            goto CONTINUE;
        }
        // _thread_safe_printk("Client received file meta data from master, requesting from chunk server...\n");

        chunk_conn->server_addr = pkt->t_pkt.active_pkt.payload.metadata.chunk_server_addr;
        uint32_t chunk_id = pkt->t_pkt.active_pkt.chunk_id_sqn;
        // _thread_safe_printk("trying to connect chunk server addr %x chunk id %d\n", pkt->t_pkt.active_pkt.payload.metadata.chunk_server_addr, chunk_id);
        if (dfs_client_try_connect(chunk_conn) == -1) {
            _thread_safe_printk("Client failed to connect to chunk server");
            goto CONTINUE;
        }

        char data[CHUNK_SIZE];
        if (dfs_client_query_chunk_data(chunk_conn, chunk_id, data) == -1) {
            _thread_safe_printk("Client failed to request file data from chunk server");
            goto CONTINUE;
        }

        printk("%s\n",data);

    } else if (strncmp(input, wr, strlen(wr)) == 0) {

        char *arg2 = strchr(input, '"') + 1;
        char *arg3 = strchr(arg2, '"');
        if (arg3 - arg2 > 128) {
            _thread_safe_printk("INPUT TOO LARGE, LIMIT WITHIN 128 CHARACTERS\n");
            goto CONTINUE;
        }
        char data[128];
        memset(data, 0, arg3 - arg2 + 1);
        memcpy(data, arg2, arg3 - arg2);
        char *filename = strchr(arg3, '>') + 1;
        if (filename == NULL) {
            _thread_safe_printk("ill formatted command \n");
            goto CONTINUE;
        }
        filename = strchr(filename, '>') + 1;
        if (filename == NULL) {
            _thread_safe_printk("ill formatted command \n");
            goto CONTINUE;
        }
        filename = strchr(filename, ' ') + 1;
        if (filename == NULL) {
            _thread_safe_printk("ill formatted command \n");
            goto CONTINUE;
        }
        _thread_safe_printk("Received client WRITE request on file [%s] input [%s], trying to handle ...\n", filename, data);

        dfs_pkt *wr_pkt = dfs_client_write_chunk_meta(master_conn, filename, strlen(filename), 0);
        if (!wr_pkt) {
            panic("Client failed to request file meta data from master server\n");
        }
        // _thread_safe_printk("Client received file meta data from master, requesting from chunk server...\n");

        chunk_conn->server_addr = wr_pkt->t_pkt.active_pkt.payload.metadata.chunk_server_addr;

        // trace("trying to connect chunk server addr %x\n", wr_pkt->t_pkt.active_pkt.payload.metadata.chunk_server_addr);
        if (dfs_client_try_connect(chunk_conn) == -1) {
            panic("Client failed to connect to chunk server \n");
        }

        uint32_t chunk_id = wr_pkt->t_pkt.active_pkt.chunk_id_sqn;
        // _thread_safe_printk("Waiting for client to input write data to chunk id %d ...\n", chunk_id);
        if (dfs_client_write_chunk_data(chunk_conn, chunk_id, data, strlen(data)) == -1) {
            panic("write to chunk failed");
        }

    } else if (strncmp(input, ls, strlen(ls)) == 0) {
        _thread_safe_printk("Received client LS request on %s, trying to handle ...\n", arg2);

        int res = dfs_client_listdir(master_conn, arg2);
        if (res == 0) {
            _thread_safe_printk("Network failure, please retry!\n");
        }
        else if (res == 2) {
            // _thread_safe_printk("No such directory!\n");
        }
    } else if (strncmp(input, cd, strlen(cd)) == 0) {
        _thread_safe_printk("Received client CD request on %s, trying to handle ...\n", arg2);

        int res = dfs_client_change_directory(master_conn, arg2);
        if (res == 0) {
            _thread_safe_printk("Network failure, please retry!\n");
        }
        else if (res == 2) {
            // _thread_safe_printk("No such directory!\n");
        }
    } else if (strcmp(input, quit) == 0) {
        return 1;
    } else {
        _thread_safe_printk("Failed to recognize any input \n Currently only support: \n \t mkdir: create dir\n\t cat: show file content\n\t touch: create file\n\t >>: Write\n\t ls: List \n\tcd: Change Directory\n\n");
    }
    CONTINUE:
        pre_fork(parse_input, (void *)(client_conns), 1, 1);
    return 1;
}