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

Database* process_database(char* name) {
    // check if database is marked for homepage
    char command[STRING_CAPACITY] = {0};
    strcpy(command, "bookkeeper -o -a homepage_mark -d ");
    strcat(command, name);
    FILE* mark = popen(command, "r");
    if (mark == NULL) {
        return NULL;
    }
    // zero return means we have a homepage mark
    if (pclose(mark)) {
        return NULL;
    }

    // iterate over marked database and get all aliased fields
    strcpy(command, "bookkeeper -C -d ");
    strcat(command, name);
    FILE* aliases = popen(command, "r");
    if (aliases == NULL) {
        return NULL;
    }

    // iterate over aliased fields and get all fields that are http links
    char alias[STRING_CAPACITY] = {0};
    Database* result = malloc(sizeof(Database));
    memset(result, 0, sizeof(Database));
    strcpy(result->label, name);
    result->cap = 20;
    result->entries = malloc(sizeof(Entry*) * 20);
    memset(result->entries, 0, sizeof(Entry*) * 20);

    while (fgets(alias, STRING_CAPACITY - 1, aliases) != NULL) {
        // add aliases and links to the index file
        strcpy(command, "bookkeeper -o -a ");
        strcat(command, alias);
        strcat(command, " -d ");
        strcat(command, name);
        FILE* get_value = popen(command, "r");
        if (get_value == NULL) {
            continue;
        }
        char value[STRING_CAPACITY] = {0};

        if (fgets(value, STRING_CAPACITY - 1, get_value) != NULL) {
            if(is_valid_link(value)) {
                Entry* entry = malloc(sizeof(Entry));
                memset(entry, 0, sizeof(Entry));
                strcpy(entry->link, value);
                strcpy(entry->alias, alias);
                if(result->len == result->cap) {
                    result->cap *= 2;
                    Entry** new = malloc(sizeof(Entry*) * result->cap);
                    memcpy(new, result->entries, sizeof(Entry*) * result->len);
                    memset(new + result->len, 0, sizeof(Entry*) * result->len);
                    free(result->entries);
                    result->entries = new;
                }
                result->entries[result->len] = entry;
                result->len ++;
            }
        }
        pclose(get_value);
    }

    pclose(aliases);
    if(result->len) {
        return result;
    } else {
        free(result->entries);
        free(result);
        return NULL;
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
        while (len-- > 0) {
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

            write_text(index, "      <a class=\"bookmark\" href=\"");
            write_text(index, ent->link);
            write_text(index, "\">");
            write_text(index, ent->alias);
            write_text(index, "</a>\n");
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
    fgets(name, STRING_CAPACITY - 1, dbs);
    while (name[0] != '\0') {
        Database *ptr = process_database(name);
        if(ptr == NULL) {
            fprintf(stderr, "WARNING: Failed to process database %s", name);
        } else {
            if (container.len == container.cap) {
                container.cap *= 2;
                Database **grow = malloc(sizeof(Database*) * container.cap);
                memcpy(grow, container.bases, sizeof(Database*) * container.len);
                memset(grow + container.len, 0, sizeof(Database*) * container.len);
                free(container.bases);
                container.bases = grow;
            }
            container.bases[container.len] = ptr;
            container.len ++;
        }
        fgets(name, STRING_CAPACITY - 1, dbs);
    }
    pclose(dbs);
    // Postprocess database results to assign hotkeys
    assign_hotkeys(container);
    build_elements(container, index);

    // cleanup
    for(int i = 0; i < container.len; i++) {
        Database* db = container.bases[i];
        for(int j = 0; j < db->len; j++) {
            free(db->entries[j]);
        }
        free(db);
    }
    free(container.bases);
}

void build_hopepage() {
    FILE* index = fopen("index.html", "w");
    if (index == NULL) {
        fprintf(stderr, "ERROR: %s", strerror(errno));
        exit(1);
    }
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
    fclose(index);
}


int main(int argn, char** argv) {
    build_hopepage();
    return 0;
}
