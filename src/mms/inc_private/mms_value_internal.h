/*
 *  mms_value_internal.h
 *
 *  Copyright 2013 Michael Zillgith
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

#ifndef MMS_VALUE_INTERNAL_H_
#define MMS_VALUE_INTERNAL_H_

#include "mms_value.h"
#include "ber_integer.h"

/**
 * @brief MMS值结构体（带紧凑打包属性）
 * 
 * 这是 libiec61850 库中最核心的数据结构之一，用于统一表示 MMS 协议中所有可能的数据类型值。
 * 它采用"类型-值"（Type-Value）的经典设计模式，通过 type 字段标明数据类型，通过联合体 value 存储具体数据。
 * 
 * 这个结构体被广泛应用于：
 *   - 读取/写入 IEC 61850 数据对象（DO）和数据属性（DA）
 *   - 构建和解析 MMS 协议报文
 *   - 表示 GOOSE/SV 报文中的数据集值
 *   - 文件服务中的数据传输
 * 
 * ATTRIBUTE_PACKED 确保结构体在内存中紧凑排列，没有字节对齐填充，这对于网络数据序列化至关重要。
 */
struct ATTRIBUTE_PACKED sMmsValue {
    MmsType type;
    uint8_t deleteValue;    // 内存释放标志
    union uMmsValue {
        MmsDataAccessError dataAccessError;
        struct {
            int size;
            MmsValue** components;
        } structure;
        bool boolean;
        Asn1PrimitiveValue* integer;
        struct {    // 浮点数
            uint8_t exponentWidth;
            uint8_t formatWidth; /* number of bits - either 32 or 64)  */
            uint8_t buf[8];
        } floatingPoint;
        struct {     // 八位位组串（Octet String）
            uint16_t size;
            int maxSize;
            uint8_t* buf;
        } octetString;
        struct {    // 位串
            int size;     /* Number of bits */
            uint8_t* buf;
        } bitString;
        struct {    // 可见字符串（Visible String）
            char* buf;
            int16_t size; /* size of the string, equals the amount of allocated memory - 1 */
        } visibleString;
        uint8_t utcTime[8];
        struct {
            uint8_t size;       // 时间数据的字节数（通常 4 或 6）
            uint8_t buf[6];     // 存储时间数据的缓冲区
        } binaryTime;
    } value;
};


LIB61850_INTERNAL MmsValue*
MmsValue_newIntegerFromBerInteger(Asn1PrimitiveValue* berInteger);

LIB61850_INTERNAL MmsValue*
MmsValue_newUnsignedFromBerInteger(Asn1PrimitiveValue* berInteger);

#endif /* MMS_VALUE_INTERNAL_H_ */
