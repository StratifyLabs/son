/*! \file */ //Copyright 2011-2017 Tyler Gilbert; All Rights Reserved

#ifndef SON_H_
#define SON_H_

/*! \mainpage
 *
 * Introduction
 * -------------
 *
 * Stratify Object Notation (SON) is an object storage file format similar to JSON but
 * optimized for use on systems with limited resources (e.g. microcontrollers).
 *
 * The following JSON data can easily be created in SON format.
 *
 * \code
 * {
 * 	"float0": 0.5,
 * 	"str0": "Hello World",
 * 	"object0" : { "number": 1 },
 * 	"array0" : [
 * 		"unsigned0": 10,
 * 		"unsigned1": 50
 * 	]
 * 	"bool0": true,
 * 	"bool1": false
 * }
 * \endcode
 *
 * Here is the code to create the above data in SON format.
 *
 * \code
 * #include "son.h"
 *
 * void create_data_file(){
 *  //the stack size needs to exceed the depth
 *  //of the data (depth in the example above is 2)
 * 	son_stack_t stack[4];
 * 	son_t h;
 * 	son_create(&h, "/home/data.son", stack, 4);
 * 	son_open_obj(&h, ""); //create the root object
 * 	son_write_float(&h, "float0", 0.5);
 * 	son_write_str(&h, "str0", "Hello World");
 * 	son_open_obj(&h, "object0");
 * 	son_write_num(&h, "number", 1);
 * 	son_close_obj(&h);
 * 	son_open_array(&h, "array0");
 * 	son_write_unum(&h, "0", 10); //inside arrays -- keys don't matter
 * 	son_write_unum(&h, "1", 50);
 * 	son_close_array(&h);
 * 	son_write_true(&h, "bool0");
 * 	son_write_false(&h, "bool1");
 * 	son_close_obj(&h); //close root -- calling this is optional, all objects/arrays will close on son_close()
 * 	son_close(&h);
 * }
 * \endcode
 *
 * The data in SON files can be accessed using an access string. Keys
 * are separated by periods and array values are accessed using brackets.
 * The following code shows how to access each value of the above example.
 *
 * \code
 * #include "son.h"
 *
 * void read_values(){
 * 	son_t h;
 * 	son_open(&h, "/home/data.son");
 * 	float float0 = son_read_float(&h, "float0");
 * 	char str0[32];
 * 	son_read_str(&h, "str0", str0, 32);
 * 	int number = son_read_num(&h, "object0.number");
 * 	unsigned int array0_0 = son_read_unum(&h, "array0[0]");
 * 	unsigned int array0_1 = son_read_unum(&h, "array0[1]");
 * 	int bool0 = son_read_bool(&h, "bool0");
 * 	int bool1 = son_read_bool(&h, "bool1");
 * }
 * \endcode
 *
 *
 *
 */

/*! \addtogroup SON Stratify Object Notation
 * @{
 */

#define SON_STR_VERSION "0.0.2"
#define SON_VERSION 0x0002
#include "son_phy.h"

/*! \brief SON Size Type */
typedef u32 son_size_t;

/*! \details Defines the maximum length of any given key
 * value.
 *
 * \showinitializer
 */
#define SON_KEY_NAME_SIZE (15)
#define SON_KEY_NAME_CAPACITY (SON_KEY_NAME_SIZE+1)


/*! \details Lists the values for valid data types.
 *
 * These values are provided for reference. They are used internally
 * and not required in the API.
 */
typedef enum {
	SON_STRING /*! String (Internal use only) */,
	SON_FLOAT /*! Float (Internal use only) */,
	SON_NUMBER_U32 /*! Unsigned 32-bit value (Internal use only) */,
	SON_NUMBER_S32 /*! Signed 32-bit value (Internal use only) */,
	SON_DATA /*! Data (user-defined size) (Internal use only) */,
	SON_OBJ /*! Object (Internal use only) */,
	SON_ARRAY /*! Array (Internal use only) */, //can be an array of distinct objects
	SON_TRUE /*! True value (Internal use only) */,
	SON_FALSE /*! False value (Internal use only) */,
	SON_NULL /*! Null value (Internal use only) */,
	SON_TOTAL
} son_value_t;

#ifdef __cplusplus
extern "C" {
#endif


/*! \addtogroup FILES File Handling (Create/Append/Open/Close)
 * @{
 */

/*! \details Defines the stack used when
 * creating and appending files.
 *
 * The members are managed internally and not used in the API.
 *
 * See son_t for an example.
 *
 */
typedef struct {
	son_size_t pos /* Internal use only */;
} son_stack_t;


/*!
 * \details Defines the data type for handling files.
 *
 * All the members are managed internally.
 *
 * Here is an example:
 * \code
 * son_t h;
 * son_stack_t stack[10];
 * son_create(&h, "/home/data.son", stack, 10);
 * son_write_str(&h, "str0", "Hello");
 * son_write_str(&h, "str1", "World");
 * son_close(&h);
 * \endcode
 *
 * \sa son_create(), son_append()
 */
typedef struct {
	son_phy_t phy /* Internal use only */;
	son_stack_t * stack /* Internal use only */;
	size_t stack_size /* Internal use only */;
	size_t stack_loc /* Internal use only */;
	u32 err /* Internal use only */;
} son_t;


/*! \brief See below for details.
 * \details These are the values returned by son_get_error(). The
 * value is set when a son_* function does not complete successfully.
 */
typedef enum {
	SON_ERR_NONE /*! This value indicates no error has occurred. */,
	SON_ERR_NO_ROOT /*! This error happens when a top level object or array is not created. */,
	SON_ERR_OPEN_IO /*! This error happens if there is an IO failure to create or open a file (run perror() for more info). */,
	SON_ERR_READ_IO /*! This error happens when an IO read operation fails (run perror() for more info). */,
	SON_ERR_WRITE_IO /*! This error happens when an IO write operation fails (run perror() for more info). */,
	SON_ERR_CLOSE_IO /*! This error happens when an IO close operation fails (run perror() for more info). */,
	SON_ERR_SEEK_IO /*! This error happens when an IO seek operation fails (run perror() for more info). */,
	SON_ERR_READ_CHECKSUM /*! This error happens when the data in the file has a invalid checksum which indicates a corrupted file or bad file format. */,
	SON_ERR_CANNOT_APPEND /*! This error happens when an append is attempted on a file that has not been saved for appending */,
	SON_ERR_CANNOT_WRITE /*! This error happens if a write is attempted on a file that has been opened for reading */,
	SON_ERR_CANNOT_READ /*! This error happens if a read is attempted on a file that has been opened for writing or appending */,
	SON_ERR_INVALID_ROOT /*! This error happens when the root object is not valid (usually a bad file format or corrupted file). */,
	SON_ERR_ARRAY_INDEX_NOT_FOUND /*! This error happens when an array index could not be found */,
	SON_ERR_ACCESS_TOO_LONG /*! This error happens if the \a access parameter len exceeds \a SON_ACCESS_MAX_USER_SIZE.  */,
	SON_ERR_KEY_NOT_FOUND /*! This error happens when the key specified by the \a access parameter could not be found. */,
	SON_ERR_STACK_OVERFLOW /*! This error happens if the depth (son_open_array() or son_open_obj()) exceeds, the handle's stack size. */,
	SON_ERR_INVALID_KEY /*! This happens if an empty key is passed to anything but the root object. */,
	SON_ERR_CANNOT_CONVERT /*! This happens if a read is tried by the base data can't be converted */,
	SON_ERR_EDIT_TYPE_MISMATCH /*! This happens if a value is edited with a function that doesn't match the base type */
} son_err_t;


#if !defined __StratifyOS__
void son_set_driver(son_t * h, void * driver);
#endif

/*! \details Returns the most recent error and sets the
 * current error value to SON_ERR_NONE.
 *
 * @param h A pointer to the handle
 * @return The most recent error value
 */
int son_get_error(son_t * h);


/*! \details Exports the data in an open SON file to JSON.
 *
 * @param h A pointer to the handle
 * @param path The path to the destination JSON file
 * @return Less than zero for an error
 */
int son_to_json(son_t * h, const char * path);

/*! \details Creates a new SON file.
 *
 * @param h A pointer to the SON handle
 * @param name The path to the file to create
 * @param stack A pointer to the SON stack structure
 * @param stack_size The number of stack entries available in the stack
 * @return Less than zero on error
 *
 * When you create an SON file, you specify the name where the file
 * will be created. If the file already exists, it is overwritten.
 *
 * You also need to provide a stack. This is so the library can work
 * without using dynamic memory allocation. The \a stack_size needs to
 * be as large as the deepest depth of the data. For example, the
 * following JSON data has a depth of 3.
 *
 * \code
 * {
 *   "make": { //depth level 1 (all files have a depth of at least 1)
 *   	"model" : { //depth level 2
 *   		"color" : "red" //depth level 3
 *   	}
 *   }
 * }
 * \endcode
 *
 * The data above can be created with the following code.
 *
 * \code
 * son_t h;
 * son_stack_t stack[4];
 * son_create(&h, "/home/data.son", stack, 4);
 * son_open_obj(&h, ""); //open the root object
 * son_open_obj(&h, "make");
 * son_open_obj(&h, "model");
 * son_write_str(&h, "color", "red");
 * son_close(&h); //this will close each obj (son_close_obj()) when closing the file
 *  //then to access the value
 * char color[16];
 * son_open(&h, "/home/data.son");
 * son_read_str(&h, "make.model.color", color, 16); //color will contain "red"
 * son_close(&h);
 * \endcode
 *
 * \sa WRITE
 *
 */
int son_create(son_t * h, const char * name, son_stack_t * stack, size_t stack_size);

/*! \details Opens a file for appending. Items
 * written to the file will be added to the root object or array.
 *
 * @param h A pointer to the handle
 * @param name Path to the file to append
 * @param stack A pointer to the SON stack
 * @param stack_size The number of stack entries
 * @return Less than zero if there was an error
 *
 * \code
 * son_stack_t son_stack[10];
 * son_t son;
 * son_append(&son, "/home/append.son", son_stack, 10);
 * son_write_str(&son, "str", "World"); //this will be appended to the root object or array
 * son_close(&son, 1); //close out all structures -- no more appending
 * \endcode
 *
 * \sa  WRITE
 */
int son_append(son_t * h, const char * name, son_stack_t * stack, size_t stack_size);

/*! \details Opens a file for reading.
 *
 * @param h A pointer to the handle
 * @param name The path to the file to open
 * @return Less than zero if there was an error
 *
 * Here is an example of how to read key data from a file.
 *
 * \code
 * son_t son;
 * u32 unum;
 * son_open(&son, "/home/data.son");
 * unum = son_read_unum(&son, "key.unum");
 * son_close(&son);
 * \endcode
 *
 * \sa READ
 *
 */
int son_open(son_t * h, const char * name);

/*! \details Closes a file that was opened or created with
 * son_create(), son_append(), or son_open().
 *
 * @param h A pointer to the handle
 * @return Less than zero (perror() for details)
 */
int son_close(son_t * h);


/*! @} */

/*! \addtogroup WRITE Writing/Appending Values to Files
 * @{
 */


/*! \details Starts a new object while creating
 * or appending values to a SON file.
 *
 * @param h A pointer to the handle
 * @param key The key for the new object
 * @return Less than zero for an error (see h->err)
 *
 * \sa son_create()
 *
 */
int son_open_obj(son_t * h, const char * key);

/*! \details Closes an object while creating or appending a
 * SON file.
 *
 * @param h A pointer to the handle
 *
 * The function son_close_obj() is used to close the object.
 *
 * \sa son_create(), son_close_obj()
 */
int son_close_obj(son_t * h);

/*! \details Opens a new array while creating or appending a SON file.
 *
 * @param h A pointer to the handle
 * @param key The key for the new array
 * @return Less then zero for an error
 *
 * The function son_close_array() is used to close the array value.
 *
 * \sa son_create(), son_close_array()
 *
 */
int son_open_array(son_t * h, const char * key);

/*! \details Closes an array while creating or appending a
 * SON file.
 *
 * @param h A pointer to the handle
 * @return Less than zero if there was an error
 *
 * \sa son_create(), son_open_array()
 */
int son_close_array(son_t * h);

/*! \details Opens a data object for writing. The son_write_open_data() function
 * is used to write data to the object. This function can be
 * used when the data is not immediately availabe in RAM (for example,
 * needs to be read from a file).
 *
 * @param h A pointer to the handle
 * @param key The key for the new object
 * @return Less than zero for an error
 *
 * \code
 *
 * son_t son;
 * int fd;
 * int bytes;
 * char buffer[16];
 *
 * son_open_data(&son, "data");
 * while( (bytes = read(fd, buffer, 16)) > 0 ){
 * 	son_write_open_data(buffer, bytes);
 * }
 * son_close_data(&son);
 *
 * \endcode
 *
 * \sa son_close_data(), son_write_open_data()
 */
int son_open_data(son_t * h, const char * key);

/*! \details Closes a data object that has previously
 * been opened using son_open_data().
 *
 * @param h A pointer to the handle
 * @return Less than zero on error
 *
 * \sa son_open_data(), son_write_open_data()
 *
 */
int son_close_data(son_t * h);

/*! \details Writes a string to the file.
 *
 * @param h A pointer to the handler
 * @param key The key to associate with the string
 * @param str The string to save
 * @return Less than zero on an error
 *
 */
int son_write_str(son_t * h, const char * key, const char * str);

/*! \details Writes a signed number to the file.
 *
 * @param h A pointer to the handler
 * @param key The key to associate with the value
 * @param num The value to save
 * @return Less than zero on an error
 *
 */
int son_write_num(son_t * h, const char * key, s32 num);

/*! \details Writes an unsigned number to the file.
 *
 * @param h A pointer to the handler
 * @param key The key to associate with the value
 * @param unum The value to save
 * @return Less than zero on an error
 *
 */
int son_write_unum(son_t * h, const char * key, u32 unum);

/*! \details Writes a float point value to the file.
 *
 * @param h A pointer to the handler
 * @param key The key to associate with the value
 * @param fnum The value to save
 * @return Less than zero on an error
 *
 */
int son_write_float(son_t * h, const char * key, float fnum);

/*! \details Writes a true value to the file.
 *
 * @param h A pointer to the handler
 * @param key The key to associate with the value
 * @return Less than zero on an error
 *
 */
int son_write_true(son_t * h, const char * key);

/*! \details Writes a false value to the file.
 *
 * @param h A pointer to the handler
 * @param key The key to associate with the value
 * @return Less than zero on an error
 *
 */
int son_write_false(son_t * h, const char * key);

/*! \details Writes a null value to the file.
 *
 * @param h A pointer to the handler
 * @param key The key to associate with the value
 * @return Less than zero on an error
 *
 */
int son_write_null(son_t * h, const char * key);

/*! \details Writes a true value to the file.
 *
 * @param h A pointer to the handler
 * @param key The key to associate with the value
 * @param data A pointer to the data to save
 * @param size The number of bytes of data to save
 * @return Less than zero on an error
 *
 */
int son_write_data(son_t * h, const char * key, const void * data, son_size_t size);

/*! \details Writes data to the file. This must be called after
 * son_open_data().
 *
 * @param h A pointer to the handler
 * @param data A pointer to the data to write
 * @param size The number of bytes to write
 * @return Less than zero on an error
 *
 */
int son_write_open_data(son_t * h, const void * data, son_size_t size);

/*! @} */

/*! \addtogroup READ Reading Values
 * @{
 */

/*! \details Defines the maximum length of the user
 * supplied values for the \a access parameter.
 *
 * \showinitializer
 *
 */
#define SON_ACCESS_MAX_USER_SIZE (93)
#define SON_ACCESS_NAME_SIZE (SON_ACCESS_MAX_USER_SIZE+2)
#define SON_ACCESS_NAME_CAPACITY (SON_ACCESS_NAME_SIZE+1)

/*! \details Seeks to the value specified by \a access and
 * copies the size of the data.
 *
 * @param h A pointer to the handler
 * @param access The access string
 * @param size A pointer to the destination data size
 * @return Less than zero on an error
 *
 * This function is useful if the size of the data is
 * unknown when it needs to be read.
 *
 * \code
 * son_t s;
 * son_size_t size;
 * char * name;
 * son_open(&s, "/home/test.son");
 *
 * son_seek(&s, "person.name", &size);
 * name = malloc(size);
 * son_read_str(&s, "person.name", name, size);
 * \endcode
 *
 *
 */
int son_seek(son_t * h, const char * access, son_size_t * size);

/*! \details Reads the value specified by \a access as a string value.
 *
 * @param h A pointer to the handler
 * @param access The access string
 * @param str A pointer to the destination string
 * @param capacity The max capacity of the destination string
 * @return Less than zero on an error
 *
 * If the base value is not a string, it will be converted to a string.
 *
 *
 */
int son_read_str(son_t * h, const char * access, char * str, son_size_t capacity);

/*! \details Reads the value specified by \a access as a signed integer.
 *
 * @param h A pointer to the handler
 * @param access The access string
 * @return Less than zero on an error
 *
 * If the base value is not a signed integer, it will be converted to
 * one if possible. SON_STRING is converted using atoi() and
 * SON_DATA cannot be converted (error will be set to
 * SON_ERR_CANNOT_CONVERT and 0 will be returned).
 *
 *
 */
s32 son_read_num(son_t * h, const char * access);

/*! \details Reads the value specified by \a access as a unsigned integer.
 *
 * @param h A pointer to the handler
 * @param access The access string
 * @return Less than zero on an error
 *
 * If the base value is not a unsigned integer, it will be converted to
 * one if possible. SON_STRING is converted using atoi() and
 * SON_DATA cannot be converted (error will be set to
 * SON_ERR_CANNOT_CONVERT and 0 will be returned).
 *
 *
 */
u32 son_read_unum(son_t * h, const char * access);

/*! \details Reads the value specified by \a access as a floating point value.
 *
 * @param h A pointer to the handler
 * @param access The access string
 * @return Less than zero on an error
 *
 * If the base value is not a floating point value, it will be converted to
 * one if possible. SON_STRING is converted using atof() and
 * SON_DATA cannot be converted (error will be set to
 * SON_ERR_CANNOT_CONVERT and 0.0 will be returned).
 *
 */
float son_read_float(son_t * h, const char * access);


/*! \details Reads the value specified by \a access as raw data (no type).
 *
 * @param h A pointer to the handler
 * @param access The access string
 * @param data A pointer to the destination data
 * @param size The number of bytes available at the destination
 * @return Less than zero on an error
 *
 * If the base data type is not raw data, it will be interpreted as raw
 * data without performing any conversions.
 *
 */
int son_read_data(son_t * h, const char * access, void * data, son_size_t size);

/*! \details Reads the value specified by \a access as a boolean.
 *
 * @param h A pointer to the handler
 * @param access The access string
 * @return Less than zero on an error
 *
 * If the base data type is not true or false, it will be converted
 * to true (1) or false (0).
 *
 */
int son_read_bool(son_t *h, const char * access);

/*! @} */

/*! \addtogroup EDIT Editing Values
 * @{
 */

/*! \details Opens a file for editing.
 *
 * For variable length data types (SON_DATA and SON_STRING), the data
 * can be modified but the size of the data is fixed. Writing a string
 * that is shorter will work as expected. Writing a string (or data
 * chunk) that is larger than the original will trucate data
 * that doesn't fit.
 *
 * When a value is edited, the value type must match the function
 * used to edit the value (no conversion is performed). An error
 * (SON_ERR_EDIT_TYPE_MISMATCH) will be set if this is attempted.
 *
 * @param h A pointer to the handle
 * @param name The name of the file to open
 * @return Less than zero for an error
 *
 */
int son_edit(son_t * h, const char * name);

/*! \details Edits a float value.
 *
 * @param h A pointer to the handle
 * @param access The access string
 * @param value The new value
 * @return Less than zero for an error
 *
 * \sa son_edit()
 *
 */
int son_edit_float(son_t * h, const char * access, float value);

/*! \details Edits a data value.
 *
 * @param h A pointer to the handle
 * @param access The access string
 * @param data A pointer to the source data
 * @param size The number of source data bytes
 * @return Less than zero for an error
 *
 * \sa son_edit()
 *
 */
int son_edit_data(son_t * h, const char * access, const void * data, son_size_t size);

/*! \details Edits a data value.
 *
 * @param h A pointer to the handle
 * @param access The access string
 * @param str The new string value
 * @return Less than zero for an error
 *
 * \sa son_edit()
 *
 */
int son_edit_str(son_t * h, const char * access, const char * str);

/*! \details Edits a signed integer value.
 *
 * @param h A pointer to the handle
 * @param access The access string
 * @param value The new value
 * @return Less than zero for an error
 *
 * \sa son_edit()
 *
 */
int son_edit_num(son_t *h, const char * access, s32 value);

/*! \details Edits an unsigned integer value.
 *
 * @param h A pointer to the handle
 * @param access The access string
 * @param value The new value
 * @return Less than zero for an error
 *
 * \sa son_edit()
 *
 */
int son_edit_unum(son_t * h, const char * access, u32 value);

/*! \details Edits a true or false value.
 *
 * @param h A pointer to the handle
 * @param access The access string
 * @param value The new value
 * @return Less than zero for an error
 *
 * \sa son_edit()
 *
 */
int son_edit_bool(son_t * h, const char * access, int value);

/*! @} */

/*! @} */

typedef struct MCU_PACK {
	u32 version;
	int (*get_error)(son_t * h);
	int (*create)(son_t * h, const char * name, son_stack_t * stack, size_t stack_size);
	int (*append)(son_t * h, const char * name, son_stack_t * stack, size_t stack_size);
	int (*open)(son_t * h, const char * name);
	int (*close)(son_t * h);
	int (*to_json)(son_t * h, const char * path);
	int (*open_obj)(son_t * h, const char * key);
	int (*close_obj)(son_t * h);
	int (*open_array)(son_t * h, const char * key);
	int (*close_array)(son_t * h);
	int (*open_data)(son_t * h, const char * key);
	int (*close_data)(son_t * h);
	int (*write_str)(son_t * h, const char * key, const char * v);
	int (*write_num)(son_t * h, const char * key, s32 v);
	int (*write_unum)(son_t * h, const char * key, u32 v);
	int (*write_float)(son_t * h, const char * key, float v);
	int (*write_true)(son_t * h, const char * key);
	int (*write_false)(son_t * h, const char * key);
	int (*write_null)(son_t * h, const char * key);
	int (*write_data)(son_t * h, const char * key, const void * v, son_size_t size);
	int (*write_open_data)(son_t * h, const void * v, son_size_t size);
	int (*read_str)(son_t * h, const char * access, char * str, son_size_t capacity);
	s32 (*read_num)(son_t * h, const char * access);
	u32 (*read_unum)(son_t * h, const char * access);
	float (*read_float)(son_t * h, const char * access);
	int (*read_data)(son_t * h, const char * access, void * data, son_size_t size);
	int (*read_bool)(son_t *h, const char * key);
	int (*seek)(son_t * h, const char * access, son_size_t * data_size);
	int (*edit)(son_t * h, const char * name);
	int (*edit_float)(son_t * h, const char * key, float v);
	int (*edit_data)(son_t * h, const char * key, const void * data, son_size_t size);
	int (*edit_str)(son_t * h, const char * key, const char *v);
	int (*edit_num)(son_t *h, const char * key, s32 v);
	int (*edit_unum)(son_t * h, const char * key, u32 v);
	int (*edit_bool)(son_t * h, const char * key, int v);
} son_api_t;

const son_api_t * son_api();


#ifdef __cplusplus
}
#endif


#endif /* SON_H_ */
