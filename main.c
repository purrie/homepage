#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "main.h"


typedef struct {
    char alias[STRING_CAPACITY];
    char link[STRING_CAPACITY];
    char hotkey[STRING_CAPACITY];
} Entry;

typedef struct {
    Entry** entries;
    char label[STRING_CAPACITY];
    size_t len;
    size_t cap;
} Database;

typedef struct {
    Database** bases;
    size_t len;
    size_t cap;
} Databases;

struct Node {
    Char value;
    Entry* entry;
    struct Node* parent;
    struct Node** subnodes;
    size_t len;
};

typedef struct Node Node;

void write_new_line(FILE* file) {
    if(fputs("\n", file) < 0) {
        fprintf(stderr, "ERROR: Failed to write string");
        fclose(file);
        exit(1);
    }
}

void write_text(FILE* file, char* str) {
    if(fputs(str, file) < 0) {
        fprintf(stderr, "ERROR: Failed to write string");
        fclose(file);
        exit(1);
    }
}

void write_line(FILE* file, char* str) {
    write_text(file, str);
    write_new_line(file);
}

int is_valid_link(char* link) {
    if (strlen(link) < 9)
        return 0;
    char test[5] = {0};
    memcpy(test, link, 4);
    if (strcmp(test, "http") != 0)
        return 0;

    return 1;
}

void process_database(Database * db) {
    char command[STRING_CAPACITY] = {0};

    {
        // check if database is marked for homepage
        strcpy(command, "bookkeeper -o -a homepage_mark -d ");
        strcat(command, db->label);
        FILE* mark = popen(command, "r");
        if (mark == NULL) {
            fprintf(stderr, "ERROR: could not open bookkeeper\n");
            return;
        }

        // zero return means we have a homepage mark
        int status = pclose(mark);
        if (errno) {
            fprintf(stderr, "WARNING: database %s is not marked and error is %s %d\n", db->label, strerror(errno), status);
            return;
        }
    }

    {
        // iterate over marked database and get all aliased fields
        strcpy(command, "bookkeeper -C -d ");
        strcat(command, db->label);
        FILE* aliases = popen(command, "r");
        if (aliases == NULL) {
            return;
        }

        // iterate over aliased fields and get all fields that are http links
        db->cap = 20;
        db->entries = malloc(sizeof(Entry*) * 20);
        memset(db->entries, 0, sizeof(Entry*) * 20);

        char alias[STRING_CAPACITY] = {0};
        while (fgets(alias, STRING_CAPACITY, aliases) != NULL) {
            size_t len = strlen(alias);
            if (alias[len-1] == '\n')
                alias[len-1] = '\0';
            if (strcmp(alias, "homepage_mark") == 0) {
                continue;
            }
            Entry* entry = malloc(sizeof(Entry));
            memset(entry, 0, sizeof(Entry));
            strcpy(entry->alias, alias);

            if(db->len == db->cap) {
                db->cap *= 2;
                Entry** new = malloc(sizeof(Entry*) * db->cap);
                memcpy(new, db->entries, sizeof(Entry*) * db->len);
                memset(new + db->len, 0, sizeof(Entry*) * db->len);
                free(db->entries);
                db->entries = new;
            }
            db->entries[db->len] = entry;
            db->len ++;
        }
        pclose(aliases);
    }

    for (size_t i = 0; i < db->len; i++) {
        strcpy(command, "bookkeeper -o -a ");
        strcat(command, db->entries[i]->alias);
        strcat(command, " -d ");
        strcat(command, db->label);

        FILE* get_value = popen(command, "r");
        if (get_value == NULL) {
            fprintf(stderr, "WARNING: failed to read alias %s\n", db->entries[i]->alias);
            goto remove;
        }
        char value[STRING_CAPACITY] = {0};

        if (fgets(value, STRING_CAPACITY, get_value) != NULL) {
            size_t len = strlen(value);
            if (value[len-1] == '\n')
                value[len-1] = '\0';

            if(is_valid_link(value)) {
                strcpy(db->entries[i]->link, value);
            } else {
                fprintf(stderr,
                        "WARNING: Alias '%s' holds data that isn't a valid link: %s\n",
                        db->entries[i]->alias, value);
                goto remove;
            }
        }

        pclose(get_value);
        continue;

        remove:
        free(db->entries[i]);
        for (size_t r = i; r < db->len; r++) {
            db->entries[r] = db->entries[r+1];
        }
        db->len --;
        i --;
        pclose(get_value);
    }

    if(db->len == 0) {
        free(db->entries);
        db->entries = NULL;
        db->cap = 0;
    }
}

void grow_subnodes(Node* node, size_t amount) {
    assert(amount > 0);
    Node** new = malloc(sizeof(Node*) * (node->len + amount));
    if (node->len) {
        memcpy(new, node->subnodes, sizeof(Node*) * node->len);
        memset(new + node->len, 0, sizeof(Node*) * amount);
        free(node->subnodes);
        node->subnodes = new;
    } else {
        memset(new, 0, sizeof(Node*) * (node->len + amount));
        node->subnodes = new;
    }
    node->len += amount;
}

Node* create_node(Node* parent, Entry* entry, Char value) {
    Node *node = malloc(sizeof(Node));
    memset(node, 0, sizeof(Node));
    node->entry = entry;
    node->parent = parent;
    node->value = value;
    return node;
}

void assign_entry_to_node(Node* node, Entry* entry, size_t level) {
    if (node->len) {
        Char new_mark = get_character_at(entry->alias, level + 1);
        for(int i = 0; i < node->len; i++) {
            Node* subnode = node->subnodes[i];
            if(new_mark.unicode == subnode->value.unicode) {
                assign_entry_to_node(subnode, entry, level + 1);
                return;
            }
        }
        grow_subnodes(node, 1);
        node->subnodes[node->len - 1] = create_node(node, entry, new_mark);

    } else {

        Char resident_mark = get_character_at(node->entry->alias, level + 1);
        // TODO getting the mark can fail
        Node* resident_node = create_node(node, node->entry, resident_mark);
        node->entry = NULL;

        Char new_mark = get_character_at(entry->alias, level + 1);
        // TODO getting the mark can fail

        if(new_mark.unicode == resident_mark.unicode) {
            grow_subnodes(node, 1);
            node->subnodes[0] = resident_node;
            assign_entry_to_node(resident_node, entry, level + 1);
        } else {
            grow_subnodes(node, 2);
            node->subnodes[0] = resident_node;
            node->subnodes[1] = create_node(node, entry, new_mark);
        }
    }
}

void assign_hotkeys_to_entries(Node* node) {
    if(node->len) {
        for(int i = 0; i < node->len; i++) {
            assign_hotkeys_to_entries(node->subnodes[i]);
        }
        free(node->subnodes);

    } else {
        Node *chain[100] = {0};
        size_t len = 0;
        Node *ptr = node;
        while (ptr != NULL) {
            chain[len] = ptr;
            ptr = ptr->parent;
            len++;
        }
        while (len --> 0) {
            char_cat(node->entry->hotkey, chain[len]->value);
        }
    }
    free(node);
}

void assign_hotkeys(Databases dbs) {
    Node tree = {0};

    for(int i = 0; i < dbs.len; i++) {
        Database* db = dbs.bases[i];
        for(int d = 0; d < db->len; d++) {
            Entry* entry = db->entries[d];
            Char start = get_character_at(entry->alias, 0);
            char exists = 0;
            for(int e = 0; e < tree.len; e++) {
                if (tree.subnodes[e]->value.unicode == start.unicode) {
                    assign_entry_to_node(tree.subnodes[e], entry, 0);
                    exists = 1;
                    break;
                }
            }
            if (exists == 0) {
                grow_subnodes(&tree, 1);
                tree.subnodes[tree.len - 1] = create_node(&tree, entry, start);
            }
        }
    }

    for(int i = 0; i < tree.len; i++) {
        assign_hotkeys_to_entries(tree.subnodes[i]);
    }
}

void build_elements(Databases dbs, FILE* index) {
    for(int i = 0; i < dbs.len; i++) {
        Database* db = dbs.bases[i];
        write_line(index, "    <div class=\"panel\">");
        write_line(index, "      <div class=\"header\">");
        write_text(index, "        ");
        write_text(index, db->label);
        write_new_line(index);
        write_line(index, "      </div>");
        for(int e = 0; e < db->len; e++) {
            Entry* ent = db->entries[e];
            write_line(index, "      <div class=\"element\">");
            write_text(index, "        <div class=\"hint\" text=\"");
            write_text(index, ent->hotkey);
            write_text(index, "\">\n");
            write_text(index, "          ");
            write_text(index, ent->hotkey);
            write_new_line(index);
            write_text(index, "        </div>");
            write_new_line(index);

            write_text(index, "        <a class=\"bookmark\" href=\"");
            write_text(index, ent->link);
            write_text(index, "\">");
            write_text(index, ent->alias);
            write_text(index, "</a>\n");
            write_line(index, "      </div>");
            free(ent);
        }
        write_line(index, "    </div>");
        free(db);
    }
    if (dbs.len)
        free(dbs.bases);
}

void build_containers(FILE* index) {
    FILE* dbs = popen("bookkeeper -D", "r");
    if (dbs == NULL) {
        fclose(index);
        fprintf(stderr, "ERROR: %s", strerror(errno));
        exit(1);
    }
    Databases container = {0};
    container.bases = malloc(sizeof(Database*) * 20);
    container.cap = 20;
    memset(container.bases, 0, sizeof(Database*) * 20);

    char name[STRING_CAPACITY] = {0};
    while (fgets(name, STRING_CAPACITY, dbs) != NULL) {
        size_t len = strlen(name);
        if (name[len-1] == '\n')
            name[len-1] = '\0';

        Database * db = malloc(sizeof(Database));
        if (db == NULL) {
            fprintf(stderr, "ERROR: Failed to allocate memory for a database");
            exit(1);
        }
        memset(db, 0, sizeof(Database));
        strcpy(db->label, name);

        if (container.len == container.cap) {
            container.cap *= 2;
            Database **grow = malloc(sizeof(Database*) * container.cap);
            memcpy(grow, container.bases, sizeof(Database*) * container.len);
            memset(grow + container.len, 0, sizeof(Database*) * container.len);
            free(container.bases);
            container.bases = grow;
        }
        container.bases[container.len] = db;
        container.len ++;
    }
    pclose(dbs);

    for (size_t i = 0; i < container.len; i++) {
        process_database(container.bases[i]);
        if (container.bases[i]->len == 0) {
            free(container.bases[i]);
            for(size_t r = i; r < container.len; r++) {
                container.bases[r] = container.bases[r+1];
            }
            container.len --;
            i --;
        }
    }

    // Postprocess database results to assign hotkeys
    assign_hotkeys(container);
    build_elements(container, index);
}

void build_hopepage() {
    /* FILE* index = fopen("index.html", "w"); */
    /* if (index == NULL) { */
    /*     fprintf(stderr, "ERROR: %s", strerror(errno)); */
    /*     exit(1); */
    /* } */
    FILE* index = stdout;
    write_line(index, "<!doctype html>");
    write_line(index, "<html>");
    write_line(index, "  <head>");
    write_line(index, "    <meta charset=\"utf-8\">");
    write_line(index, "    <title>Homepage</title>");
    write_line(index, "    <link rel=\"stylesheet\" href=\"styles.css\">");
    write_line(index, "    <script src=\"code.js\"></script>");
    write_line(index, "  </head>");
    write_line(index, "  <body onload=\"on_load()\">");
    write_line(index, "    <div id=\"time\" class=\"clock\"></div>");
    write_line(index, "    <div id=\"preview\"></div>");
    build_containers(index);
    write_line(index, "  </body>");
    write_line(index, "</html>");
    /* fclose(index); */
}


int main(int argn, char** argv) {
    build_hopepage();
    return 0;
}
