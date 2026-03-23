/**
 * @file cJSON.h
 * @brief Ultralightweight JSON parser in ANSI C
 * 
 * This is a minimal cJSON implementation for the Magic Debugger project.
 * Based on cJSON by Dave Gamble (https://github.com/DaveGamble/cJSON)
 */

#ifndef CJSON_H
#define CJSON_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* typedef for cJSON_bool */
typedef int cJSON_bool;

/* cJSON types */
#define cJSON_Invalid (0)
#define cJSON_False    (1 << 0)
#define cJSON_True     (1 << 1)
#define cJSON_Null     (1 << 2)
#define cJSON_Number   (1 << 3)
#define cJSON_String   (1 << 4)
#define cJSON_Array    (1 << 5)
#define cJSON_Object   (1 << 6)
#define cJSON_Raw       (1 << 7)
#define cJSON_IsReference   256
#define cJSON_StringIsConst 512

/* The cJSON structure */
typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

/* Memory allocation hooks */
typedef struct cJSON_Hooks {
    void *(*malloc_fn)(size_t sz);
    void (*free_fn)(void *ptr);
} cJSON_Hooks;

/* Setup memory allocation hooks */
void cJSON_InitHooks(cJSON_Hooks *hooks);

/* Parse JSON string */
cJSON *cJSON_Parse(const char *value);
cJSON *cJSON_ParseWithLength(const char *value, size_t buffer_length);
cJSON *cJSON_ParseWithLengthOpts(const char *value, size_t buffer_length,
                                   const char **return_parse_end,
                                   cJSON_bool require_null_terminated);
cJSON *cJSON_ParseWithOpts(const char *value, const char **return_parse_end,
                            cJSON_bool require_null_terminated);

/* Delete cJSON structure */
void cJSON_Delete(cJSON *item);

/* Get array size */
int cJSON_GetArraySize(const cJSON *array);

/* Get array item */
cJSON *cJSON_GetArrayItem(const cJSON *array, int index);

/* Get object item (case insensitive) */
cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string);
cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *object, const char *string);

/* Check type */
cJSON_bool cJSON_IsInvalid(const cJSON *item);
cJSON_bool cJSON_IsFalse(const cJSON *item);
cJSON_bool cJSON_IsTrue(const cJSON *item);
cJSON_bool cJSON_IsBool(const cJSON *item);
cJSON_bool cJSON_IsNull(const cJSON *item);
cJSON_bool cJSON_IsNumber(const cJSON *item);
cJSON_bool cJSON_IsString(const cJSON *item);
cJSON_bool cJSON_IsArray(const cJSON *item);
cJSON_bool cJSON_IsObject(const cJSON *item);
cJSON_bool cJSON_IsRaw(const cJSON *item);

/* Create items */
cJSON *cJSON_CreateNull(void);
cJSON *cJSON_CreateTrue(void);
cJSON *cJSON_CreateFalse(void);
cJSON *cJSON_CreateBool(cJSON_bool boolean);
cJSON *cJSON_CreateNumber(double num);
cJSON *cJSON_CreateString(const char *string);
cJSON *cJSON_CreateRaw(const char *raw);
cJSON *cJSON_CreateArray(void);
cJSON *cJSON_CreateObject(void);

/* Create arrays */
cJSON *cJSON_CreateIntArray(const int *numbers, int count);
cJSON *cJSON_CreateFloatArray(const float *numbers, int count);
cJSON *cJSON_CreateDoubleArray(const double *numbers, int count);
cJSON *cJSON_CreateStringArray(const char **strings, int count);

/* Add items to arrays/objects */
void cJSON_AddItemToArray(cJSON *array, cJSON *item);
void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
void cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item);
void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item);
void cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item);

/* Remove/Detach items */
cJSON *cJSON_DetachItemViaPointer(cJSON *parent, cJSON *item);
cJSON *cJSON_DetachItemFromArray(cJSON *array, int which);
void cJSON_DeleteItemFromArray(cJSON *array, int which);
cJSON *cJSON_DetachItemFromObject(cJSON *object, const char *string);
cJSON *cJSON_DetachItemFromObjectCaseSensitive(cJSON *object, const char *string);
void cJSON_DeleteItemFromObject(cJSON *object, const char *string);
void cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string);

/* Insert items */
void cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem);
cJSON_bool cJSON_ReplaceItemViaPointer(cJSON *parent, cJSON *item, cJSON *replacement);
cJSON_bool cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem);
cJSON_bool cJSON_ReplaceItemInObject(cJSON *object, const char *string, cJSON *newitem);
cJSON_bool cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object, const char *string, cJSON *newitem);

/* Duplicate */
cJSON *cJSON_Duplicate(const cJSON *item, cJSON_bool recurse);
void cJSON_Minify(char *json);

/* Convenience functions */
#define cJSON_AddNullToObject(object, name) cJSON_AddItemToObject(object, name, cJSON_CreateNull())
#define cJSON_AddTrueToObject(object, name) cJSON_AddItemToObject(object, name, cJSON_CreateTrue())
#define cJSON_AddFalseToObject(object, name) cJSON_AddItemToObject(object, name, cJSON_CreateFalse())
#define cJSON_AddBoolToObject(object, name, b) cJSON_AddItemToObject(object, name, cJSON_CreateBool(b))
#define cJSON_AddNumberToObject(object, name, n) cJSON_AddItemToObject(object, name, cJSON_CreateNumber(n))
#define cJSON_AddStringToObject(object, name, s) cJSON_AddItemToObject(object, name, cJSON_CreateString(s))
#define cJSON_AddRawToObject(object, name, s) cJSON_AddItemToObject(object, name, cJSON_CreateRaw(s))

/* Print JSON */
char *cJSON_Print(const cJSON *item);
char *cJSON_PrintUnformatted(const cJSON *item);
char *cJSON_PrintBuffered(const cJSON *item, int prebuffer, cJSON_bool fmt);

/* Print to preallocated buffer */
cJSON_bool cJSON_PrintPreallocated(cJSON *item, char *buffer, const int length, const cJSON_bool format);

/* Compare */
cJSON_bool cJSON_Compare(const cJSON *a, const cJSON *b, const cJSON_bool case_sensitive);

/* Error constants */
#define CJSON_ERROR_NULL (1)
#define CJSON_ERROR_SYNTAX (2)
#define CJSON_ERROR_PARTIAL (3)
#define CJSON_ERROR_INVALID_UTF8 (4)

/* Get error */
const char *cJSON_GetErrorPtr(void);
const char *cJSON_GetStringValue(const cJSON *item);
double cJSON_GetNumberValue(const cJSON *item);

#ifdef __cplusplus
}
#endif

#endif /* CJSON_H */
