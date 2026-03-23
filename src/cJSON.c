/**
 * @file cJSON.c
 * @brief Minimal cJSON implementation for Magic Debugger
 */

#include "cJSON.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <ctype.h>

/* Hooks */
static void *(*g_malloc)(size_t) = malloc;
static void (*g_free)(void *) = free;
static void *(*g_realloc)(void *, size_t) = realloc;

/* Hooks setup */
void cJSON_InitHooks(cJSON_Hooks *hooks) {
    if (hooks == NULL) {
        g_malloc = malloc;
        g_free = free;
        g_realloc = realloc;
    } else {
        g_malloc = hooks->malloc_fn;
        g_free = hooks->free_fn;
        /* Use standard realloc if no realloc hook */
        g_realloc = realloc;
    }
}

/* Create items */
cJSON *cJSON_CreateObject(void) {
    cJSON *item = (cJSON*)g_malloc(sizeof(cJSON));
    if (item) {
        memset(item, 0, sizeof(cJSON));
        item->type = cJSON_Object;
    }
    return item;
}

cJSON *cJSON_CreateArray(void) {
    cJSON *item = (cJSON*)g_malloc(sizeof(cJSON));
    if (item) {
        memset(item, 0, sizeof(cJSON));
        item->type = cJSON_Array;
    }
    return item;
}

cJSON *cJSON_CreateNull(void) {
    cJSON *item = (cJSON*)g_malloc(sizeof(cJSON));
    if (item) {
        memset(item, 0, sizeof(cJSON));
        item->type = cJSON_Null;
    }
    return item;
}

cJSON *cJSON_CreateTrue(void) {
    cJSON *item = (cJSON*)g_malloc(sizeof(cJSON));
    if (item) {
        memset(item, 0, sizeof(cJSON));
        item->type = cJSON_True;
    }
    return item;
}

cJSON *cJSON_CreateFalse(void) {
    cJSON *item = (cJSON*)g_malloc(sizeof(cJSON));
    if (item) {
        memset(item, 0, sizeof(cJSON));
        item->type = cJSON_False;
    }
    return item;
}

cJSON *cJSON_CreateBool(cJSON_bool b) {
    cJSON *item = (cJSON*)g_malloc(sizeof(cJSON));
    if (item) {
        memset(item, 0, sizeof(cJSON));
        item->type = b ? cJSON_True : cJSON_False;
    }
    return item;
}

cJSON *cJSON_CreateNumber(double num) {
    cJSON *item = (cJSON*)g_malloc(sizeof(cJSON));
    if (item) {
        memset(item, 0, sizeof(cJSON));
        item->type = cJSON_Number;
        item->valuedouble = num;
        item->valueint = (int)num;
    }
    return item;
}

cJSON *cJSON_CreateString(const char *str) {
    if (str == NULL) return NULL;
    cJSON *item = (cJSON*)g_malloc(sizeof(cJSON));
    if (item) {
        memset(item, 0, sizeof(cJSON));
        item->type = cJSON_String;
        item->valuestring = (char*)g_malloc(strlen(str) + 1);
        if (item->valuestring) {
            strcpy(item->valuestring, str);
        } else {
            g_free(item);
            return NULL;
        }
    }
    return item;
}

/* Delete */
void cJSON_Delete(cJSON *item) {
    if (item == NULL) return;
    
    cJSON *child = item->child;
    while (child) {
        cJSON *next = child->next;
        cJSON_Delete(child);
        child = next;
    }
    
    if (item->type == cJSON_String || item->type == cJSON_Raw) {
        g_free(item->valuestring);
    }
    g_free(item->string);
    g_free(item);
}

/* Add items */
void cJSON_AddItemToArray(cJSON *array, cJSON *item) {
    if (array == NULL || item == NULL) return;
    
    item->prev = item->next = NULL;
    
    if (array->child == NULL) {
        array->child = item;
    } else {
        cJSON *child = array->child;
        while (child->next) child = child->next;
        child->next = item;
        item->prev = child;
    }
}

void cJSON_AddItemToObject(cJSON *object, const char *name, cJSON *item) {
    if (object == NULL || name == NULL || item == NULL) return;
    
    g_free(item->string);
    item->string = (char*)g_malloc(strlen(name) + 1);
    if (item->string) {
        strcpy(item->string, name);
    }
    cJSON_AddItemToArray(object, item);
}

void cJSON_AddItemToObjectCS(cJSON *object, const char *name, cJSON *item) {
    cJSON_AddItemToObject(object, name, item);
}

/* Get items */
int cJSON_GetArraySize(const cJSON *array) {
    if (array == NULL) return 0;
    int size = 0;
    cJSON *child = array->child;
    while (child) {
        size++;
        child = child->next;
    }
    return size;
}

cJSON *cJSON_GetArrayItem(const cJSON *array, int index) {
    if (array == NULL || index < 0) return NULL;
    cJSON *child = array->child;
    for (int i = 0; i < index && child; i++) {
        child = child->next;
    }
    return child;
}

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *name) {
    if (object == NULL || name == NULL) return NULL;
    cJSON *child = object->child;
    while (child && child->string && strcmp(child->string, name) != 0) {
        child = child->next;
    }
    return child;
}

cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *object, const char *name) {
    return cJSON_GetObjectItem(object, name);  /* Simplified */
}

/* Type checks */
cJSON_bool cJSON_IsInvalid(const cJSON *item) { return item && item->type == cJSON_Invalid; }
cJSON_bool cJSON_IsFalse(const cJSON *item) { return item && (item->type & cJSON_False); }
cJSON_bool cJSON_IsTrue(const cJSON *item) { return item && (item->type & cJSON_True); }
cJSON_bool cJSON_IsBool(const cJSON *item) { return item && (item->type & (cJSON_True | cJSON_False)); }
cJSON_bool cJSON_IsNull(const cJSON *item) { return item && item->type == cJSON_Null; }
cJSON_bool cJSON_IsNumber(const cJSON *item) { return item && item->type == cJSON_Number; }
cJSON_bool cJSON_IsString(const cJSON *item) { return item && item->type == cJSON_String; }
cJSON_bool cJSON_IsArray(const cJSON *item) { return item && item->type == cJSON_Array; }
cJSON_bool cJSON_IsObject(const cJSON *item) { return item && item->type == cJSON_Object; }
cJSON_bool cJSON_IsRaw(const cJSON *item) { return item && item->type == cJSON_Raw; }

/* Duplicate */
cJSON *cJSON_Duplicate(const cJSON *item, cJSON_bool recurse) {
    if (item == NULL) return NULL;
    
    cJSON *new_item = (cJSON*)g_malloc(sizeof(cJSON));
    if (new_item == NULL) return NULL;
    
    memcpy(new_item, item, sizeof(cJSON));
    new_item->next = new_item->prev = NULL;
    new_item->child = NULL;
    new_item->string = NULL;
    
    if (item->type == cJSON_String || item->type == cJSON_Raw) {
        if (item->valuestring) {
            new_item->valuestring = (char*)g_malloc(strlen(item->valuestring) + 1);
            if (new_item->valuestring == NULL) {
                g_free(new_item);
                return NULL;
            }
            strcpy(new_item->valuestring, item->valuestring);
        }
    }
    
    if (item->string) {
        new_item->string = (char*)g_malloc(strlen(item->string) + 1);
        if (new_item->string == NULL) {
            g_free(new_item->valuestring);
            g_free(new_item);
            return NULL;
        }
        strcpy(new_item->string, item->string);
    }
    
    if (recurse && item->child) {
        cJSON *child = item->child;
        while (child) {
            cJSON *new_child = cJSON_Duplicate(child, true);
            if (new_child == NULL) {
                cJSON_Delete(new_item);
                return NULL;
            }
            cJSON_AddItemToArray(new_item, new_child);
            child = child->next;
        }
    }
    
    return new_item;
}

/* Skip whitespace */
static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Parse string */
static const char *parse_string(cJSON *item, const char *str) {
    if (*str != '\"') return NULL;
    str++;
    
    /* Calculate length */
    const char *ptr = str;
    size_t len = 0;
    while (*ptr && *ptr != '\"') {
        if (*ptr == '\\') ptr++;
        if (*ptr) { ptr++; len++; }
    }
    if (*ptr != '\"') return NULL;
    
    /* Allocate string */
    item->valuestring = (char*)g_malloc(len + 1);
    if (item->valuestring == NULL) return NULL;
    
    /* Copy string */
    char *out = item->valuestring;
    while (*str && *str != '\"') {
        if (*str == '\\') {
            str++;
            switch (*str) {
                case 'n': *out++ = '\n'; break;
                case 't': *out++ = '\t'; break;
                case 'r': *out++ = '\r'; break;
                case 'b': *out++ = '\b'; break;
                case 'f': *out++ = '\f'; break;
                case '\\': *out++ = '\\'; break;
                case '\"': *out++ = '\"'; break;
                case '/': *out++ = '/'; break;
                case 'u': str += 4; *out++ = '?'; break;
                default: *out++ = *str;
            }
        } else {
            *out++ = *str;
        }
        str++;
    }
    *out = '\0';
    item->type = cJSON_String;
    
    return (*str == '\"') ? str + 1 : NULL;
}

/* Parse number */
static const char *parse_number(cJSON *item, const char *str) {
    char *end;
    double num = strtod(str, &end);
    if (end == str) return NULL;
    
    item->type = cJSON_Number;
    item->valuedouble = num;
    item->valueint = (int)num;
    return end;
}

/* Forward declaration */
static const char *parse_value(cJSON *item, const char *str);

/* Parse array */
static const char *parse_array(cJSON *item, const char *str) {
    if (*str != '[') return NULL;
    str = skip_ws(str + 1);
    
    item->type = cJSON_Array;
    
    if (*str == ']') return str + 1;
    
    while (*str) {
        cJSON *new_item = cJSON_CreateNull();
        if (new_item == NULL) return NULL;
        
        str = parse_value(new_item, skip_ws(str));
        if (str == NULL) {
            cJSON_Delete(new_item);
            return NULL;
        }
        
        cJSON_AddItemToArray(item, new_item);
        str = skip_ws(str);
        
        if (*str == ']') return str + 1;
        if (*str != ',') return NULL;
        str = skip_ws(str + 1);
    }
    return NULL;
}

/* Parse object */
static const char *parse_object(cJSON *item, const char *str) {
    if (*str != '{') return NULL;
    str = skip_ws(str + 1);
    
    item->type = cJSON_Object;
    
    if (*str == '}') return str + 1;
    
    while (*str) {
        cJSON *child = cJSON_CreateNull();
        if (child == NULL) return NULL;
        
        /* Parse key */
        str = skip_ws(str);
        str = parse_string(child, str);
        if (str == NULL) {
            cJSON_Delete(child);
            return NULL;
        }
        child->string = child->valuestring;
        child->valuestring = NULL;
        
        /* Colon */
        str = skip_ws(str);
        if (*str != ':') {
            cJSON_Delete(child);
            return NULL;
        }
        str = skip_ws(str + 1);
        
        /* Parse value */
        str = parse_value(child, str);
        if (str == NULL) {
            cJSON_Delete(child);
            return NULL;
        }
        
        cJSON_AddItemToArray(item, child);
        str = skip_ws(str);
        
        if (*str == '}') return str + 1;
        if (*str != ',') return NULL;
        str = skip_ws(str + 1);
    }
    return NULL;
}

/* Parse value */
static const char *parse_value(cJSON *item, const char *str) {
    str = skip_ws(str);
    
    if (*str == '\0') return NULL;
    
    /* null */
    if (strncmp(str, "null", 4) == 0) {
        item->type = cJSON_Null;
        return str + 4;
    }
    /* false */
    if (strncmp(str, "false", 5) == 0) {
        item->type = cJSON_False;
        return str + 5;
    }
    /* true */
    if (strncmp(str, "true", 4) == 0) {
        item->type = cJSON_True;
        return str + 4;
    }
    /* string */
    if (*str == '\"') {
        return parse_string(item, str);
    }
    /* array */
    if (*str == '[') {
        return parse_array(item, str);
    }
    /* object */
    if (*str == '{') {
        return parse_object(item, str);
    }
    /* number */
    if (isdigit((unsigned char)*str) || *str == '.' || *str == '-') {
        return parse_number(item, str);
    }
    
    return NULL;
}

/* Parse */
cJSON *cJSON_Parse(const char *value) {
    if (value == NULL) return NULL;
    
    cJSON *item = (cJSON*)g_malloc(sizeof(cJSON));
    if (item == NULL) return NULL;
    
    memset(item, 0, sizeof(cJSON));
    
    const char *end = parse_value(item, value);
    if (end == NULL) {
        cJSON_Delete(item);
        return NULL;
    }
    
    return item;
}

cJSON *cJSON_ParseWithLength(const char *value, size_t buffer_length) {
    (void)buffer_length;
    return cJSON_Parse(value);
}

cJSON *cJSON_ParseWithOpts(const char *value, const char **return_parse_end,
                            cJSON_bool require_null_terminated) {
    (void)require_null_terminated;
    cJSON *item = cJSON_Parse(value);
    if (return_parse_end) *return_parse_end = value;
    return item;
}

/* Print helpers */
static char *print_string(const char *str) {
    size_t len = strlen(str);
    char *out = (char*)g_malloc(len * 2 + 3);
    if (out == NULL) return NULL;
    
    char *ptr = out;
    *ptr++ = '\"';
    while (*str) {
        switch (*str) {
            case '\"': *ptr++ = '\\'; *ptr++ = '\"'; break;
            case '\\': *ptr++ = '\\'; *ptr++ = '\\'; break;
            case '\n': *ptr++ = '\\'; *ptr++ = 'n'; break;
            case '\t': *ptr++ = '\\'; *ptr++ = 't'; break;
            case '\r': *ptr++ = '\\'; *ptr++ = 'r'; break;
            case '\b': *ptr++ = '\\'; *ptr++ = 'b'; break;
            case '\f': *ptr++ = '\\'; *ptr++ = 'f'; break;
            default:
                if ((unsigned char)*str < 32) {
                    ptr += sprintf(ptr, "\\u%04x", (unsigned char)*str);
                } else {
                    *ptr++ = *str;
                }
        }
        str++;
    }
    *ptr++ = '\"';
    *ptr = '\0';
    return out;
}

static char *print_value(const cJSON *item, int depth, int fmt);

static char *print_array(const cJSON *item, int depth, int fmt) {
    size_t bufsize = 256;
    char *buf = (char*)g_malloc(bufsize);
    if (buf == NULL) return NULL;
    
    char *ptr = buf;
    *ptr++ = '[';
    if (fmt) *ptr++ = '\n';
    
    cJSON *child = item->child;
    while (child) {
        if (fmt) {
            for (int i = 0; i < depth + 1; i++) *ptr++ = '\t';
        }
        
        char *child_str = print_value(child, depth + 1, fmt);
        if (child_str) {
            size_t len = strlen(child_str);
            /* Reallocate if needed */
            if (ptr - buf + len + 10 > bufsize) {
                bufsize = (ptr - buf + len + 10) * 2;
                char *newbuf = (char*)g_realloc(buf, bufsize);
                if (newbuf == NULL) {
                    g_free(child_str);
                    g_free(buf);
                    return NULL;
                }
                ptr = newbuf + (ptr - buf);
                buf = newbuf;
            }
            strcpy(ptr, child_str);
            ptr += len;
            g_free(child_str);
        }
        
        if (child->next) {
            *ptr++ = ',';
            if (fmt) *ptr++ = ' ';
        }
        if (fmt) *ptr++ = '\n';
        child = child->next;
    }
    
    if (fmt) {
        for (int i = 0; i < depth; i++) *ptr++ = '\t';
    }
    *ptr++ = ']';
    *ptr = '\0';
    
    return buf;
}

static char *print_object(const cJSON *item, int depth, int fmt) {
    size_t bufsize = 256;
    char *buf = (char*)g_malloc(bufsize);
    if (buf == NULL) return NULL;
    
    char *ptr = buf;
    *ptr++ = '{';
    if (fmt) *ptr++ = '\n';
    
    cJSON *child = item->child;
    while (child) {
        if (fmt) {
            for (int i = 0; i < depth + 1; i++) *ptr++ = '\t';
        }
        
        /* Print key */
        char *key_str = print_string(child->string);
        if (key_str) {
            strcpy(ptr, key_str);
            ptr += strlen(key_str);
            g_free(key_str);
        }
        
        *ptr++ = ':';
        if (fmt) *ptr++ = '\t';
        
        /* Print value */
        char *val_str = print_value(child, depth + 1, fmt);
        if (val_str) {
            size_t len = strlen(val_str);
            if (ptr - buf + len + 10 > bufsize) {
                bufsize = (ptr - buf + len + 10) * 2;
                char *newbuf = (char*)g_realloc(buf, bufsize);
                if (newbuf == NULL) {
                    g_free(val_str);
                    g_free(buf);
                    return NULL;
                }
                ptr = newbuf + (ptr - buf);
                buf = newbuf;
            }
            strcpy(ptr, val_str);
            ptr += len;
            g_free(val_str);
        }
        
        if (child->next) {
            *ptr++ = ',';
            if (fmt) *ptr++ = ' ';
        }
        if (fmt) *ptr++ = '\n';
        child = child->next;
    }
    
    if (fmt) {
        for (int i = 0; i < depth; i++) *ptr++ = '\t';
    }
    *ptr++ = '}';
    *ptr = '\0';
    
    return buf;
}

static char *print_value(const cJSON *item, int depth, int fmt) {
    if (item == NULL) return NULL;
    
    switch (item->type) {
        case cJSON_Null:
            return md_strdup("null");
        case cJSON_False:
            return md_strdup("false");
        case cJSON_True:
            return md_strdup("true");
        case cJSON_Number: {
            char buf[64];
            if (item->valueint == item->valuedouble) {
                snprintf(buf, sizeof(buf), "%d", item->valueint);
            } else {
                snprintf(buf, sizeof(buf), "%.15g", item->valuedouble);
            }
            return md_strdup(buf);
        }
        case cJSON_String:
            return print_string(item->valuestring ? item->valuestring : "");
        case cJSON_Array:
            return print_array(item, depth, fmt);
        case cJSON_Object:
            return print_object(item, depth, fmt);
        default:
            return NULL;
    }
}

char *cJSON_Print(const cJSON *item) {
    return print_value(item, 0, 1);
}

char *cJSON_PrintUnformatted(const cJSON *item) {
    return print_value(item, 0, 0);
}

char *cJSON_PrintBuffered(const cJSON *item, int prebuffer, cJSON_bool fmt) {
    (void)prebuffer;
    return print_value(item, 0, fmt);
}

cJSON_bool cJSON_PrintPreallocated(cJSON *item, char *buffer, const int length, const cJSON_bool format) {
    (void)format;  /* Unused parameter */
    char *output = cJSON_PrintUnformatted(item);
    if (output == NULL || length <= 0) return false;
    
    size_t needed = strlen(output) + 1;
    if (length < (int)needed) {
        g_free(output);
        return false;
    }
    
    strncpy(buffer, output, needed);
    g_free(output);
    return true;
}

/* Compare */
cJSON_bool cJSON_Compare(const cJSON *a, const cJSON *b, const cJSON_bool case_sensitive) {
    if (a == NULL && b == NULL) return true;
    if (a == NULL || b == NULL) return false;
    if (a->type != b->type) return false;
    
    if (a->string && b->string) {
        if (case_sensitive) {
            if (strcmp(a->string, b->string) != 0) return false;
        } else {
            if (strcasecmp(a->string, b->string) != 0) return false;
        }
    } else if (a->string || b->string) {
        return false;
    }
    
    switch (a->type) {
        case cJSON_String:
        case cJSON_Raw:
            if (a->valuestring && b->valuestring) {
                return strcmp(a->valuestring, b->valuestring) == 0;
            }
            return a->valuestring == b->valuestring;
        case cJSON_Number:
            return fabs(a->valuedouble - b->valuedouble) < DBL_EPSILON;
        case cJSON_Array:
        case cJSON_Object: {
            if (cJSON_GetArraySize(a) != cJSON_GetArraySize(b)) return false;
            cJSON *a_child = a->child;
            cJSON *b_child = b->child;
            while (a_child && b_child) {
                if (!cJSON_Compare(a_child, b_child, case_sensitive)) return false;
                a_child = a_child->next;
                b_child = b_child->next;
            }
            return a_child == NULL && b_child == NULL;
        }
        default:
            return true;
    }
}

/* Error handling */
const char *cJSON_GetErrorPtr(void) { return NULL; }
const char *cJSON_GetStringValue(const cJSON *item) {
    return (item && cJSON_IsString(item)) ? item->valuestring : NULL;
}
double cJSON_GetNumberValue(const cJSON *item) {
    return (item && cJSON_IsNumber(item)) ? item->valuedouble : 0.0;
}

/* Missing stubs */
void cJSON_Minify(char *json) { (void)json; }
cJSON *cJSON_DetachItemViaPointer(cJSON *parent, cJSON *item) {
    if (!parent || !item) return NULL;
    if (parent->child == item) {
        parent->child = item->next;
    } else {
        cJSON *prev = parent->child;
        while (prev && prev->next != item) prev = prev->next;
        if (prev) prev->next = item->next;
    }
    if (item->prev) item->prev->next = item->next;
    if (item->next) item->next->prev = item->prev;
    item->prev = item->next = NULL;
    return item;
}
cJSON *cJSON_DetachItemFromArray(cJSON *array, int which) {
    if (!array) return NULL;
    cJSON *item = cJSON_GetArrayItem(array, which);
    return cJSON_DetachItemViaPointer(array, item);
}
cJSON *cJSON_DetachItemFromObject(cJSON *object, const char *string) {
    if (!object || !string) return NULL;
    cJSON *item = object->child;
    while (item) {
        if (item->string && strcmp(item->string, string) == 0) {
            return cJSON_DetachItemViaPointer(object, item);
        }
        item = item->next;
    }
    return NULL;
}
void cJSON_DeleteItemFromArray(cJSON *array, int which) { cJSON_Delete(cJSON_DetachItemFromArray(array, which)); }
void cJSON_DeleteItemFromObject(cJSON *object, const char *string) { cJSON_Delete(cJSON_DetachItemFromObject(object, string)); }
void cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string) { cJSON_DeleteItemFromObject(object, string); }
void cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem) { (void)array; (void)which; (void)newitem; }
cJSON_bool cJSON_ReplaceItemViaPointer(cJSON *parent, cJSON *item, cJSON *replacement) { (void)parent; (void)item; (void)replacement; return false; }
cJSON_bool cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem) { (void)array; (void)which; (void)newitem; return false; }
cJSON_bool cJSON_ReplaceItemInObject(cJSON *object, const char *string, cJSON *newitem) { (void)object; (void)string; (void)newitem; return false; }
cJSON_bool cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object, const char *string, cJSON *newitem) { return cJSON_ReplaceItemInObject(object, string, newitem); }
void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item) { cJSON_AddItemToArray(array, item); }
void cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item) { cJSON_AddItemToObject(object, string, item); }
cJSON *cJSON_CreateRaw(const char *raw) { return cJSON_CreateString(raw); }
cJSON *cJSON_CreateIntArray(const int *numbers, int count) { (void)numbers; (void)count; return cJSON_CreateArray(); }
cJSON *cJSON_CreateFloatArray(const float *numbers, int count) { (void)numbers; (void)count; return cJSON_CreateArray(); }
cJSON *cJSON_CreateDoubleArray(const double *numbers, int count) { (void)numbers; (void)count; return cJSON_CreateArray(); }
cJSON *cJSON_CreateStringArray(const char **strings, int count) { (void)strings; (void)count; return cJSON_CreateArray(); }
cJSON *cJSON_ParseWithLengthOpts(const char *value, size_t buffer_length, const char **return_parse_end, cJSON_bool require_null_terminated) {
    (void)buffer_length; (void)return_parse_end; (void)require_null_terminated;
    return cJSON_Parse(value);
}
cJSON *cJSON_DetachItemFromObjectCaseSensitive(cJSON *object, const char *string) {
    return cJSON_DetachItemFromObject(object, string);
}
