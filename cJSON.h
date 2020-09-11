/*
 * cJSON.h
 *
 *  Created on: 8 Sep 2020
 *      Author: fahrul
 */

#ifndef IO_SUPPORT_CJSON_CJSON_H_
#define IO_SUPPORT_CJSON_CJSON_H_
#include "string.h"
#include "stddef.h"
#include "stdlib.h"

/* cJSON Types: */
#define cJSON_Invalid (0)
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)
#define cJSON_Raw    (1 << 7) /* raw json */

//cJSON structer
typedef struct _cJSON_Def{
  //allow to walk array/object chains
  struct _cJSON_Def *next;
  struct _cJSON_Def *prev;

  //
  struct _cJSON_Def *child;

  //cJSON type
  unsigned char type;
  /* The item's string, if type==cJSON_String  and type == cJSON_Raw */
  char *valuestring;
  /* writing to valueint is DEPRECATED, use cJSON_SetNumberValue instead */
  int valueint;
  /* The item's number, if type==cJSON_Number */
  double valuedouble;

  /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
  char *string;

}cJSON;


/**
 * @brief	initialize function cJSON with static memory
 * @param	mallocation function
 * @param	free mallocation function
 * @retval	none
 */

void cJSON_initMalloc(void (*mallocFunction)(size_t __size__), void(*freeMalloc)(void *pointer));

/**
 * @brief	initialize function cJSON with dynamic memory
 * @param	mallocation function
 * @retval	none
 */

void cJSON_initMemory(void *memory, size_t size);

/**
  * @brief 	Validation array of data character
  * @param 	data: Array of data charcter to be validation
  * @param 	json: result of validation data
  * @retval 	TRUE: validation success
  * 		FALSE: validation failed
  */
unsigned char cJSON_ObjectValidation(char *data, char *json);

/**
  * @brief 	parse json data
  * @param 	json: json data will be parse
  * @retval 	NULL: data not found
  * 		other: data found
  */
cJSON *cJSON_parse(char *json);

/**
 * @brief	free up memory
 * @param	memory pointer will be free up
 */
void cJSON_free(void *pointer);

/**
 * @brief	find JSON object by name
 * @param	name:	keyword name
 * @param	object:	JSON object
 * @retval	NULL: object not found
 * 		other: object found
 */
cJSON *cJSON_getObjectByName(const cJSON * const  object, const char * const  name);

/**
 *@brief	delete json object
 *@param	head: head of json object
 */
void cJSON_Delete(cJSON *head);


cJSON *cJSON_getArrayObject(cJSON *array, unsigned char index);
#endif /* IO_SUPPORT_CJSON_CJSON_H_ */
