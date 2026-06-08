#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <portaudio.h>
#include <opus/opus.h>
#include "ui.h"

#define SERVER_IP "192.168.1.20"
#define PORT 5000
#define SAMPLE_RATE 48000
#define FRAME_SIZE 960
#define MAX_PACKET 4000
#define BUFFER_CAPACITY 5 // Size of Jitter Buffer (20ms)

typedef struct {
    int16_t data[BUFFER_CAPACITY][FRAME_SIZE];
    int write_idx;
    int read_idx;
    int count;
    pthread_mutex_t lock;
} JitterBuffer;

JitterBuffer jb = { .write_idx = 0, .read_idx = 0, .count = 0, .lock = PTHREAD_MUTEX_INITIALIZER };

int audio_enabled = 1; 
int sock, tcp_sock;
struct sockaddr_in server_addr;
OpusEncoder *enc;
OpusDecoder *dec;

PaStream *in_stream, *out_stream;

void* network_receiver_thread(void* arg){
    unsigned char incoming_opus[MAX_PACKET];
    int16_t decoded_pcm[FRAME_SIZE];

    while (1) {
        ssize_t n = recv(sock, incoming_opus, MAX_PACKET, MSG_DONTWAIT);

        if (n > 0) {
            int samples = opus_decode(dec, incoming_opus, n, decoded_pcm, FRAME_SIZE, 0);
            if (samples > 0) {
                // Put data in JitterBuffer
                pthread_mutex_lock(&jb.lock);
                if (jb.count < BUFFER_CAPACITY) {
                    memcpy(jb.data[jb.write_idx], decoded_pcm, samples * sizeof(int16_t));
                    jb.write_idx = (jb.write_idx + 1) % BUFFER_CAPACITY;
                    jb.count++;
                }
                pthread_mutex_unlock(&jb.lock);
            }
        } else {
            usleep(1000);
        }
    }
    return NULL;
}

void* audio_output_thread(void* arg) {
    int16_t play_pcm[FRAME_SIZE];

    while (1) {
        int has_data = 0;
        
        //Work with JitterBuffer
        pthread_mutex_lock(&jb.lock);
        if (jb.count > 0) {
            memcpy(play_pcm, jb.data[jb.read_idx], FRAME_SIZE * sizeof(int16_t));
            jb.read_idx = (jb.read_idx + 1) % BUFFER_CAPACITY;
            jb.count--;
            has_data = 1;
        }
        pthread_mutex_unlock(&jb.lock);

        if (has_data) {
            Pa_WriteStream(out_stream, play_pcm, FRAME_SIZE);
        } else {
            memset(play_pcm, 0, FRAME_SIZE * sizeof(int16_t));
            Pa_WriteStream(out_stream, play_pcm, FRAME_SIZE);
        }
    }
    return NULL;
}

void* mic_sender_thread(void* arg){
    int16_t mic_pcm[FRAME_SIZE];
    unsigned char opus_send[MAX_PACKET];
    while (1) {
        PaError err = Pa_ReadStream(in_stream, mic_pcm, FRAME_SIZE);

        if (err == paNoError){
            if (audio_enabled) {
                int bytes = opus_encode(enc, mic_pcm, FRAME_SIZE, opus_send, MAX_PACKET);
                sendto(sock, opus_send, bytes, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
            }
        } else {
            usleep(1000);
        }
    }
    return NULL;
}

void* chat_receiver_thread(void* arg){
    char buffer[1024];
    while(1) {
        int n = recv(tcp_sock, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            add_msg("friend", buffer); // Add to UI
        }
    }
    return NULL;
}
    
int main() {
    int err;
    
    // 1. Opus Setup
    enc = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &err);
    dec = opus_decoder_create(SAMPLE_RATE, 1, &err);

    // 2. Socket Setup
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    connect(tcp_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // 3. PortAudio Setup 
    Pa_Initialize();
    Pa_OpenDefaultStream(&in_stream, 1, 0, paInt16, SAMPLE_RATE, FRAME_SIZE, NULL, NULL);
    Pa_OpenDefaultStream(&out_stream, 0, 1, paInt16, SAMPLE_RATE, FRAME_SIZE, NULL, NULL);
    Pa_StartStream(in_stream);
    Pa_StartStream(out_stream);
    
    // 3. UI Setup
    initscr(); noecho(); curs_set(0); timeout(50); start_color(); use_default_colors();
    init_pair(1, COLOR_GREEN, -1);
    init_pair(2, COLOR_CYAN, -1);
    init_pair(3, 8, -1); // Grey
    init_pair(4, COLOR_BLACK, COLOR_GREEN); // Status Active
    init_pair(5, COLOR_MAGENTA, -1);
    init_pair(6, COLOR_WHITE, COLOR_RED);   // Status Muted

    int h, w; getmaxyx(stdscr, h, w);
    int sidebar_w = 20;
    WINDOW *voice = newwin(5, w - sidebar_w, 0, 0);
    WINDOW *users = newwin(h - 1, sidebar_w, 0, w - sidebar_w);
    WINDOW *chat  = newwin(h - 6, w - sidebar_w, 5, 0);
    WINDOW *input = newwin(1, w, h - 1, 0);

    //Threads
    pthread_t tid_net, tid_out, tid_mic, tid_chat;
    pthread_create(&tid_net, NULL, network_receiver_thread, NULL);
    pthread_create(&tid_out, NULL, audio_output_thread, NULL);
    pthread_create(&tid_chat, NULL, chat_receiver_thread, NULL);
    pthread_create(&tid_mic, NULL, mic_sender_thread, NULL);

    char input_buf[256] = {0}; int input_ptr = 0;

    while (1) {
        draw_wave(voice, w - sidebar_w, 5);
        
        // Sidebar
        werase(users);
        wattron(users, COLOR_PAIR(1)); mvwprintw(users, 1, 2, "● ONLINE"); wattroff(users, COLOR_PAIR(1));
        //wattron(users, COLOR_PAIR(3)); mvwprintw(users, 3, 2, "me"); mvwprintw(users, 4, 2, "friend"); wattroff(users, COLOR_PAIR(3));
        wrefresh(users);

        // Chat
        werase(chat);
        mvwhline(chat, 0, 0, ACS_HLINE, w - sidebar_w);
        for (int i = 0; i < chat_count; i++) {
            mvwprintw(chat, i + 1, 2, "%s", chat_history[i]);
        }
        wrefresh(chat);

        // Input
        werase(input);
        wbkgd(input, audio_enabled ? COLOR_PAIR(4) : COLOR_PAIR(6));
        mvwprintw(input, 0, 1, " > %s", input_buf);
        wrefresh(input);

        int ch = getch();
        if (ch == 27) break;
        if (ch == '\t') audio_enabled = !audio_enabled;
        if (ch == '\n' && input_ptr > 0) {
            send(tcp_sock, input_buf, strlen(input_buf), 0);
            add_msg("me", input_buf);
            memset(input_buf, 0, 256); input_ptr = 0;
        } else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && input_ptr > 0) {
            input_buf[--input_ptr] = '\0';
        } else if (ch >= 32 && ch <= 126 && input_ptr < 200) {
            input_buf[input_ptr++] = (char)ch;
        }
    }

    endwin();
    return 0;
}
