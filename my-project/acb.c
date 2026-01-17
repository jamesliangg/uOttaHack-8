#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>  // Optional for TUI; fallback printf

#define DB_FILE "acb.db"

typedef struct {
    char symbol[16];
    int shares;
    double total_cost;
} Symbol;

int init_db(sqlite3 *db) {
    char *sql = "CREATE TABLE IF NOT EXISTS transactions ("
                "id INTEGER PRIMARY KEY, symbol TEXT, type TEXT, qty REAL, price REAL, date TEXT);";
    char *err = 0;
    return sqlite3_exec(db, sql, 0, 0, &err) == SQLITE_OK;
}

double calc_acb(sqlite3 *db, const char *symbol, int *shares_out) {
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT SUM(qty) as tot_shares, SUM(qty * price) as tot_cost FROM transactions WHERE symbol='%s';", symbol);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0.0;
    double acb_share = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        double tot_shares = sqlite3_column_double(stmt, 0);
        double tot_cost = sqlite3_column_double(stmt, 1);
        *shares_out = (int)tot_shares;
        acb_share = tot_shares > 0 ? tot_cost / tot_shares : 0.0;
    }
    sqlite3_finalize(stmt);
    return acb_share;
}

int add_tx(sqlite3 *db, const char *symbol, const char *type, double qty, double price) {
    char sql[256];
    snprintf(sql, sizeof(sql), "INSERT INTO transactions (symbol, type, qty, price, date) VALUES ('%s', '%s', %f, %f, datetime('now'));", symbol, type, qty, price);
    char *err = 0;
    return sqlite3_exec(db, sql, 0, 0, &err) == SQLITE_OK;
}

int main() {
    sqlite3 *db;
    if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
        fprintf(stderr, "DB open fail: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    init_db(db);

    // TUI loop (simplified)
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE);
    printw("ACB Tracker (q=quit, a=add, l=list, d=delete all)\n");
    refresh();

    char cmd;
    while ((cmd = getch()) != 'q') {
        if (cmd == 'a') {
            char sym[16], typ[8]; double qty, price;
            printw("\nSymbol: "); refresh(); echo(); getnstr(sym, 15); noecho();
            printw("Type (b=buy, s=sell, r=roc): "); refresh();
            char type_key = getch();
            if (type_key == 'b') strcpy(typ, "buy");
            else if (type_key == 's') strcpy(typ, "sell");
            else if (type_key == 'r') strcpy(typ, "roc");
            else strcpy(typ, "unknown");
            printw("\nQty: "); refresh(); echo(); scanw("%lf", &qty);
            printw("Price: "); refresh(); scanw("%lf", &price); noecho();
            add_tx(db, sym, typ, qty, price);
            printw("Added!\n");
        } else if (cmd == 'l') {
            // List all symbols' ACB (query each unique)
            char *sql = "SELECT DISTINCT symbol FROM transactions;";
            sqlite3_stmt *stmt;
            sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
            printw("\nSymbol | Shares | ACB/Share\n");
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *sym = (const char*)sqlite3_column_text(stmt, 0);
                int shares; double acb = calc_acb(db, sym, &shares);
                printw("%s | %d | %.2f\n", sym, shares, acb);
            }
            sqlite3_finalize(stmt);
        } else if (cmd == 'd') {
            printw("\nClear database? (y/n): "); refresh();
            char confirm = getch();
            if (confirm == 'y') {
                sqlite3_exec(db, "DELETE FROM transactions;", 0, 0, 0);
                printw("Database cleared!\n");
            } else {
                printw("Cancelled.\n");
            }
        }
        refresh();
    }
    endwin();
    sqlite3_close(db);
    return 0;
}
