#ifndef UI_H
#define UI_H

#define _XOPEN_SOURCE_SOURCE 700
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_MSG_HIST 20
char chat_history[MAX_MSG_HIST][256];
int chat_count = 0;
extern int audio_enabled;

char *levels[] = {" ", " ", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

void add_msg(const char *name, const char *text) {
    time_t rawtime;
    struct tm *timeinfo;
    char time_str[10];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_str, 10, "%H:%M", timeinfo);

    char formatted[256];
    snprintf(formatted, 255, "[%s] %s: %s", time_str, name, text);

    if (chat_count < MAX_MSG_HIST) {
        strcpy(chat_history[chat_count++], formatted);
    } else {
        for (int i = 0; i < MAX_MSG_HIST - 1; i++) strcpy(chat_history[i], chat_history[i+1]);
        strcpy(chat_history[MAX_MSG_HIST - 1], formatted);
    }
}

void draw_wave(WINDOW *win, int width, int height) {
    werase(win);
    wattron(win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(win, 0, 1, audio_enabled ? " ⚡ VOICE_ACTIVE " : " ⚡ MUTED (TAB) ");
    wattroff(win, COLOR_PAIR(1) | A_BOLD);

    for (int x = 1; x < width - 2; x++) {
        int level = rand() % 9;
        wattron(win, COLOR_PAIR(2));
        mvwaddstr(win, height / 2, x, levels[level]);
        wattroff(win, COLOR_PAIR(2));
    }
    wrefresh(win);
}

#endif
