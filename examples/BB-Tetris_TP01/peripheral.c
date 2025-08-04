#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include "bbstate.h"
#include <bluetooth/hci.h>     // For HCI functions
#include <bluetooth/hci_lib.h> // For HCI functions
#include "tropic_simple.h"
#include <ncurses.h>
#include <sys/time.h>

/* L2CAP server channel - must match client configuration */
#define L2CAP_SERVER_PORT_NUM 0x0235

#define ROWS 20
#define COLS 15
#define TRUE 1
#define FALSE 0

// Pre-shared cryptographic keys for secure handshake
static uint8_t private_key[32] = {
    0x60, 0x58, 0xce, 0x93, 0x0d, 0xf3, 0xdd, 0x2e, 0xa0, 0x0e, 0x1a,
    0x62, 0x6b, 0xc7, 0x0b, 0xa4, 0x17, 0x74, 0xea, 0x11, 0xe9, 0xa2,
    0xef, 0x28, 0x7e, 0xd2, 0x5a, 0x59, 0xca, 0x04, 0x03, 0x6b};

static uint8_t public_key[32] = {
    0x49, 0x5a, 0xfa, 0xd7, 0xaf, 0x39, 0xae, 0x9f, 0x18, 0xdb, 0x1a,
    0x69, 0xd6, 0x88, 0xe3, 0xd2, 0x4a, 0xa1, 0xec, 0xa5, 0x49, 0xac,
    0x95, 0xc0, 0x46, 0x80, 0x3d, 0x22, 0x03, 0x59, 0x65, 0x73};

// Remote device's public key - must match central device's public key
static uint8_t remote_public_key[32] = {
    0x76, 0x8a, 0x58, 0x2e, 0x1f, 0x44, 0x21, 0xc9, 0xb7, 0x55, 0xf3,
    0x70, 0xdf, 0xe9, 0x44, 0x98, 0x5f, 0x31, 0xe9, 0x54, 0x77, 0x7e,
    0xb9, 0xba, 0xd6, 0x3d, 0xa0, 0xec, 0xf7, 0x4f, 0x6f, 0x61};

// Global state for the secure communication session
static bbstate state;

// Tetris game structures and variables
typedef struct {
    char **array;
    int width, row, col;
} Shape;

char Table[ROWS][COLS] = {0};
int score = 0;
char GameOn = TRUE;
suseconds_t timer = 400000;
int decrease = 1000;
Shape current;

const Shape ShapesArray[7]= {
    {(char *[]){(char []){0,1,1},(char []){1,1,0}, (char []){0,0,0}}, 3},                           //S shape     
    {(char *[]){(char []){1,1,0},(char []){0,1,1}, (char []){0,0,0}}, 3},                           //Z shape     
    {(char *[]){(char []){0,1,0},(char []){1,1,1}, (char []){0,0,0}}, 3},                           //T shape     
    {(char *[]){(char []){0,0,1},(char []){1,1,1}, (char []){0,0,0}}, 3},                           //L shape     
    {(char *[]){(char []){1,0,0},(char []){1,1,1}, (char []){0,0,0}}, 3},                           //flipped L shape    
    {(char *[]){(char []){1,1},(char []){1,1}}, 2},                                                 //square shape
    {(char *[]){(char []){0,0,0,0}, (char []){1,1,1,1}, (char []){0,0,0,0}, (char []){0,0,0,0}}, 4} //long bar shape
};

// BB Protocol random number generator context
typedef struct {
    int socket;
    bbstate* state;
} tetris_rng_ctx;

static tetris_rng_ctx g_rng_ctx;

/**
 * Print buffer contents in hexadecimal format
 */
void print_buf(void* buf, size_t buf_len) {
    uint8_t* bufr = (uint8_t*)buf;
    for (int i = 0; i < buf_len; i++) {
        printf("%02x", bufr[i]);
    }
    printf("\n");
}

/**
 * Get random bytes from central device using BB protocol
 * Returns 0 on success, -1 on failure
 */
static int get_random_bytes_from_central(tetris_rng_ctx* ctx, uint8_t* out, uint8_t count) {
    struct tropic_message msg = {0};
    struct tropic_response resp = {0};
    uint8_t encrypted_msg[2048];
    uint8_t decrypted_resp[2048];
    
    snprintf(msg.command, sizeof(msg.command), "random %d", count);
    msg.data_len = 0;
    
    // Encrypt and send using BB protocol
    size_t enc_len = aead_encrypt(encrypted_msg, ctx->state->key, ctx->state->counter++,
                                 NULL, 0, (uint8_t*)&msg, sizeof(msg));
    if (send(ctx->socket, encrypted_msg, enc_len, 0) <= 0) return -1;
    
    // Receive and decrypt using BB protocol
    ssize_t recv_len = recv(ctx->socket, encrypted_msg, sizeof(encrypted_msg), 0);
    if (recv_len <= 0) return -1;
    
    size_t dec_len = aead_decrypt(decrypted_resp, ctx->state->key, ctx->state->counter++,
                                 NULL, 0, encrypted_msg, recv_len);
    if (dec_len < sizeof(resp)) return -1;
    
    memcpy(&resp, decrypted_resp, sizeof(resp));
    if (resp.data_len < count) return -1;
    
    memcpy(out, resp.data, count);
    return 0;
}

// Tetris game functions
Shape CopyShape(Shape shape) {
    Shape new_shape = shape;
    char **copyshape = shape.array;
    new_shape.array = (char**)malloc(new_shape.width*sizeof(char*));
    int i, j;
    for(i = 0; i < new_shape.width; i++) {
        new_shape.array[i] = (char*)malloc(new_shape.width*sizeof(char));
        for(j=0; j < new_shape.width; j++) {
            new_shape.array[i][j] = copyshape[i][j];
        }
    }
    return new_shape;
}

void DeleteShape(Shape shape) {
    int i;
    for(i = 0; i < shape.width; i++) {
        free(shape.array[i]);
    }
    free(shape.array);
}

int CheckPosition(Shape shape) {
    char **array = shape.array;
    int i, j;
    for(i = 0; i < shape.width; i++) {
        for(j = 0; j < shape.width; j++) {
            if((shape.col+j < 0 || shape.col+j >= COLS || shape.row+i >= ROWS)) {
                if(array[i][j])
                    return FALSE;
            }
            else if(Table[shape.row+i][shape.col+j] && array[i][j])
                return FALSE;
        }
    }
    return TRUE;
}

void SetNewRandomShape() {
    uint8_t rbytes[2];
    
    // Strictly use BB protocol for randomness - no fallback
    if (get_random_bytes_from_central(&g_rng_ctx, rbytes, 2) != 0) {
        printf("Failed to get random bytes from central device\n");
        GameOn = FALSE;
        return;
    }
    
    Shape new_shape = CopyShape(ShapesArray[rbytes[0] % 7]);
    new_shape.col = rbytes[1] % (COLS-new_shape.width+1);
    new_shape.row = 0;
    DeleteShape(current);
    current = new_shape;
    if(!CheckPosition(current)) {
        GameOn = FALSE;
    }
}

void RotateShape(Shape shape) {
    Shape temp = CopyShape(shape);
    int i, j, k, width;
    width = shape.width;
    for(i = 0; i < width; i++) {
        for(j = 0, k = width-1; j < width; j++, k--) {
            shape.array[i][j] = temp.array[k][i];
        }
    }
    DeleteShape(temp);
}

void WriteToTable() {
    int i, j;
    for(i = 0; i < current.width; i++) {
        for(j = 0; j < current.width; j++) {
            if(current.array[i][j])
                Table[current.row+i][current.col+j] = current.array[i][j];
        }
    }
}

void RemoveFullRowsAndUpdateScore() {
    int i, j, sum, count=0;
    for(i=0; i<ROWS; i++) {
        sum = 0;
        for(j=0; j<COLS; j++) {
            sum+=Table[i][j];
        }
        if(sum==COLS) {
            count++;
            int l, k;
            for(k = i; k >=1; k--)
                for(l=0; l<COLS; l++)
                    Table[k][l]=Table[k-1][l];
            for(l=0; l<COLS; l++)
                Table[k][l]=0;
            timer-=decrease--;
        }
    }
    score += 100*count;
}

void PrintTable() {
    char Buffer[ROWS][COLS] = {0};
    int i, j;
    for(i = 0; i < current.width; i++) {
        for(j = 0; j < current.width; j++) {
            if(current.array[i][j])
                Buffer[current.row+i][current.col+j] = current.array[i][j];
        }
    }
    clear();
    for(i=0; i<COLS-9; i++)
        printw(" ");
    printw("TROPIC01 Tetris\n");
    for(i = 0; i < ROWS; i++) {
        for(j = 0; j < COLS; j++) {
            printw("%c ", (Table[i][j] + Buffer[i][j])? '#': '.');
        }
        printw("\n");
    }
    printw("\nScore: %d\n", score);
}

void ManipulateCurrent(int action) {
    Shape temp = CopyShape(current);
    switch(action) {
        case 's':
            temp.row++;
            if(CheckPosition(temp))
                current.row++;
            else {
                WriteToTable();
                RemoveFullRowsAndUpdateScore();
                SetNewRandomShape();
            }
            break;
        case 'd':
            temp.col++;
            if(CheckPosition(temp))
                current.col++;
            break;
        case 'a':
            temp.col--;
            if(CheckPosition(temp))
                current.col--;
            break;
        case 'w':
            RotateShape(temp);
            if(CheckPosition(temp))
                RotateShape(current);
            break;
    }
    DeleteShape(temp);
    PrintTable();
}

struct timeval before_now, now;
int hasToUpdate() {
    return ((suseconds_t)(now.tv_sec*1000000 + now.tv_usec) -((suseconds_t)before_now.tv_sec*1000000 + before_now.tv_usec)) > timer;
}

/**
 * Main function - implements Bluetooth L2CAP server for TROPIC01 Tetris
 */
int main(int argc, char** argv) {
    struct sockaddr_l2 loc_addr = {0}, rem_addr = {0};
    uint8_t buffer[128] = {0};
    int server_socket, client_socket, bytes_read;
    unsigned int opt = sizeof(rem_addr);

    bdaddr_t local_bdaddr = {0};
    int dev_id = 0;

    printf("Starting TROPIC01 Tetris with BB Protocol...\n");

    // Get the local Bluetooth address of hci0 adapter
    int hci_sock = hci_open_dev(dev_id);
    if (hci_sock < 0) {
        perror("failed to open HCI device hci0");
        exit(1);
    }

    if (hci_read_bd_addr(hci_sock, &local_bdaddr, 0) < 0) {
        perror("failed to read local Bluetooth address for hci0 device");
        close(hci_sock);
        exit(1);
    }

    char local_addr_str[18];
    ba2str(&local_bdaddr, local_addr_str);
    printf("Using local HCI device: hci%d (%s)\n", dev_id, local_addr_str);

    // Enable device discoverability
    uint8_t param = (SCAN_PAGE | SCAN_INQUIRY);
    if (hci_send_cmd(hci_sock, OGF_HOST_CTL, OCF_WRITE_SCAN_ENABLE, 1, &param) < 0) {
        perror("Failed to send HCI_Write_Scan_Enable command");
        return -1;
    }
    close(hci_sock);

    // Create L2CAP socket for server
    server_socket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (server_socket < 0) {
        perror("failed to create socket");
        exit(1);
    }

    // Bind socket to local Bluetooth adapter and specified port
    loc_addr.l2_family = AF_BLUETOOTH;
    bacpy(&loc_addr.l2_bdaddr, &local_bdaddr);
    loc_addr.l2_psm = htobs(L2CAP_SERVER_PORT_NUM);

    printf("Binding to Bluetooth address...\n");
    if (bind(server_socket, (struct sockaddr*)&loc_addr, sizeof(loc_addr)) < 0) {
        perror("failed to bind");
        close(server_socket);
        exit(1);
    }

    printf("Listening for central device...\n");
    listen(server_socket, 2);

    // Accept connection from central device
    client_socket = accept(server_socket, (struct sockaddr*)&rem_addr, &opt);
    if (client_socket < 0) {
        perror("accept");
        close(server_socket);
        return client_socket;
    }

    // Display connected client information
    char buf[18];
    ba2str(&rem_addr.l2_bdaddr, buf);
    printf("Connected from %s\n", buf);

    // Initialize secure session state as peripheral device
    bbstate_init(&state, BB_ROLE_PERIPHERAL, public_key, private_key,
                 remote_public_key, NULL);

    // Receive handshake initiation from central device
    bytes_read = recv(client_socket, buffer, sizeof(struct bb_session_start_req), 0);
    if (bytes_read <= 0) {
        perror("Error handshake initiation\n");
        close(client_socket);
        close(server_socket);
        exit(1);
    }

    // Process the received handshake request
    bb_session_start_rx(&state, buffer);

    // Prepare handshake response
    bb_session_start_rsp(&state, buffer);

    // Send handshake response to complete key exchange
    int rc = send(client_socket, buffer, sizeof(struct bb_session_start_req), 0);
    if (rc <= 0) {
        perror("Error sending");
        close(client_socket);
        close(server_socket);
        exit(1);
    }

    // Handshake complete
    printf("BB Protocol handshake complete\n");
    printf("Session key: ");
    print_buf(state.key, 32);

    // Set up RNG context for Tetris
    g_rng_ctx.socket = client_socket;
    g_rng_ctx.state = &state;

    // Initialize Tetris game
    printf("Starting Tetris with TROPIC01 random numbers...\n");
    score = 0;
    int c;
    initscr();
    gettimeofday(&before_now, NULL);
    timeout(1);
    SetNewRandomShape();
    PrintTable();
    
    // Main Tetris game loop
    while(GameOn) {
        if ((c = getch()) != ERR) {
            ManipulateCurrent(c);
        }
        gettimeofday(&now, NULL);
        if (hasToUpdate()) {
            ManipulateCurrent('s');
            gettimeofday(&before_now, NULL);
        }
    }
    
    // Game over
    DeleteShape(current);
    endwin();
    
    // Display final game state
    int i, j;
    for(i = 0; i < ROWS; i++) {
        for(j = 0; j < COLS; j++) {
            printf("%c ", Table[i][j] ? '#': '.');
        }
        printf("\n");
    }
    printf("\nGame Over!\n");
    printf("Final Score: %d\n", score);

    // Cleanup connections
    close(client_socket);
    close(server_socket);
    return 0;
}