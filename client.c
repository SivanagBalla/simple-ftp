#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "vars.h"
#include "utils.h"

enum USER_CMD {
    USER_INVALID = -1,
    USER_LS = 0,
    USER_PWD,
    USER_CD,
    USER_CDUP,
    USER_RENAME,
    USER_PUT,
    USER_GET,
    USER_USER,
    USER_PASS,
    USER_TYPE,
    USER_BYE,
    USER_MKD,
    USER_DELE,
    USER_RNFR,
    USER_RNTO,
    USER_RMD,
    USER_LCD,
    USER_LLS,
    USER_LPWD,
    USER_HELP,
    USER_QUIT,
    USER_EXIT,
    USER_Q,
    USER_SIZE,
    USER_COUNT
};

struct ftp_cmd USER_CMD_LIST[USER_COUNT] = {
    {"LS", USER_LS},
    {"PWD", USER_PWD},
    {"CD", USER_CD},
    {"CDUP", USER_CDUP},
    {"RENAME", USER_RENAME},
    {"PUT", USER_PUT},
    {"GET", USER_GET},
    {"USER", USER_USER},
    {"PASS", USER_PASS},
    {"TYPE", USER_TYPE},
    {"BYE", USER_BYE},
    {"MKD", USER_MKD},
    {"DELE", USER_DELE},
    {"RNFR", USER_RNFR},
    {"RNTO", USER_RNTO},
    {"RMD", USER_RMD},
    {"LCD", USER_LCD},
    {"LLS", USER_LLS},
    {"LPWD", USER_LPWD},
    {"SIZE", USER_SIZE},
    {"QUIT", USER_QUIT},
    {"EXIT", USER_EXIT},
    {"Q", USER_Q},
    {"HELP", USER_HELP}
};

enum USER_CMD parse_input_cmd(char* buf, int len) {
    int i, j;
    for (i=0; i<sizeof(USER_CMD_LIST)/sizeof(USER_CMD_LIST[0]); i++) {
        for(j=0; USER_CMD_LIST[i].name[j] && j < len; j++) {
            if (USER_CMD_LIST[i].name[j] != buf[j] & 0x1f && USER_CMD_LIST[i].name[j] != buf[j]- 32) 
                break;
        }
        if (USER_CMD_LIST[i].name[j] == '\0' && (buf[j]==' ' || buf[j]==0))
            return USER_CMD_LIST[i].cmd;
    }
    return USER_INVALID;
}

enum CLIENT_STATE {
    ST_NONE,
    ST_SIZE,
    ST_PASVLIST,
    ST_PASVLIST2,
    ST_PASVGET,
    ST_PASVGET2,
    ST_PASVPUT,
    ST_PASVPUT2
};

int running = 1;

void ouch() {
    running = 0;
}

int main(int argc, char *argv[]) {
    int server_port = LISTEN_PORT;
    int non_interactive = 0;
    int cmd_pos = 3;
    char cmd_line[BUF_SIZE] = {0};

    if (argc < 2) {
        printf("usage: %s <addr> [2121]\n", argv[0]);
        exit(0);
    }

    if (argc >= 3) {
        server_port = atoi(argv[2]);
        if ( server_port == 0 ) {
            enum USER_CMD cmd = parse_input_cmd(argv[2], strlen(argv[2]));
            if (cmd == USER_GET || cmd == USER_PUT) {
                non_interactive = 1;
                cmd_pos = 2;
                server_port = LISTEN_PORT;
            } else {
                err(1, "invalid port: %d", server_port);
                exit(1);
            }
        }
    }

    if ((cmd_pos == 2 && argc == 4) ||
        (cmd_pos == 3 && argc == 5)) {
        enum USER_CMD cmd = parse_input_cmd(argv[cmd_pos], strlen(argv[cmd_pos]));
        switch(cmd) {
            case USER_GET:
            case USER_PUT:
                non_interactive = 1;
                strcpy(cmd_line, argv[cmd_pos]);
                cmd_line[3] = ' ';
                strcpy(cmd_line + 4, argv[cmd_pos+1]);
                break;
            default:
                break;
        }
    }
    int client = new_client(ntohl(inet_addr(argv[1])), server_port);
    if (client < 0) {
        err(1, "can not connect to %s %d", argv[1], server_port);
        err(1, "exit ...");
        exit(1);
    }
    int i, n;
    char buf[BUF_SIZE+1];
    char tmpbuf[BUF_SIZE+1];
    char cmdbuf[BUF_SIZE+1];
    int data_client = -1;
    struct sockaddr_in data_client_addr;
    uint32_t addr;
    uint16_t port;
    char path[BUF_SIZE];
    int code = -1;
    enum CLIENT_STATE state = ST_NONE;
    char filename[BUF_SIZE], line[BUF_SIZE];
    int filesize = 0;

    while ((n=recv(client, buf, sizeof(buf), MSG_PEEK)) > 0) {
        if (!running) break;
        for (i=0; i<n; i++) {
            if (buf[i] == '\n') break;
        }
        if (buf[i] != '\n') {
            err(1, "no line break found");
            break;
        }
        n = recv(client, buf, i+1, 0);
        buf[n] = 0;
        printf("%s", buf);
        fflush(stdout);
        parse_number(buf, &code);
        if (code < RPL_ERR_UNKWNCMD && state != ST_NONE) {
            switch(state) {
                case ST_PASVLIST:
                case ST_PASVGET:
                case ST_PASVPUT:
                    if (code == RPL_PASVOK) {
                        strcpy(tmpbuf, buf);
                        tmpbuf[0] = tmpbuf[1] = tmpbuf[2] = tmpbuf[3] = ' ';
                        parse_addr_port(tmpbuf, &addr, &port);
                        switch(state) {
                            case ST_PASVLIST:
                                send_str(client, "LIST\r\n");
                                break;
                            case ST_PASVGET:
                                send_str(client, "RETR %s\r\n", filename);
                                break;
                            case ST_PASVPUT:
                                send_str(client, "STOR %s\r\n", filename);
                                break;
                        }
                        data_client = new_client(addr, port);
                        state++;
                    } else {
                        err(1, " get unknown response, %d", code);
                        state = ST_NONE;
                    }
                    break;
                case ST_SIZE:
                    if (code == RPL_FILEST) {
                        strcpy(tmpbuf, buf);
                        tmpbuf[0] = tmpbuf[1] = tmpbuf[2] = tmpbuf[3] = ' ';
                        parse_number(tmpbuf, &filesize);
                        info(1, "filesize :%d", filesize);

                        send_str(client, "PASV\r\n");
                        state = ST_PASVGET;
                    } else {
                        err(1, "unknown response");
                        state = ST_NONE;
                    }
                    break;
                case ST_PASVLIST2:
                case ST_PASVGET2:
                case ST_PASVPUT2:
                    if (data_client < 0) {
                        err(1, "data client not created");
                    } else {
                        if (state == ST_PASVLIST2) {
                            recv_file(data_client, stdout, filesize);
                        } else if (state == ST_PASVGET2) {
                            recv_path(data_client, filename, 0, filesize);
                        } else if (state == ST_PASVPUT2) {
                            FILE *f = fopen(filename, "rb");
                            if (f) {
                                send_file(data_client, f, 1);
                                fclose(f);
                            } else {
                                err(1, "err open file '%s'", filename);
                            }
                        }
                        info(1, "closing data socket ... %d", close(data_client));
                        data_client = -1;
                        state = ST_NONE;
                    }
                    break;
                default:
                    state = ST_NONE;
                    break;
            }
            if (code < RPL_ERR_UNKWNCMD)
                continue;
        }
        if (code >= RPL_ERR_UNKWNCMD) state = ST_NONE;

        int valid = 0;
        while (!valid) {
            valid = 1;
            printf("ftp > ");
            if (non_interactive) {
                printf("%s\n", cmd_line);
                strcpy(line, cmd_line);
                strcpy(cmd_line, "exit");
            } else if (!fgets(line, BUF_SIZE, stdin)){
                running = 0;
                break;
            }
            int len = strlen(line);
            len --;
            while (line[len] == '\n' || line[len] == '\r') len--;
            len ++;
            line[len] = 0;
            if (strnlen(line, BUF_SIZE) == 0) {
                valid = 0;
                continue;
            }
            enum USER_CMD cmd = parse_input_cmd(line, len);
            switch(cmd) {
                case USER_USER:
                case USER_PASS:
                case USER_TYPE:
                case USER_MKD:
                case USER_DELE:
                case USER_RNFR:
                case USER_RNTO:
                case USER_RMD:
                    send_str(client, "%s\r\n", line);
                    break;
                case USER_LS:
                    send_str(client, "PASV\r\n");
                    state = ST_PASVLIST;
                    break;
                case USER_CD:
                    send_str(client, "CWD %s\r\n", &line[3]);
                    break;
                case USER_SIZE:
                    send_str(client, "SIZE %s\r\n", &line[5]);
                    state = ST_SIZE;
                    break;
                case USER_PWD:
                    send_str(client, "PWD\r\n");
                    break;
                case USER_CDUP:
                    send_str(client, "CDUP\r\n");
                    break;
                case USER_HELP:
                    for (i=0; i<sizeof(USER_CMD_LIST)/sizeof(USER_CMD_LIST[0]); i++) {
                        printf("%s\n", USER_CMD_LIST[i].name);
                    }
                    valid = 0;
                    break;
                case USER_BYE:
                    send_str(client, "QUIT\r\n");
                    running = 0;
                    break;
                case USER_LCD:
                    chdir(&line[4]);
                    valid = 0;
                    break;
                case USER_LLS:
                    getcwd(path, sizeof(path));
                    printf("%s\n", path);

                    sprintf(cmdbuf, "ls -l %s", path);
                    FILE *p2 = popen(cmdbuf, "r");
                    int n;
                    while ((n=fread(tmpbuf, 1, BUF_SIZE, p2)) > 0 ) {
                        fwrite(tmpbuf, 1, n, stdout);
                    }
                    pclose(p2);

                    valid = 0;
                    break;
                case USER_LPWD:
                    getcwd(path, sizeof(path));
                    printf("%s\n", path);
                    valid = 0;
                    break;
                case USER_GET:
                    strcpy(filename, &line[4]);
                    filesize = 0;
                    send_str(client, "SIZE %s\r\n", filename);
                    state = ST_SIZE;
                    break;
                case USER_PUT:
                    send_str(client, "PASV\r\n");
                    strcpy(filename, &line[4]);
                    state = ST_PASVPUT;
                    break;
                case USER_Q:
                case USER_QUIT:
                case USER_EXIT:
                    ouch();
                    break;
                default:
                    warn(1, "unknown user cmd");
                    valid = 0;
                    break;
            }
        }
        if (!running) break;
    }
    int st = close(client);
    info(1, "FTP client close socket ... %d", st);
    info(1, "FTP client shutdown");
    if (data_client > 0) {
        st = close(data_client);
        info(1, "FTP client close data socket ... %d", st);
        info(1, "FTP client data socket shutdown");
    }
    return 0;
}

