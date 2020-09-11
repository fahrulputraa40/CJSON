/*
 * cJSON.c
 *
 *  Created on: 8 Sep 2020
 *      Author: fahrul
 */
#include "cJSON.h"

#define INT_MAX					0x100
#define INT_MIN					-0x100

#define cJSON_Terminated			223
#define cJSON_NESTING_LIMIT			5

//what is data in buffer with specific size and buffer is not null?
#define can_read(buffer, size)			(buffer != NULL && buffer->offset+size <= buffer->length)
//check if buffer can access at given index
#define can_access_at_index(buffer, index)	(buffer != NULL && buffer->offset+index < buffer->length)
//get pointer to buffer at the position
#define buffer_at_offset(buffer)		(buffer->contents+buffer->offset)

typedef struct _cJSON_Hooks{
  void *(*malloc)(size_t size);
  void (*freeMalloc)(void *pointer);
  void *memory;
  size_t sizeMemory;
  size_t terminatePosition;
}cJSON_Hook;


typedef struct _cJSONparseBuffer{
  char *contents;
  unsigned int length;
  unsigned int offset;
  unsigned int depth;
}cJSON_parseBuffer;

cJSON_Hook hooks = {
  .malloc = NULL,
  .memory = NULL,
  .sizeMemory = 0,
  .freeMalloc = NULL,
  .terminatePosition = 0
};

unsigned char parse_string(cJSON *item, cJSON_parseBuffer *buffer);
unsigned char parseValue(cJSON *item, cJSON_parseBuffer *buffer);
cJSON *cJSON_newItem();

/**
 * @brief		get free memory of dynamic memory
 * @param		get free memory of certain size
 * @note		character '\0' sign memory is free
 */
void *cJSON_getMemoryAllocation(size_t size){
  unsigned int i;
  size_t y=0;
  for(i=0; i<hooks.sizeMemory; i++){
      if(((char *)hooks.memory)[i] == '\0')
//	return (hooks.memory+i);
	{
	 for(y=0;y<size && (unsigned int)y+ i < hooks.sizeMemory;y++){
	     if(((char *)hooks.memory)[(unsigned int)y+ i] != '\0')
	       break;
	 }
	 if(y == size)
	   return (hooks.memory+i);
	}

  }
  return NULL;
}

/**
 * @brief		free up memory of dynamic memory
 * @param		memory pointer of dynamic memory
 * @note		character '\t' sign limit of memory
 */
void cJSON_freeMemory(void *pointer){
  size_t size;
  for(size = 0; size < hooks.sizeMemory; size++)
    {
      if(((char *)pointer)[size] == cJSON_Terminated){
	  size++;
	  break;
      }
    }
  memset(pointer, '\0', size);
}

void *cJSON_save(size_t size){
  void *ptr = NULL;

  if(hooks.malloc != NULL){
    ptr = hooks.malloc(size);
  }
  else if(hooks.memory != NULL){
    ptr = cJSON_getMemoryAllocation(size+2);
    ((char *)ptr)[size+1] = cJSON_Terminated;
  }

  if(ptr != NULL)
    memset(ptr, '*', size);
  return ptr;
}

void cJSON_free(void *data){
  if(hooks.freeMalloc != NULL)
    {
      hooks.freeMalloc(data);
    }
  else if(hooks.memory != NULL){
      cJSON_freeMemory(data);
  }
}

void cJSON_initMemory(void *memory, size_t size){
  hooks.freeMalloc = NULL;
  hooks.malloc = NULL;
  hooks.memory = memory;
  hooks.sizeMemory = size;
  memset(memory, '\0', size);
}

static cJSON_parseBuffer *buffer_skip_white_space(cJSON_parseBuffer *buffer){
  if(!can_access_at_index(buffer, 0))
    return NULL;
  while(can_access_at_index(buffer, 0) && buffer_at_offset(buffer)[0] <= 32){
      buffer->offset++;
  }
  if(buffer->offset == buffer->length)
    return NULL;
  return buffer;
}

//skip utf 8 bom (byte order mask)
cJSON_parseBuffer *skip_utf8_bom(cJSON_parseBuffer *buffer){
  if(buffer != NULL || buffer->contents == NULL || buffer->offset != 0)
    return NULL;
  if(can_access_at_index(buffer, 4) && (strncmp((const char *)buffer_at_offset(buffer),  "\xEF\xBB\xBF", 3) == 0)){
      buffer->offset += 3;
  }
  return buffer;
}


unsigned char parse_object(cJSON *item, cJSON_parseBuffer *buffer){
  unsigned char result = 1;
  cJSON *head = NULL;
  cJSON *current_item = NULL;

  if(buffer->depth >= cJSON_NESTING_LIMIT){
      return 0; //to deeply
  }

  buffer->depth++;
  if(!can_access_at_index(buffer, 0) || buffer_at_offset(buffer)[0] != '{'){
      return 0; //not object
  }

  buffer->offset++;
  buffer_skip_white_space(buffer);
  if(can_access_at_index(buffer, 0) && buffer_at_offset(buffer)[0] == '}'){
      return 1; //empity object
  }

  //check if we skip to end of buffer
  if(!can_access_at_index(buffer, 0)){
      buffer->offset--;
      return 0;
  }
  //back to first character
  buffer->offset--;
  do{
      cJSON *new_item = cJSON_newItem();
      if(new_item == NULL){
	  result = 0;
	  break;
      }
      if(head == NULL){
	  current_item = head = new_item;
      }
      else{
	  current_item ->next = new_item;
	  new_item->prev = current_item;
	  current_item = new_item;
      }
      buffer->offset++;
      buffer_skip_white_space(buffer);
      if(!parse_string(current_item, buffer)){
	  result = 0;
	  break;
      }
      current_item->string = current_item->valuestring;
      current_item->valuestring = NULL;
      buffer_skip_white_space(buffer);
      if(!can_access_at_index(buffer, 0) || buffer_at_offset(buffer)[0] != ':'){
	  result = 0;
	  break;
      }
      buffer->offset++;
      buffer_skip_white_space(buffer);
      if(!parseValue(current_item, buffer)){
	  result = 0;
	  break;
      }
      buffer_skip_white_space(buffer);
  }while(can_access_at_index(buffer, 0) && buffer_at_offset(buffer)[0] == ',');

  if(result == 0 || (!can_access_at_index(buffer, 0) || buffer_at_offset(buffer)[0] != '}')){
      //cJSON Delete
      cJSON_Delete(head);
      return 0;
  }

  buffer->depth--;
  if(head != NULL)
    head->prev = current_item;
  item->type = cJSON_Object;
  item->child = head;
  buffer->offset++;
  return result;
}

unsigned char parse_string(cJSON *item, cJSON_parseBuffer *buffer){
  const char *end_pointer = buffer_at_offset(buffer)+1;
  const char *start_pointer = buffer_at_offset(buffer)+1;
  char *output = NULL;

  if(buffer_at_offset(buffer)[0] != '\"')
    return 0;

  size_t allocation_length = 0;

  //start parsing
  while(((size_t)(end_pointer - buffer->contents)) < buffer->length && *end_pointer != '\"'){
      //is escape sequent
      end_pointer++;
  }

  //string ended expectedly
  if(((size_t)(end_pointer - buffer->contents)) >= buffer->length || (*end_pointer != '\"'))
      return 0;

  allocation_length = (size_t)(end_pointer - buffer_at_offset(buffer));
  output = (char *)(cJSON_save(allocation_length+sizeof("")));
  //failed to create memory
  if(output == NULL)
    return 0;
  item->valuestring = output;
  while(start_pointer < end_pointer){
      output[0] = start_pointer[0];
      output++;
      start_pointer++;
  }
  output[0] = '\0';
  item->type = cJSON_String;
  buffer->offset = (size_t)(end_pointer - buffer->contents);
  buffer->offset++;
  return 1;
}

unsigned char parseNumbers(cJSON *item, cJSON_parseBuffer *buffer){
  double number = 0;
  char *after_end = NULL;
  char number_c_string[64];
  size_t i = 0;

  for(i=0; i < sizeof(number_c_string) && can_access_at_index(buffer, i); i++){
      unsigned char isBreak = 0;
      switch (buffer_at_offset(buffer)[i]) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '+':
	case '-':
	case 'e':
	case 'E':
	case '.':
	    number_c_string[i] = buffer_at_offset(buffer)[i];
	    break;
	default:
	  isBreak = 1;
	  break;
      }
      if(isBreak)
	break;
  }
  number_c_string[i] = '\0';
  number = strtod(number_c_string, (char **)&after_end);
  if(number_c_string == after_end)
    return 0;
  item->valueint = (int)number;

  if(number > INT_MAX)
    item->valueint = INT_MAX;
  else if(number < INT_MIN)
    item->valueint = INT_MIN;
  else
    item->valueint = (int)number;

  item->type = cJSON_Number;
  buffer->offset += (size_t)(after_end - number_c_string);
  return 1;
}

unsigned char parseArray(cJSON *item, cJSON_parseBuffer *buffer){
  cJSON *head = NULL;
  cJSON *current_item;
  unsigned char result = 1;

  if(buffer->depth >= cJSON_NESTING_LIMIT){
      return 0;
  }
  buffer->depth++;
  if(buffer_at_offset(buffer)[0] != '['){
    return 0;
  }
  buffer->offset++;
  buffer_skip_white_space(buffer);
  if(can_access_at_index(buffer, 0) && buffer_at_offset(buffer)[0] == ']')
  {
    buffer->depth--;
    item->type = cJSON_Array;
    item->child = head;
    buffer->offset++;
    return 1;
  }

  if(!can_access_at_index(buffer,0))
  {
    buffer->offset--;
    return 0;
  }
  buffer->offset--;
  do{
      cJSON *new_item = cJSON_newItem();
      if(new_item == NULL)
      {
	result = 0;
	break;
      }
      if(head == NULL){
	  current_item = head = new_item;
      }
      else{
	  current_item->next = new_item;
	  new_item->prev = current_item;
	  current_item = new_item;
      }
      buffer->offset++;
      buffer_skip_white_space(buffer);
      if(!parseValue(current_item, buffer)){
	  result = 0;
	  break;
      }
      buffer_skip_white_space(buffer);
  }while(can_access_at_index(buffer, 0) && buffer_at_offset(buffer)[0] == ',');

  if(result == 0 ||!can_access_at_index(buffer, 0) || buffer_at_offset(buffer)[0] != ']'){
      cJSON_Delete(head);
      return 0;
  }

  buffer->depth--;
  if(head != NULL){
      head->prev = current_item;
  }

  item->type = cJSON_Array;
  item->child = head;

  buffer->offset++;
  return result;
}

unsigned char parseValue(cJSON *item, cJSON_parseBuffer *buffer){
  if(item== NULL || buffer == NULL || buffer->contents == NULL)
    return 0;
//  System_Debug_println("Parsing excute %c at offset: %d", buffer_at_offset(buffer)[0], buffer->offset);
  //parse different types of values:
  //null
  if(can_read(buffer, 4) && (strncmp((const char *)buffer->contents, "null", 4) == 0)){
      item->type = cJSON_NULL;
      buffer->offset+=4;
      return 1;
  }
  //false
  else if(can_read(buffer, 5) && (strncmp((const char *)buffer->contents, "false", 5) == 0)){
        item->type = cJSON_False;
        buffer->offset+=5;
        return 1;
    }
  //true
  else if(can_read(buffer, 4) && (strncmp((const char *)buffer->contents, "true", 4) == 0)){
          item->type = cJSON_True;
          buffer->offset+=4;
          return 1;
   }
  //string
  else if(can_read(buffer, 0) && buffer_at_offset(buffer)[0] == '\"'){
            return parse_string(item, buffer);
     }
  //number
  else if(can_read(buffer, 0) && buffer_at_offset(buffer)[0] >= '0' && buffer_at_offset(buffer)[0] <= '9'){
	return parseNumbers(item, buffer);
  }
  //object
  else if(can_read(buffer, 0) && buffer_at_offset(buffer)[0] == '{'){
      return parse_object(item, buffer);
  }
  //array
  else if(can_read(buffer, 0) && buffer_at_offset(buffer)[0] == '['){
        return parseArray(item, buffer);
  }


  return 0;
}

cJSON *cJSON_parse(char *json){
  cJSON_parseBuffer buffer;
  buffer.depth = 0;
  buffer.length = strlen(json);
  buffer.offset = 0;

  buffer.contents = json;
  cJSON *item = cJSON_newItem();
  if(item == NULL || buffer.contents == NULL)
    return NULL;
  if(!parseValue(item, &buffer))
    return NULL;
   return item;
}


cJSON *cJSON_getObjectByName(const cJSON * const  object, const char * const  name){
  cJSON *current_item = NULL;
  unsigned depth = 0;

  if(object == NULL || name == NULL){
      return NULL;
  }
  current_item = object->child;
  while(current_item != NULL && current_item->string != NULL && strcmp(name, current_item->string) != 0 && depth < cJSON_NESTING_LIMIT){
      current_item = current_item->next;
      depth++;
  }

  if(current_item->string == NULL && current_item == NULL)
    return NULL;


  return current_item;
}

cJSON *cJSON_getArrayObject(cJSON *array, unsigned char index){
  if(array->type != cJSON_Array)
    return NULL;

  cJSON *item = array;
  for(unsigned char i=0;i < index; i++){
   if(item->next != NULL)
    item = item->next;
  }
  if(item->child->type == cJSON_Object)
    return item->child;

  return NULL;
}

cJSON *cJSON_newItem(){
  cJSON *new_item = (cJSON *)cJSON_save(sizeof(cJSON));
  if(new_item == NULL)
    return NULL;

  new_item->child = NULL;
  new_item->next = NULL;
  new_item->string = NULL;
  new_item->valuestring = NULL;
  return new_item;
}

void cJSON_Delete(cJSON *head){
  cJSON *next = NULL;
  while(head != NULL){
      next = head->next;

      if(head->child != NULL){
	  cJSON_Delete(head->child);
      }
      if(head->valuestring != NULL){
	  cJSON_free(head->valuestring);
      }
      if (head->string != NULL){
	cJSON_free (head->string);
      }
      cJSON_free(head);
      head = next;
  }

}

unsigned char cJSON_ObjectValidation(char *data, char *json){
  unsigned char OpenBrackets_Count = 0;
  unsigned char CloseBrackets_Count = 0;
  char *start_pointer = NULL;
  char *end_pointer = NULL;
  const char *bracket = "{}";

  for(unsigned int i=0; i<strlen(data);i++){
      if(data[i] == bracket[0])
	OpenBrackets_Count++;
      if(data[i] == bracket[1])
	CloseBrackets_Count++;
  }

  if(OpenBrackets_Count == 0 || CloseBrackets_Count == 0)
    return 0;
  else if(OpenBrackets_Count > CloseBrackets_Count){
      unsigned char count1 = 0, count2 = 0, i, lock = 0;
      for(i=0;i<strlen(data);i++){
	  if(data[i] == bracket[0])
	    count1++;
	  if(data[i] == bracket[1])
	    count2++;

	  if((count1 == (OpenBrackets_Count +1 - CloseBrackets_Count)) && ((lock & 0x0f) == 0)){
	    start_pointer = &data[i];
	    lock = 0x0f;
	  }
	  if((count2 == CloseBrackets_Count) && ((lock & 0xf0) == 0)){
	      lock = 0xf0;
	    end_pointer = &data[i];
	  }
      }
      strncpy(json, start_pointer, (size_t)(end_pointer-start_pointer)+1);
  }
  else if(OpenBrackets_Count < CloseBrackets_Count){
      unsigned char count1 = 0, count2 = 0, i, lock = 0;
            for(i=0;i<strlen(data);i++){
      	  if(data[i] == bracket[0])
      	    count1++;
      	  if(data[i] == bracket[1])
      	    count2++;

      	  if((count1 == 1) && ((lock & 0x0f) == 0)){
      	    start_pointer = &data[i];
      	    lock = 0x0f;
      	  }
      	  if((count2 == OpenBrackets_Count) && ((lock & 0xf0) == 0)){
      	      lock = 0xf0;
      	    end_pointer = &data[i];
      	  }
            }
            strncpy(json, start_pointer, (size_t)(end_pointer-start_pointer)+1);
  }
  else
      strcpy(json, data);
  return 1;
}
