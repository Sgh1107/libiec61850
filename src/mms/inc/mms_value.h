/*
 *  mms_value.h
 *
 *  Copyright 2013-2018 Michael Zillgith
 *
 *  This file is part of libIEC61850.
 *
 *  libIEC61850 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libIEC61850 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

#ifndef MMS_VALUE_H_
#define MMS_VALUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libiec61850_common_api.h"
#include "mms_common.h"
#include "mms_types.h"

typedef enum {
    DATA_ACCESS_ERROR_SUCCESS_NO_UPDATE = -3,
    DATA_ACCESS_ERROR_NO_RESPONSE = -2, /* for server internal purposes only! */
    DATA_ACCESS_ERROR_SUCCESS = -1,
    DATA_ACCESS_ERROR_OBJECT_INVALIDATED = 0,
    DATA_ACCESS_ERROR_HARDWARE_FAULT = 1,
    DATA_ACCESS_ERROR_TEMPORARILY_UNAVAILABLE = 2,
    DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED = 3,
    DATA_ACCESS_ERROR_OBJECT_UNDEFINED = 4,
    DATA_ACCESS_ERROR_INVALID_ADDRESS = 5,
    DATA_ACCESS_ERROR_TYPE_UNSUPPORTED = 6,
    DATA_ACCESS_ERROR_TYPE_INCONSISTENT = 7,
    DATA_ACCESS_ERROR_OBJECT_ATTRIBUTE_INCONSISTENT = 8,
    DATA_ACCESS_ERROR_OBJECT_ACCESS_UNSUPPORTED = 9,
    DATA_ACCESS_ERROR_OBJECT_NONE_EXISTENT = 10,
    DATA_ACCESS_ERROR_OBJECT_VALUE_INVALID = 11,
    DATA_ACCESS_ERROR_UNKNOWN = 12
} MmsDataAccessError;

/**
 * MmsValue - complex value type for MMS Client API
 */
typedef struct sMmsValue MmsValue;

/*************************************************************************************
 *  Array functions
 *************************************************************************************/
/**
 * @brief 创建一个数组，并用默认值初始化元素
 * @param elementType type description for the elements the new array
 * @param size the size of the new array
 */
LIB61850_API MmsValue* MmsValue_createArray(const MmsVariableSpecification* elementType, int size);

LIB61850_API uint32_t MmsValue_getArraySize(const MmsValue* self);

/**
 * @brief Get an element of an array or structure.
 * @param index ndex of the requested array or structure element
 */
LIB61850_API MmsValue* MmsValue_getElement(const MmsValue* array, int index);

/**
 * @brief 创建一个空数组
 */
LIB61850_API MmsValue* MmsValue_createEmptyArray(int size);

/**
 * @brief Set an element of a complex type
 * NOTE: If the element already exists it will simply be replaced by the provided new value.
 * The caller is responsible to free the replaced value.
 * @param complexValue MmsValue instance to operate on. Has to be of a type MMS_STRUCTURE or MMS_ARRAY
 * @param the index of the element to set/replace
 * @param elementValue the (new) value of the element
 */
LIB61850_API void MmsValue_setElement(MmsValue* complexValue, int index, MmsValue* elementValue);


////////////////////////////////////////////////////////////////
//              Basic type functions
////////////////////////////////////////////////////////////////
LIB61850_API MmsDataAccessError MmsValue_getDataAccessError(const MmsValue* self);

LIB61850_API int64_t MmsValue_toInt64(const MmsValue* self);

LIB61850_API int32_t MmsValue_toInt32(const MmsValue* value);

LIB61850_API uint32_t MmsValue_toUint32(const MmsValue* value);

LIB61850_API double MmsValue_toDouble(const MmsValue* self);

LIB61850_API float MmsValue_toFloat(const MmsValue* self);

LIB61850_API uint32_t MmsValue_toUnixTimestamp(const MmsValue* self);

LIB61850_API void MmsValue_setFloat(MmsValue* self, float newFloatValue);

LIB61850_API void MmsValue_setDouble(MmsValue* self, double newFloatValue);

LIB61850_API void MmsValue_setInt8(MmsValue* value, int8_t integer);

LIB61850_API void MmsValue_setInt16(MmsValue* value, int16_t integer);

LIB61850_API void MmsValue_setInt32(MmsValue* self, int32_t integer);

LIB61850_API void MmsValue_setInt64(MmsValue* value, int64_t integer);

LIB61850_API void MmsValue_setUint8(MmsValue* value, uint8_t integer);

LIB61850_API void MmsValue_setUint16(MmsValue* value, uint16_t integer);

LIB61850_API void MmsValue_setUint32(MmsValue* value, uint32_t integer);

LIB61850_API void MmsValue_setBoolean(MmsValue* value, bool boolValue);

LIB61850_API bool MmsValue_getBoolean(const MmsValue* value);

LIB61850_API const char* MmsValue_toString(MmsValue* self);

LIB61850_API int MmsValue_getStringSize(MmsValue* self);

LIB61850_API void MmsValue_setVisibleString(MmsValue* self, const char* string);


/**
 * @brief Set a single bit (set to one) of an MmsType object of type MMS_BITSTRING
 * @param self MmsValue instance to operate on. Has to be of a type MMS_BITSTRING.
 * @param bitPos the position of the bit in the bit string. Starting with 0. The bit
 *        with position 0 is the first bit if the MmsValue instance is serialized.
 * @param value the new value of the bit (true = 1 / false = 0)
 */
LIB61850_API void MmsValue_setBitStringBit(MmsValue* self, int bitPos, bool value);

/**
 * @brief 获取类型为 MMS_BITSTRING 的 MmsType 对象中单个位（设置为 1）的值
 * @param self MmsValue instance to operate on. Has to be of a type MMS_BITSTRING.
 * @param bitPos the position of the bit in the bit string. Starting with 0. The bit
 *        with position 0 is the first bit if the MmsValue instance is serialized.
 * @return the value of the bit (true = 1 / false = 0)
 */
LIB61850_API bool MmsValue_getBitStringBit(const MmsValue* self, int bitPos);

LIB61850_API void MmsValue_deleteAllBitStringBits(MmsValue* self);


/**
 * @brief Get the size of a bit string in bits.
 * @param self MmsValue instance to operate on. Has to be of a type MMS_BITSTRING.
 */
LIB61850_API int MmsValue_getBitStringSize(const MmsValue* self);

/**
 * @brief Get the number of bytes required by this bitString
 * @param self MmsValue instance to operate on. Has to be of a type MMS_BITSTRING.
 */
LIB61850_API int MmsValue_getBitStringByteSize(const MmsValue* self);

/**
 * @brief Count the number of set bits in a bit string.
 */
LIB61850_API int MmsValue_getNumberOfSetBits(const MmsValue* self);

/**
 * Set all bits (set to one) of an MmsType object of type MMS_BITSTRING
 */
LIB61850_API void MmsValue_setAllBitStringBits(MmsValue* self);

/**
 * @brief 将比特串转换为无符号整数(小端序)
 */
LIB61850_API uint32_t MmsValue_getBitStringAsInteger(const MmsValue* self);

/**
 * @brief Convert an unsigned integer to a bit string
 * The integer representation in the bit string assumes the first bit is the
 * least significant bit (little endian bit order).
 * @param self MmsValue instance to operate on. Has to be of a type MMS_BITSTRING.
 * @param intValue the integer value that is used to set the bit string
 */
LIB61850_API void MmsValue_setBitStringFromInteger(MmsValue* self, uint32_t intValue);

/**
 * @brief 将位串转换为无符号整数(大端序)
 */
LIB61850_API uint32_t MmsValue_getBitStringAsIntegerBigEndian(const MmsValue* self);

/**
 * @brief Convert an unsigned integer to a bit string (big endian bit order)
 * The integer representation in the bit string assumes the first bit is the
 * most significant bit (big endian bit order).
 * @param self MmsValue instance to operate on. Has to be of a type MMS_BITSTRING.
 * @param intValue the integer value that is used to set the bit string
 */
LIB61850_API void MmsValue_setBitStringFromIntegerBigEndian(MmsValue* self, uint32_t intValue);

/**
 * @brief Update an MmsValue object of UtcTime type with a timestamp
 * @param self MmsValue instance to operate on. Has to be of a type MMS_BOOLEAN.
 * @param timeval the new value in seconds since epoch (1970/01/01 00:00 UTC)
 */
LIB61850_API MmsValue* MmsValue_setUtcTime(MmsValue* self, uint32_t timeval);

/**
 * @brief Update an MmsValue object of type MMS_UTCTIME with a millisecond time.
 * @param self MmsValue instance to operate on. Has to be of a type MMS_UTCTIME.
 * @param timeval the new value in milliseconds since epoch (1970/01/01 00:00 UTC)
 */
LIB61850_API MmsValue* MmsValue_setUtcTimeMs(MmsValue* self, uint64_t timeval);

/**
 * @brief Update an MmsValue object of type MMS_UTCTIME with a buffer containing a BER encoded UTCTime.
 * The buffer must have a size of 8 bytes!
 * @param self MmsValue instance to operate on. Has to be of a type MMS_UTCTIME.
 * @param buffer buffer containing the encoded UTCTime.
 */
LIB61850_API void MmsValue_setUtcTimeByBuffer(MmsValue* self, const uint8_t* buffer);


/**
 * @brief return the raw buffer containing the UTC time data
 * Note: This will return the address of the raw byte buffer. The array length is 8 byte.
 * @param self MmsValue instance to operate on. Has to be of a type MMS_UTCTIME.
 * @return the buffer containing the raw data
 */
LIB61850_API uint8_t* MmsValue_getUtcTimeBuffer(MmsValue* self);

/**
 * @brief Get a millisecond time value from an MmsValue object of MMS_UTCTIME type.
 * @param self MmsValue instance to operate on. Has to be of a type MMS_UTCTIME.
 * @return the value in milliseconds since epoch (1970/01/01 00:00 UTC)
 */
LIB61850_API uint64_t MmsValue_getUtcTimeInMs(const MmsValue* value);


/**
 * @brief Get a millisecond time value and optional us part from an MmsValue object of MMS_UTCTIME type.
 * @param self MmsValue instance to operate on. Has to be of a type MMS_UTCTIME.
 * @param usec a pointer to store the us (microsecond) value.
 * @return the value in milliseconds since epoch (1970/01/01 00:00 UTC)
 */
LIB61850_API uint64_t MmsValue_getUtcTimeInMsWithUs(const MmsValue* self, uint32_t* usec);


/**
 * @brief set the TimeQuality byte of the UtcTime
 * Meaning of the bits in the timeQuality byte:
 * bit 7 = leapSecondsKnown
 * bit 6 = clockFailure
 * bit 5 = clockNotSynchronized
 * bit 0-4 = subsecond time accuracy (number of significant bits of subsecond time)
 * @param self MmsValue instance to operate on. Has to be of a type MMS_UTCTIME.
 * @param timeQuality the byte representing the time quality
 */
LIB61850_API void MmsValue_setUtcTimeQuality(MmsValue* self, uint8_t timeQuality);


/**
 * @brief Update an MmsValue object of type MMS_UTCTIME with a millisecond time. 
 * Meaning of the bits in the timeQuality byte:
 * bit 7 = leapSecondsKnown
 * bit 6 = clockFailure
 * bit 5 = clockNotSynchronized
 * bit 0-4 = subsecond time accuracy (number of significant bits of subsecond time)
 * @param self MmsValue instance to operate on. Has to be of a type MMS_UTCTIME.
 * @param timeval the new value in milliseconds since epoch (1970/01/01 00:00 UTC)
 * @param timeQuality the byte representing the time quality
 * @return the updated MmsValue instance
 */
LIB61850_API MmsValue* MmsValue_setUtcTimeMsEx(MmsValue* self, uint64_t timeval, uint8_t timeQuality);


/**
 * @brief get the TimeQuality byte of the UtcTime
 * Meaning of the bits in the timeQuality byte:
 * bit 7 = leapSecondsKnown
 * bit 6 = clockFailure
 * bit 5 = clockNotSynchronized
 * bit 0-4 = subsecond time accuracy (number of significant bits of subsecond time)
 * @param self MmsValue instance to operate on. Has to be of a type MMS_UTCTIME.
 * @return the byte representing the time quality
 */
LIB61850_API uint8_t MmsValue_getUtcTimeQuality(const MmsValue* self);


/**
 * @brief Update an MmsValue object of type MMS_BINARYTIME with a millisecond time.
 * @param self MmsValue instance to operate on. Has to be of a type MMS_UTCTIME.
 * @param timeval the new value in milliseconds since epoch (1970/01/01 00:00 UTC)
 */
LIB61850_API void MmsValue_setBinaryTime(MmsValue* self, uint64_t timestamp);

/**
 * @brief 从类型为 MMS_BINARYTIME 的 MmsValue 对象中获取毫秒时间值
 * @param self MmsValue instance to operate on. Has to be of a type MMS_BINARYTIME.
 * @return 自纪元（1970年1月1日 00:00 UTC）以来的毫秒数
 */
LIB61850_API uint64_t MmsValue_getBinaryTimeAsUtcMs(const MmsValue* self);

/**
 * @brief Set the value of an MmsValue object of type MMS_OCTET_STRING.
 * This method will copy the provided buffer to the internal buffer of the
 * MmsValue instance. This will only happen if the internal buffer size is large
 * enough for the new value. Otherwise the object value is not changed.
 * @param self MmsValue instance to operate on. Has to be of a type MMS_OCTET_STRING.
 * @param buf the buffer that contains the new value
 * @param size the size of the buffer that contains the new value
 */
LIB61850_API void MmsValue_setOctetString(MmsValue* self, const uint8_t* buf, int size);

/**
 * @brief Set a single octet of an MmsValue object of type MMS_OCTET_STRING.
 * This method will copy the provided octet to the internal buffer of the
 * MmsValue instance, at the 'octetPos' position. This will only happen
 * if the internal buffer size is large enough. Otherwise the object value is not changed.
 * @param self MmsValue instance to operate on. Has to be of a type MMS_OCTET_STRING.
 * @param octetPos the position of the octet in the octet string. Starting with 0.
 *        The octet with position 0 is the first octet if the MmsValue instance is serialized.
 * @param value the new value of the octet (0 to 255, or 0x00 to 0xFF)
 */
LIB61850_API void MmsValue_setOctetStringOctet(MmsValue* self, int octetPos, uint8_t value);

/**
 * @brief Returns the size in bytes of an MmsValue object of type MMS_OCTET_STRING.
 * NOTE: To access the byte in the buffer the function \ref MmsValue_getOctetStringBuffer
 * has to be used.
 * @param self MmsValue instance to operate on. Has to be of a type MMS_OCTET_STRING.
 * @return size in bytes
 */
LIB61850_API uint16_t MmsValue_getOctetStringSize(const MmsValue* self);

LIB61850_API uint16_t MmsValue_getOctetStringMaxSize(MmsValue* self);

LIB61850_API uint8_t* MmsValue_getOctetStringBuffer(MmsValue* self);

LIB61850_API uint8_t MmsValue_getOctetStringOctet(MmsValue* self, int octetPos);

LIB61850_API bool MmsValue_update(MmsValue* self, const MmsValue* source);

/**
 * @brief Check if two instances of MmsValue have the same value.
 * @return true if both instances are of the same type and have the same value
 */
LIB61850_API bool MmsValue_equals(const MmsValue* self, const MmsValue* otherValue);

/**
 * @brief Check if two (complex) instances of MmsValue have the same type.
 * @param self MmsValue instance to operate on.
 * @param otherValue MmsValue that is used to test
 * @return true if both instances and all their children are of the same type.
 */
LIB61850_API bool MmsValue_equalTypes(const MmsValue* self, const MmsValue* otherValue);



/*************************************************************************************
 * Constructors and destructors
 *************************************************************************************/
LIB61850_API MmsValue* MmsValue_newDataAccessError(MmsDataAccessError accessError);

LIB61850_API MmsValue* MmsValue_newInteger(int size);

LIB61850_API MmsValue* MmsValue_newUnsigned(int size);

LIB61850_API MmsValue* MmsValue_newBoolean(bool boolean);

/**
 * @brief Create a new MmsValue instance of type MMS_BITSTRING.
 * @param bitSize the size of the bit string in bit
 * @return new MmsValue instance of type MMS_BITSTRING
 */
LIB61850_API MmsValue* MmsValue_newBitString(int bitSize);

LIB61850_API MmsValue* MmsValue_newOctetString(int size, int maxSize);

LIB61850_API MmsValue* MmsValue_newStructure(const MmsVariableSpecification* typeSpec);

LIB61850_API MmsValue* MmsValue_createEmptyStructure(int size);

LIB61850_API MmsValue* MmsValue_newDefaultValue(const MmsVariableSpecification* typeSpec);

LIB61850_API MmsValue* MmsValue_newIntegerFromInt8(int8_t integer);

LIB61850_API MmsValue* MmsValue_newIntegerFromInt16(int16_t integer);

LIB61850_API MmsValue* MmsValue_newIntegerFromInt32(int32_t integer);

LIB61850_API MmsValue* MmsValue_newIntegerFromInt64(int64_t integer);

LIB61850_API MmsValue* MmsValue_newUnsignedFromUint32(uint32_t integer);

/**
 * @brief Create a new 32 bit wide float variable and initialize with value
 * @param value the initial value
 * @return new MmsValue instance of type MMS_FLOAT
 */
LIB61850_API MmsValue* MmsValue_newFloat(float value);

/**
 * @brief Create a new 64 bit wide float variable and initialize with value
 * @param value the initial value
 * @return new MmsValue instance of type MMS_FLOAT
 */
LIB61850_API MmsValue* MmsValue_newDouble(double value);

/**
 * @brief Create a (deep) copy of an MmsValue instance
 * 此操作将分配动态内存。调用者需要稍后调用MmsValue_delete()来释放此内存
 * @param self the MmsValue instance that will be cloned
 * @return 一个 MmsValue 实例，它是给定实例的精确副本
 */
LIB61850_API MmsValue* MmsValue_clone(const MmsValue* self);


/**
 * @brief Create a (deep) copy of an MmsValue instance in a user provided buffer
 * 此操作会将给定的 MmsValue 实例复制到用户提供的缓冲区.
 * @param self the MmsValue instance that will be cloned
 * @param destinationAddress the start address of the user provided buffer
 * @return 指向缓冲区中最后一个写入字节之后的位置的指针
 */
LIB61850_API uint8_t* MmsValue_cloneToBuffer(const MmsValue* self, uint8_t* destinationAddress);


/**
 * @brief Determine the required amount of bytes by a clone.
 * This function is intended to be used to determine the buffer size of a clone operation
 * (MmsValue_cloneToBuffer) in advance.
 * @param self the MmsValue instance
 * @return the number of bytes required by a clone
 */
LIB61850_API int MmsValue_getSizeInMemory(const MmsValue* self);


/**
 * @brief Delete an MmsValue instance.
 *此操作会释放 MmsValue 实例的所有动态分配内存如果实例类型为 MMS_STRUCTURE 或 MMS_ARRAY，则所有子元素也将被删除
 */
LIB61850_API void MmsValue_delete(MmsValue* self);

LIB61850_API void MmsValue_deleteConditional(MmsValue* value);

/**
 * @brief Create a new MmsValue instance of type MMS_VISIBLE_STRING.
 * This function will allocate as much memory as required to hold the string and sets the maximum size of
 * the string to this size.
 * @param string a text string that should be the value of the new instance of NULL for an empty string.
 * @return new MmsValue instance of type MMS_VISIBLE_STRING
 */
LIB61850_API MmsValue* MmsValue_newVisibleString(const char* string);

/**
 * @brief Create a new MmsValue instance of type MMS_VISIBLE_STRING.
 * This function will create a new empty MmsValue string object. The maximum size of the string is set
 * according to the size parameter. The function allocates as much memory as is required to hold a string
 * of the maximum size.
 * @param size the new maximum size of the string.
 * @return new MmsValue instance of type MMS_VISIBLE_STRING
 */
LIB61850_API MmsValue* MmsValue_newVisibleStringWithSize(int size);

/**
 * @brief Create a new MmsValue instance of type MMS_STRING.
 * This function will create a new empty MmsValue string object. The maximum size of the string is set
 * according to the size parameter. The function allocates as much memory as is required to hold a string
 * of the maximum size.
 * @param size the new maximum size of the string.
 * @return new MmsValue instance of type MMS_STRING
 */
LIB61850_API MmsValue* MmsValue_newMmsStringWithSize(int size);

/**
 * @brief Create a new MmsValue instance of type MMS_BINARYTIME.
 * If the timeOfDay parameter is set to true then the resulting
 * MMS_BINARYTIME object is only 4 octets long and includes only
 * the seconds since midnight. Otherwise the MMS_BINARYTIME
 * @param timeOfDay if true only the TimeOfDay value is included.
 * @return new MmsValue instance of type MMS_BINARYTIME
 */
LIB61850_API MmsValue* MmsValue_newBinaryTime(bool timeOfDay);

/**
 * @brief Create a new MmsValue instance of type MMS_VISIBLE_STRING from the specified byte array
 * @param byteArray the byte array containing the string data
 * @param size the size of the byte array
 * @return new MmsValue instance of type MMS_VISIBLE_STRING
 */
LIB61850_API MmsValue* MmsValue_newVisibleStringFromByteArray(const uint8_t* byteArray, int size);

/**
 * @brief Create a new MmsValue instance of type MMS_STRING from the specified byte array
 * @param byteArray the byte array containing the string data
 * @param size the size of the byte array
 * @return new MmsValue instance of type MMS_STRING
 */
LIB61850_API MmsValue* MmsValue_newMmsStringFromByteArray(const uint8_t* byteArray, int size);

/**
 * @brief Create a new MmsValue instance of type MMS_STRING.
 * @param string a text string that should be the value of the new instance of NULL for an empty string.
 * @return new MmsValue instance of type MMS_STRING
 */
LIB61850_API MmsValue* MmsValue_newMmsString(const char* string);

/**
 * @brief Set the value of MmsValue instance of type MMS_STRING
 * @param string a text string that will be the new value of the instance
 */
LIB61850_API void MmsValue_setMmsString(MmsValue* value, const char* string);

/**
 * @brief Create a new MmsValue instance of type MMS_UTCTIME.
 * @param timeval time value as UNIX timestamp (seconds since epoch)
 * @return new MmsValue instance of type MMS_UTCTIME
 */
LIB61850_API MmsValue* MmsValue_newUtcTime(uint32_t timeval);

/**
 * @brief Create a new MmsValue instance of type MMS_UTCTIME.
 * @param timeval time value as millisecond timestamp (milliseconds since epoch)
 * @return new MmsValue instance of type MMS_UTCTIME
 */
LIB61850_API MmsValue* MmsValue_newUtcTimeByMsTime(uint64_t timeval);

LIB61850_API void MmsValue_setDeletable(MmsValue* self);
LIB61850_API void MmsValue_setDeletableRecursive(MmsValue* value);
LIB61850_API int MmsValue_isDeletable(MmsValue* self);
LIB61850_API MmsType MmsValue_getType(const MmsValue* self);

/**
 * @brief Get a sub-element of a MMS_STRUCTURE value specified by a path name.
 * @param self the MmsValue instance
 * @param varSpec - type specification if the MMS_STRUCTURE value
 * @param mmsPath - path (in MMS variable name syntax) to specify the sub element.
 * @return the sub elements MmsValue instance or NULL if the element does not exist
 */
LIB61850_API MmsValue*
MmsValue_getSubElement(MmsValue* self, MmsVariableSpecification* varSpec, char* mmsPath);

LIB61850_API const char* MmsValue_getTypeString(MmsValue* self);

/**
 * @brief create a string representation of the MmsValue object in the provided buffer
 * NOTE: This function is for debugging purposes only. It may not be aimed to be used
 * in embedded systems. It requires a full featured snprintf function.
 * @param self the MmsValue instance
 * @param buffer the buffer where to copy the string representation
 * @param bufferSize the size of the provided buffer
 * @return a pointer to the start of the buffer
 */
LIB61850_API const char* MmsValue_printToBuffer(const MmsValue* self, char* buffer, int bufferSize);

/**
 * @brief 从 BER 编码的 MMS 数据元素创建新的 MmsValue 实例（反序列化）
 * WARNING: API 在 1.0.3 版本中已更改（添加了 endBufPos 参数）
 * @param buffer 要读取的缓冲区
 * @param bufPos 缓冲区中mms值数据的起始位置
 * @param bufferLength 缓冲区长度
 * @param endBufPos 读取 MMS 数据元素后缓冲区中的位置（如果不需要则为 NULL）
 * @return 从缓冲区创建的 MmsValue 实例
 */
LIB61850_API MmsValue* MmsValue_decodeMmsData(uint8_t* buffer, int bufPos, int bufferLength, int* endBufPos);

/**
 * @brief 使用定义的最大递归深度，从 BER 编码的 MMS 数据元素（反序列化）创建一个新的 MmsValue 实例
 * @param buffer the buffer to read from
 * @param bufPos the start position of the mms value data in the buffer
 * @param bufferLength the length of the buffer
 * @param endBufPos the position in the buffer after the read MMS data element (NULL if not required)
 * @param maxDepth the maximum recursion depth
 * @return the MmsValue instance created from the buffer
 */
LIB61850_API MmsValue*
MmsValue_decodeMmsDataMaxRecursion(uint8_t* buffer, int bufPos, int bufferLength, int* endBufPos, int maxDepth);

/**
 * @brief 将 MmsValue 实例序列化为 BER 编码的 MMS 数据元素
 * @param self the MmsValue instance
 * @param buffer the buffer to encode the MMS data element
 * @param bufPos 缓冲区中开始编码的位置
 * @param encode 编码到缓冲区（true）或仅计算长度（false）
 * @return 相应 MMS 数据元素的编码长度
 */
LIB61850_API int MmsValue_encodeMmsData(MmsValue* self, uint8_t* buffer, int bufPos, bool encode);


/**
 * @brief Get the maximum possible BER encoded size of the MMS data element
 * @return the maximum encoded size in bytes of the MMS data element
 */
LIB61850_API int MmsValue_getMaxEncodedSize(MmsValue* self);

/**
 * @brief Calculate the maximum encoded size of a variable of this type
 * @param self the MMS variable specification instance
 */
LIB61850_API int MmsVariableSpecification_getMaxEncodedSize(MmsVariableSpecification* self);

LIB61850_API const char* MmsError_toString(MmsError err);

#ifdef __cplusplus
}
#endif

#endif /* MMS_VALUE_H_ */
