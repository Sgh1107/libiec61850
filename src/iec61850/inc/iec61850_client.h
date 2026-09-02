/*
 *  iec61850_client.h
 *
 *  Copyright 2013-2023 Michael Zillgith
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

#ifndef IEC61850_CLIENT_H_
#define IEC61850_CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libiec61850_common_api.h"
#include "iec61850_common.h"
#include "mms_value.h"
#include "mms_client_connection.h"
#include "linked_list.h"

/** an opaque handle to the instance data of a ClientDataSet object */
typedef struct sClientDataSet* ClientDataSet;

/** an opaque handle to the instance data of a ClientReport object */
typedef struct sClientReport* ClientReport;

/** an opaque handle to the instance data of a ClientReportControlBlock object */
typedef struct sClientReportControlBlock* ClientReportControlBlock;

/** an opaque handle to the instance data of a ClientGooseControlBlock object */
typedef struct sClientGooseControlBlock* ClientGooseControlBlock;

/** An opaque handle to the instance data of the IedConnection object */
typedef struct sIedConnection* IedConnection;

/** Detailed description of the last application error of the client connection instance */
typedef struct
{
    int ctlNum;
    ControlLastApplError error;
    ControlAddCause addCause;
} LastApplError;

/** IedConnection实例的连接状态 - 包括closed(idle)、connecting、connected或closing */
typedef enum
{
    IED_STATE_CLOSED = 0,
    IED_STATE_CONNECTING,
    IED_STATE_CONNECTED,
    IED_STATE_CLOSING
} IedConnectionState;

/**
 * 用于描述大多数客户端侧服务函数的错误原因
 * 这个枚举类型定义了 IEC 61850 客户端在调用服务函数时可能返回的所有错误码
 */
typedef enum {
    /* ===== 通用错误（0-9） ===== */
    /** 没有发生错误 - 服务请求已成功执行 */
    IED_ERROR_OK = 0,

    /** 服务请求无法执行，因为客户端尚未建立连接 */
    IED_ERROR_NOT_CONNECTED = 1,

    /** 连接服务无法执行，因为客户端已经处于连接状态 */
    IED_ERROR_ALREADY_CONNECTED = 2,

    /** 由于连接丢失，服务请求无法执行 */
    IED_ERROR_CONNECTION_LOST = 3,

    /** 客户端协议栈或服务器不支持该服务或某些给定的参数 */
    IED_ERROR_SERVICE_NOT_SUPPORTED = 4,

    /** 连接被服务器拒绝 */
    IED_ERROR_CONNECTION_REJECTED = 5,

    /** 无法发送请求，因为已达到未完成调用数上限 */
    IED_ERROR_OUTSTANDING_CALL_LIMIT_REACHED = 6,

    /* ===== 客户端侧错误（10-19） ===== */
    /** API 函数被调用时传入了无效参数 */
    IED_ERROR_USER_PROVIDED_INVALID_ARGUMENT = 10,

    /** 启用报告失败，原因是数据集不匹配 */
    IED_ERROR_ENABLE_REPORT_FAILED_DATASET_MISMATCH = 11,

    /** 提供的对象引用无效（存在语法错误） */
    IED_ERROR_OBJECT_REFERENCE_INVALID = 12,

    /** 接收到的对象类型不符合预期 */
    IED_ERROR_UNEXPECTED_VALUE_RECEIVED = 13,

    /* ===== 服务端错误 - 由服务器报告（20-97） ===== */
    /** 与服务器的通信超时失败 */
    IED_ERROR_TIMEOUT = 20,

    /** 由于访问控制限制，服务器拒绝了对请求对象/服务的访问 */
    IED_ERROR_ACCESS_DENIED = 21,

    /** 服务器报告请求的对象不存在（由服务器返回） */
    IED_ERROR_OBJECT_DOES_NOT_EXIST = 22,

    /** 服务器报告请求的对象已经存在 */
    IED_ERROR_OBJECT_EXISTS = 23,

    /** 服务器不支持请求的访问方法（由服务器返回） */
    IED_ERROR_OBJECT_ACCESS_UNSUPPORTED = 24,

    /** 服务器期望的对象类型与收到的类型不一致（由服务器返回） */
    IED_ERROR_TYPE_INCONSISTENT = 25,

    /** 对象或服务暂时不可用（由服务器返回） */
    IED_ERROR_TEMPORARILY_UNAVAILABLE = 26,

    /** 指定的对象在服务器中未定义（由服务器返回） */
    IED_ERROR_OBJECT_UNDEFINED = 27,

    /** 指定的地址无效（由服务器返回） */
    IED_ERROR_INVALID_ADDRESS = 28,

    /** 由于硬件故障，服务执行失败（由服务器返回） */
    IED_ERROR_HARDWARE_FAULT = 29,

    /** 服务器不支持请求的数据类型（由服务器返回） */
    IED_ERROR_TYPE_UNSUPPORTED = 30,

    /** 提供的属性不一致（由服务器返回） */
    IED_ERROR_OBJECT_ATTRIBUTE_INCONSISTENT = 31,

    /** 提供的对象值无效（由服务器返回） */
    IED_ERROR_OBJECT_VALUE_INVALID = 32,

    /** 对象已失效（由服务器返回） */
    IED_ERROR_OBJECT_INVALIDATED = 33,

    /** 从服务器接收到格式无效的响应消息 */
    IED_ERROR_MALFORMED_MESSAGE = 34,

    /** 由于所需资源仍在使用中，服务未能执行 */
    IED_ERROR_OBJECT_CONSTRAINT_CONFLICT = 35,

    /* ===== 其他错误（98-99） ===== */
    /** 服务未实现 */
    IED_ERROR_SERVICE_NOT_IMPLEMENTED = 98,

    /** 未知错误 */
    IED_ERROR_UNKNOWN = 99
} IedClientError;

LIB61850_API const char* IedClientError_toString(IedClientError err);

///////////////////////////////////////////////////////////////////
//             Connection creation and destruction
///////////////////////////////////////////////////////////////////
/**
 * @brief 创建一个新的 IedConnection 实例
 * 此函数创建一个新的 IedConnection 实例，用于处理与 IED 的连接。
 * 它已分配所有必需资源。新连接处于“CLOSED”状态。必须先调用 connect 方法才能使用它。
 * 该连接将采用非 TLS 线程模式
 * @return the new IedConnection instance
 */
LIB61850_API IedConnection IedConnection_create(void);

/**
 * @brief 创建一个新的 IedConnection 实例（扩展版本）
 * 此函数创建一个新的 IedConnection 实例，用于处理与 IED 的连接。它已分配所有必需资源。
 * 新连接处于“CLOSED”状态。必须先调用IedConnection_connect或IedConnection_connectAsync方法才能使用它。
 * 如果提供了 TLSConfiguration 对象，则连接将使用 TLS。
 * 如果 useThread 为 false，则 IedConnection 处于非线程模式，必须定期调用 IedConnection_tick 函数来接收消息并执行维护任务。
 * @param tlsConfig the TLS configuration to be used, or NULL for non-TLS connection
 * @param useThreads when true, the IedConnection is in thread mode
 * @return the new IedConnection instance
 */
LIB61850_API IedConnection IedConnection_createEx(TLSConfiguration tlsConfig, bool useThreads);

/**
 * @brief 创建一个新的支持 TLS 的 IedConnection 实例
 * 此函数创建一个新的 IedConnection 实例，用于处理与 IED 的连接。
 * 它已分配所有必需资源。新连接处于“CLOSED”状态。
 * 必须先调用IedConnection_connect或IedConnection_connectAsync方法才能使用它。
 * 如果提供了 TLSConfiguration 对象，则连接将使用 TLS。连接将以线程模式运行
 * @deprecated Use \ref IedConnection_createEx instead
 * @param tlsConfig the TLS configuration to be used
 * @return the new IedConnection instance
 */
LIB61850_API IedConnection IedConnection_createWithTlsSupport(TLSConfiguration tlsConfig);

/**
 * @brief destroy an IedConnection instance.
 * The connection will be closed if it is in "connected" state. All allocated resources of the connection
 * will be freed.
 * @param self the connection object
 */
LIB61850_API void IedConnection_destroy(IedConnection self);

/**
* @brief 设置客户端使用的本地 IP 地址和端口
* NOTE: This function is optional. When not used the OS decides what IP address and TCP port to use.
* @param self IedConnection instance
* @param localIpAddress the local IP address or hostname as C string
* @param localPort the local TCP port to use. When < 1 the OS will chose the TCP port to use.
*/
LIB61850_API void IedConnection_setLocalAddress(IedConnection self, const char* localIpAddress, int localPort);


/**
 * @brief 设置连接超时时间毫秒
 * 设置此连接的超时时间。此函数必须在调用 IedConnection_connect 之前调用
 * @param self the connection object
 * @param timoutInMs the connection timeout in ms
 */
LIB61850_API void IedConnection_setConnectTimeout(IedConnection self, uint32_t timeoutInMs);


/**
 * @brief Set the maximum number outstanding calls allowed for this connection
 * @param self the connection object
 * @param calling the maximum outstanding calls allowed by the caller (client)
 * @param called the maximum outstanding calls allowed by the called endpoint (server)
 */
LIB61850_API void IedConnection_setMaxOutstandingCalls(IedConnection self, int calling, int called);


/**
 * @brief 设置请求超时时间（毫秒）
 * Set the request timeout for this connection. You can call this function any time to adjust
 * timeout behavior.
 * @param self the connection object
 * @param timoutInMs the connection timeout in ms
 */
LIB61850_API void IedConnection_setRequestTimeout(IedConnection self, uint32_t timeoutInMs);


/**
 * @brief 获取此连接的请求超时时间（以毫秒为单位）
 */
LIB61850_API uint32_t IedConnection_getRequestTimeout(IedConnection self);


/**
 * @brief 设置此 IedConnection 实例生成的所有时间戳的时间质量
 * @param self the connection object
 * @param leapSecondKnown set/unset leap seconds known flag
 * @param clockFailure set/unset clock failure flag
 * @param clockNotSynchronized set/unset clock not synchronized flag
 * @param subsecondPrecision set the subsecond precision (number of significant bits of the fractionOfSecond part of the time stamp)
 */
LIB61850_API void
IedConnection_setTimeQuality(IedConnection self, bool leapSecondKnown, bool clockFailure, 
                             bool clockNotSynchronized, int subsecondPrecision);


/**
 * @brief 执行 MMS message 处理和维护任务（仅限非线程模式）
 * 用户应用程序必须在非线程模式下定期调用此函数。返回值有助于判断堆栈何时空闲，从而可以执行其他任务。
 * NOTE: 使用非线程模式时，不应使用同步（阻塞）API 函数。如果未在单独的线程中调用 IedConnection_tick，同步函数将无限期阻塞。
 * @return true when connection is currently waiting and calling thread can be suspended, false means
 *         connection is busy and the tick function should be called again as soon as possible.
 */
LIB61850_API bool IedConnection_tick(IedConnection self);


/**
 * @brief 通用服务回调处理程序
 * NOTE: 此回调处理程序被多个异步服务函数使用，这些函数只需要以成功（IED_ERROR_OK）或失败的形式提供简单的反馈
 * @param invokeId 相关服务请求使用的调用 ID
 * @param parameter user provided parameter
 * @param err the result code. IED_ERROR_OK indicates success.
 */
typedef void (*IedConnection_GenericServiceHandler) (uint32_t invokeId, void* parameter, IedClientError err);

/////////////////////////////////////////////////////////////////////////
//                       Association service
/////////////////////////////////////////////////////////////////////////
/**
 * @brief Connect to a server
 * NOTE: Function will block until connection is up or timeout happened.
 * @param self the connection object
 * @param error the error code if an error occurs
 * @param hostname the host name or IP address of the server to connect to
 * @param tcpPort the TCP port number of the server to connect to
 */
LIB61850_API void IedConnection_connect(IedConnection self, IedClientError* error, const char* hostname, int tcpPort);


/**
 * @brief 异步连接到服务器
 * 该函数会立即返回。没有错误并不表示连接已建立。
 * 必须通过轮询 IedConnection_getState 函数或使用 IedConnection_StateChangedHandler 来跟踪当前的连接状态
 * @param self the connection object
 * @param error the error code if an error occurs
 * @param hostname the host name or IP address of the server to connect to
 * @param tcpPort the TCP port number of the server to connect to
 */
LIB61850_API void IedConnection_connectAsync(IedConnection self, IedClientError* error, const char* hostname, int tcpPort);

/**
 * @brief 中止连接
 * 这将通过向服务器发送 ACSE 中止消息来关闭 MMS 关联。
 * 发送中止消息后，连接将立即关闭。客户端可以在函数返回时假定连接已关闭，并可以调用销毁方法。
 * 如果连接未处于“已连接”状态，则会报告 IED_ERROR_NOT_CONNECTED 错误
 * @param self the connection object
 * @param error the error code if an error occurs
 */
LIB61850_API void IedConnection_abort(IedConnection self, IedClientError* error);
LIB61850_API void IedConnection_abortAsync(IedConnection self, IedClientError* error);

/**
 * @brief Release the connection
 * This will release the MMS association by sending an MMS conclude message to the server.
 * The client can NOT assume the connection to be closed when the function returns, It can
 * also fail if the server returns with a negative response. To be sure that the connection
 * will be close the close or abort methods should be used. If the connection is not in "connected" state an
 * IED_ERROR_NOT_CONNECTED error will be reported.
 * @param self the connection object
 * @param error the error code if an error occurs
 */
LIB61850_API void IedConnection_release(IedConnection self, IedClientError* error);
LIB61850_API void IedConnection_releaseAsync(IedConnection self, IedClientError* error);

LIB61850_API void IedConnection_close(IedConnection self);

/**
 * @brief return the state of the connection.
 * This function can be used to determine if the connection is established or closed.
 * @param self the connection object
 * @return the connection state
 */
LIB61850_API IedConnectionState IedConnection_getState(IedConnection self);

/**
 * @brief Access to last application error received by the client connection
 * @param self the connection object
 * @return the LastApplError value
 */
LIB61850_API LastApplError IedConnection_getLastApplError(IedConnection self);


/**
 * @brief 连接关闭时调用的回调处理程序
 * @deprecated Use \ref IedConnection_StateChangedHandler instead
 * @param user provided parameter
 * @param connection the connection object of the closed connection
 */
typedef void (*IedConnectionClosedHandler) (void* parameter, IedConnection connection);


/**
 * @brief 安装一个处理函数，当连接丢失/关闭时调用该函数
 * @deprecated Use \ref IedConnection_StateChangedHandler instead
 * @param self the connection object
 * @param handler that callback function
 * @param parameter the user provided parameter that is handed over to the callback function
 */
LIB61850_API void
IedConnection_installConnectionClosedHandler(IedConnection self, IedConnectionClosedHandler handler, void* parameter);


/**
 * @brief 当连接状态 ( IedConnectionState ) 改变时，将调用回调处理程序
 * @param user provided parameter
 * @param connection the related connection
 * @param newState the new state of the connection
 */
typedef void
(*IedConnection_StateChangedHandler) (void* parameter, IedConnection connection, IedConnectionState newState);


/**
 * @brief Install a handler function that is called when the connection state changes
 * @param self the connection object
 * @param handler that callback function
 * @param parameter the user provided parameter that is handed over to the callback function
 */
LIB61850_API void
IedConnection_installStateChangedHandler(IedConnection self, IedConnection_StateChangedHandler handler, void* parameter);


/**
 * @brief 获取底层 MmsConnection 的句柄
 * 获取此 IedConnection 使用的底层 MmsConnection 实例的访问权限。
 * 这可用于设置/更改特定的 MmsConnection 参数或调用底层 MMS 服务/函数
 * @return 此 IedConnection 使用的 MmsConnection 实例
 */
LIB61850_API MmsConnection IedConnection_getMmsConnection(IedConnection self);

/** SV ASDU contains attribute RefrTm */
#define IEC61850_SV_OPT_REFRESH_TIME 1

/** SV ASDU contains attribute SmpSynch */
#define IEC61850_SV_OPT_SAMPLE_SYNC 2

/** SV ASDU contains attribute SmpRate */
#define IEC61850_SV_OPT_SAMPLE_RATE 4

/** SV ASDU contains attribute DatSet */
#define IEC61850_SV_OPT_DATA_SET 8

/** SV ASDU contains attribute Security */
#define IEC61850_SV_OPT_SECURITY 16

#define IEC61850_SV_SMPMOD_SAMPLES_PER_PERIOD 0

#define IEC61850_SV_SMPMOD_SAMPLES_PER_SECOND 1

#define IEC61850_SV_SMPMOD_SECONDS_PER_SAMPLE 2

/** an opaque handle to the instance data of a ClientSVControlBlock object */
typedef struct sClientSVControlBlock* ClientSVControlBlock;


/**
 * @brief Create a new ClientSVControlBlock instance
 * This function simplifies client side access to server MSV/USV control blocks
 * NOTE: Do not use the functions after the IedConnection object is invalidated!
 * The access functions cause synchronous read/write calls to the server. For asynchronous
 * access use the \ref IedConnection_readObjectAsync and \ref IedConnection_writeObjectAsync
 * functions.
 * @param connection the IedConnection object with a valid connection to the server.
 * @param reference the object reference of the control block
 * @return the new instance
 */
LIB61850_API ClientSVControlBlock ClientSVControlBlock_create(IedConnection connection, const char* reference);


/**
 * @brief Free all resources related to the ClientSVControlBlock instance.
 * @param self the ClientSVControlBlock instance to operate on
 */
LIB61850_API void ClientSVControlBlock_destroy(ClientSVControlBlock self);

/**
 * @brief Test if this SVCB is multicast
 * @param self the ClientSVControlBlock instance to operate on
 * @return true if multicast SCVB, false otherwise (unicast)
 */
LIB61850_API bool ClientSVControlBlock_isMulticast(ClientSVControlBlock self);

/**
 * @brief Return the error code of the last write or write acccess to the SVCB
 * @param self the ClientSVControlBlock instance to operate on
 * @return the error code of the last read or write access
 */
LIB61850_API IedClientError ClientSVControlBlock_getLastComError(ClientSVControlBlock self);


LIB61850_API bool ClientSVControlBlock_setSvEna(ClientSVControlBlock self, bool value);
LIB61850_API bool ClientSVControlBlock_getSvEna(ClientSVControlBlock self);
LIB61850_API bool ClientSVControlBlock_setResv(ClientSVControlBlock self, bool value);
LIB61850_API bool ClientSVControlBlock_getResv(ClientSVControlBlock self);
LIB61850_API char* ClientSVControlBlock_getMsvID(ClientSVControlBlock self);

/**
 * @brief Get the (MMS) reference to the data set
 * NOTE: the returned string is dynamically allocated with the
 * GLOBAL_MALLOC macro. The application is responsible to release
 * the memory when the string is no longer needed.
 * @param self the ClientSVControlBlock instance to operate on
 * @return the data set reference as a NULL terminated string
 */
LIB61850_API char* ClientSVControlBlock_getDatSet(ClientSVControlBlock self);
LIB61850_API uint32_t ClientSVControlBlock_getConfRev(ClientSVControlBlock self);
LIB61850_API uint16_t ClientSVControlBlock_getSmpRate(ClientSVControlBlock self);


/**
 * @brief returns the destination address of the SV publisher
 * @param self the ClientSVControlBlock instance to operate on
 */
LIB61850_API PhyComAddress ClientSVControlBlock_getDstAddress(ClientSVControlBlock self);


/**
 * @brief Gets the OptFlds parameter of the RCB (decides what information to include in a report)
 * @param self the RCB instance
 * @return bit field representing the optional fields of a report (uses flags from \ref REPORT_OPTIONS)
 */
LIB61850_API int ClientSVControlBlock_getOptFlds(ClientSVControlBlock self);

/**
 * @brief returns number of sample mode of the SV publisher
 * @param self the ClientSVControlBlock instance to operate on
 */
LIB61850_API uint8_t ClientSVControlBlock_getSmpMod(ClientSVControlBlock self);

/**
 * @brief returns number of ASDUs included in the SV message
 * @param self the ClientSVControlBlock instance to operate on
 * @return the number of ASDU included in a single SV message
 */
LIB61850_API int ClientSVControlBlock_getNoASDU(ClientSVControlBlock self);


////////////////////////////////////////////////////////////////
//         GOOSE services handling (MMS part)
////////////////////////////////////////////////////////////////
/** Enable GOOSE publisher GoCB block element */
#define GOCB_ELEMENT_GO_ENA       1

/** GOOSE ID GoCB block element */
#define GOCB_ELEMENT_GO_ID        2

/** Data set GoCB block element */
#define GOCB_ELEMENT_DATSET       4

/** Configuration revision GoCB block element (this is usually read-only) */
#define GOCB_ELEMENT_CONF_REV     8

/** Need commission GoCB block element (read-only according to 61850-7-2) */
#define GOCB_ELEMENT_NDS_COMM    16

/** Destination address GoCB block element (read-only according to 61850-7-2) */
#define GOCB_ELEMENT_DST_ADDRESS 32

/** Minimum time GoCB block element (read-only according to 61850-7-2) */
#define GOCB_ELEMENT_MIN_TIME    64

/** Maximum time GoCB block element (read-only according to 61850-7-2) */
#define GOCB_ELEMENT_MAX_TIME   128

/** Fixed offsets GoCB block element (read-only according to 61850-7-2) */
#define GOCB_ELEMENT_FIXED_OFFS 256

/** select all elements of the GoCB */
#define GOCB_ELEMENT_ALL        511


//////////////////////////////////////////////////////////////////
//              ClientGooseControlBlock class
//////////////////////////////////////////////////////////////////

LIB61850_API ClientGooseControlBlock ClientGooseControlBlock_create(const char* dataAttributeReference);

LIB61850_API void ClientGooseControlBlock_destroy(ClientGooseControlBlock self);

LIB61850_API bool ClientGooseControlBlock_getGoEna(ClientGooseControlBlock self);

LIB61850_API void ClientGooseControlBlock_setGoEna(ClientGooseControlBlock self, bool goEna);

LIB61850_API const char* ClientGooseControlBlock_getGoID(ClientGooseControlBlock self);

LIB61850_API void ClientGooseControlBlock_setGoID(ClientGooseControlBlock self, const char* goID);

LIB61850_API const char* ClientGooseControlBlock_getDatSet(ClientGooseControlBlock self);

LIB61850_API void ClientGooseControlBlock_setDatSet(ClientGooseControlBlock self, const char* datSet);

LIB61850_API uint32_t ClientGooseControlBlock_getConfRev(ClientGooseControlBlock self);

LIB61850_API bool ClientGooseControlBlock_getNdsComm(ClientGooseControlBlock self);

LIB61850_API uint32_t ClientGooseControlBlock_getMinTime(ClientGooseControlBlock self);

LIB61850_API uint32_t ClientGooseControlBlock_getMaxTime(ClientGooseControlBlock self);

LIB61850_API bool ClientGooseControlBlock_getFixedOffs(ClientGooseControlBlock self);

LIB61850_API PhyComAddress ClientGooseControlBlock_getDstAddress(ClientGooseControlBlock self);

LIB61850_API void ClientGooseControlBlock_setDstAddress(ClientGooseControlBlock self, PhyComAddress value);

LIB61850_API DEPRECATED MmsValue* /* MMS_OCTET_STRING */
ClientGooseControlBlock_getDstAddress_addr(ClientGooseControlBlock self);

LIB61850_API DEPRECATED void
ClientGooseControlBlock_setDstAddress_addr(ClientGooseControlBlock self, MmsValue* macAddr);

LIB61850_API DEPRECATED uint8_t
ClientGooseControlBlock_getDstAddress_priority(ClientGooseControlBlock self);

LIB61850_API DEPRECATED void
ClientGooseControlBlock_setDstAddress_priority(ClientGooseControlBlock self, uint8_t priorityValue);

LIB61850_API DEPRECATED uint16_t
ClientGooseControlBlock_getDstAddress_vid(ClientGooseControlBlock self);

LIB61850_API DEPRECATED void
ClientGooseControlBlock_setDstAddress_vid(ClientGooseControlBlock self, uint16_t vidValue);

LIB61850_API DEPRECATED uint16_t
ClientGooseControlBlock_getDstAddress_appid(ClientGooseControlBlock self);

LIB61850_API DEPRECATED void
ClientGooseControlBlock_setDstAddress_appid(ClientGooseControlBlock self, uint16_t appidValue);


//////////////////////////////////////////////////////////////////////////////
//        GOOSE services (access to GOOSE Control Blocks (GoCB))
//////////////////////////////////////////////////////////////////////////////
/**
 * @brief 读取连接服务器上 GOOSE 控制块 (GoCB) 的属性
 * GoCB 包含单个 GOOSE 发布者的配置值, 所请求的 GoCB 必须通过其对象 IEC 61850 ACSI 对象参考来指定 E.g.
 * "simpleIOGernericIO/LLN0.gcbEvents"
 * 此函数用于执行 GoCB 值的实际读取服务。要访问接收到的值，必须使用 ClientGooseControlBlock 的函数
 *
 * 如果使用 NULL 参数调用 `updateGoCB` 函数，则会创建一个新的 `ClientGooseControlBlock` 实例，
 * 并用服务器接收到的值填充该实例。用户需要在不再需要该对象时调用 `ClientGooseControlBlock_destroy` 函数来释放它。
 * 如果使用对现有 `ClientGooseControlBlock` 实例的引用调用该函数，则会更新属性值，而不会创建新实例
 * Note: This function maps to a single MMS read request to retrieve the complete GoCB at once.
 * @param goCBReference GOOSE 控制块的 IEC 61850-7-2 ACSI 对象参考
 * @param updateRcb 对现有 ClientGooseControlBlock 实例的引用或 NULL
 * @return 新的 ClientGooseControlBlock 实例，或者用户通过 updateRcb 参数提供的实例
 */
LIB61850_API ClientGooseControlBlock
IedConnection_getGoCBValues(IedConnection self, IedClientError* error, 
                            const char* goCBReference, ClientGooseControlBlock updateGoCB);

typedef void
(*IedConnection_GetGoCBValuesHandler) (uint32_t invokeId, void* parameter, IedClientError err, ClientGooseControlBlock goCB);


LIB61850_API uint32_t
IedConnection_getGoCBValuesAsync(IedConnection self, IedClientError* error, const char* goCBReference, 
        ClientGooseControlBlock updateGoCB, IedConnection_GetGoCBValuesHandler handler, void* parameter);

/**
 * @brief 对连接的服务器上的 GOOSE 控制块 (GoCB) 的属性进行写入访问
 * GoCB 及其要写入的值由 goCB 参数指定
 *
 * parametersMask 参数指定此请求需要设置远程 GoCB 的哪些属性。
 * 您可以通过对定义的位值进行按位或运算来指定多个属性。如果需要写入所有属性，可以使用 GOCB_ELEMENT_ALL。
 *
 * singleRequest 参数指定与相应 MMS 写入请求的映射关系。符合标准的服务器应接受两种格式。
 * 但有些服务器只接受一种格式。在这种情况下，此参数的值就显得尤为重要。
 * @param goCB ClientGooseControlBlock 实例实际保存要写入的参数值
 * @param parametersMask 指定 setGoCBValues 请求中包含的参数
 * @param singleRequest 指定 seGoCBValues 服务是映射到包含多个变量的单个 MMS 写入请求，还是映射到多个 MMS 写入请求
 */
LIB61850_API void
IedConnection_setGoCBValues(IedConnection self, IedClientError* error, ClientGooseControlBlock goCB,
        uint32_t parametersMask, bool singleRequest);

/**
 * @param parametersMask 指定 setGoCBValues 请求中包含的参数
 * @param singleRequest specifies if the seGoCBValues services is mapped to a single MMS write request containing
 *        multiple variables or to multiple MMS write requests.
 * @param handler 服务完成或超时时调用的用户回调函数
 * @param parameter user provided parameter that is passed to the callback handler
 * @return the invoke ID of the request
 */
LIB61850_API uint32_t
IedConnection_setGoCBValuesAsync(IedConnection self, IedClientError* error, ClientGooseControlBlock goCB,
    uint32_t parametersMask, bool singleRequest, IedConnection_GenericServiceHandler handler, void* parameter);


//////////////////////////////////////////////////////////////////////////////
//                     Data model access services
//////////////////////////////////////////////////////////////////////////////
/**
 * @brief 读取功能约束数据属性（FCDA）或功能约束数据（FCD）
 * @param self  the connection object to operate on
 * @param error the error code if an error occurs
 * @param object reference of the object/attribute to read
 * @param fc the functional constraint of the data attribute or data object to read
 * @return the MmsValue instance of the received value or NULL if the request failed
 */
LIB61850_API MmsValue*
IedConnection_readObject(IedConnection self, IedClientError* error, const char* dataAttributeReference, FunctionalConstraint fc);

/// 客户端数据访问（读/写）服务功能
typedef void
(*IedConnection_ReadObjectHandler) (uint32_t invokeId, void* parameter, IedClientError err, MmsValue* value);


/**
 * @brief read a functional constrained data attribute (FCDA) or functional constrained data (FCD) - async version
 * @param self  the connection object to operate on
 * @param error the error code if an error occurs
 * @param object reference of the object/attribute to read
 * @param fc the functional constraint of the data attribute or data object to read
 * @param handler the user provided callback handler
 * @param parameter user provided parameter that is passed to the callback handler
 * @return 请求的调用 ID
 */
LIB61850_API uint32_t
IedConnection_readObjectAsync(IedConnection self, IedClientError* error, const char* objRef, FunctionalConstraint fc,
        IedConnection_ReadObjectHandler handler, void* parameter);


/**
 * @brief write a functional constrained data attribute (FCDA) or functional constrained data (FCD).
 * @param self  the connection object to operate on
 * @param error the error code if an error occurs
 * @param object reference of the object/attribute to write
 * @param fc the functional constraint of the data attribute or data object to write
 * @param value the MmsValue to write (has to be of the correct type - MMS_STRUCTURE for FCD)
 */
LIB61850_API void
IedConnection_writeObject(IedConnection self, IedClientError* error, const char* dataAttributeReference, 
                          FunctionalConstraint fc, MmsValue* value);


/**
 * @brief write a functional constrained data attribute (FCDA) or functional constrained data (FCD) - async version
 * @param self  the connection object to operate on
 * @param error the error code if an error occurs
 * @param object reference of the object/attribute to write
 * @param fc the functional constraint of the data attribute or data object to write
 * @param value the MmsValue to write (has to be of the correct type - MMS_STRUCTURE for FCD)
 * @param handler the user provided callback handler
 * @param parameter user provided parameter that is passed to the callback handler
 * @return the invoke ID of the request
 */
LIB61850_API uint32_t
IedConnection_writeObjectAsync(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, MmsValue* value, IedConnection_GenericServiceHandler handler, void* parameter);

/**
 * @brief 读取布尔类型的函数约束数据属性 (FCDA)
 * @param self  the connection object to operate on
 * @param error the error code if an error occurs
 * @param object reference of the data attribute to read
 * @param fc the functional constraint of the data attribute to read
 */
LIB61850_API bool
IedConnection_readBooleanValue(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc);

LIB61850_API float
IedConnection_readFloatValue(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc);

LIB61850_API char*
IedConnection_readStringValue(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc);

LIB61850_API int32_t
IedConnection_readInt32Value(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc);

LIB61850_API int64_t
IedConnection_readInt64Value(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc);

LIB61850_API uint32_t
IedConnection_readUnsigned32Value(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc);


/**
 * @brief 读取类型为时间戳（UTC 时间）的功能约束数据属性（FCDA）
 *  NOTE: 如果时间戳参数设置为 NULL，则该函数会分配一个新的时间戳实例。
 *        否则，返回值是指向用户提供的时间戳实例的指针。新的时间戳实例必须由函数调用者释放。
 * @param timestamp a pointer to a user provided timestamp instance or NULL
 * @return the timestamp value
 */
LIB61850_API Timestamp*
IedConnection_readTimestampValue(IedConnection self, IedClientError* error, const char* objectReference, 
                                 FunctionalConstraint fc, Timestamp* timeStamp);

/**
 * @brief read a functional constrained data attribute (FCDA) of type Quality
 * @param self  the connection object to operate on
 * @param error the error code if an error occurs
 * @param object reference of the data attribute to read
 * @param fc the functional constraint of the data attribute to read
 * @return the timestamp value
 */
LIB61850_API Quality
IedConnection_readQualityValue(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc);

LIB61850_API void
IedConnection_writeBooleanValue(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, bool value);

LIB61850_API void
IedConnection_writeInt32Value(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, int32_t value);

LIB61850_API void
IedConnection_writeUnsigned32Value(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, uint32_t value);

LIB61850_API void
IedConnection_writeFloatValue(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, float value);

LIB61850_API void
IedConnection_writeVisibleStringValue(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, char* value);

LIB61850_API void
IedConnection_writeOctetString(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, uint8_t* value, int valueLength);


///////////////////////////////////////////////////
//            Reporting services
///////////////////////////////////////////////////
/**
 * @brief 对连接服务器上的报表控制块 (RCB) 的属性具有读取权限。请求的 RCB 必须通过其对象引用来指定
 * e.g. "simpleIOGenericIO/LLN0.RP.EventsRCB01"
 * 报表控制块的名称中，逻辑节点部分之后会包含“RP”或“BR”。“RP”是无缓冲报表控制块名称的一部分，“BR”是有缓冲报表控制块名称的一部分
 */
LIB61850_API ClientReportControlBlock
IedConnection_getRCBValues(IedConnection self, IedClientError* error, const char* rcbReference,
        ClientReportControlBlock updateRcb);

typedef void
(*IedConnection_GetRCBValuesHandler) (uint32_t invokeId, void* parameter, IedClientError err, ClientReportControlBlock rcb);

LIB61850_API uint32_t
IedConnection_getRCBValuesAsync(IedConnection self, IedClientError* error, const char* rcbReference, 
                        ClientReportControlBlock updateRcb, IedConnection_GetRCBValuesHandler handler, void* parameter);

/** Describes the reason for the inclusion of the element in the report */
typedef int ReasonForInclusion;

/** the element is not included in the received report */
#define IEC61850_REASON_NOT_INCLUDED 0

/** the element is included due to a change of the data value */
#define IEC61850_REASON_DATA_CHANGE 1

/** the element is included due to a change in the quality of data */
#define IEC61850_REASON_QUALITY_CHANGE 2

/** the element is included due to an update of the data value */
#define IEC61850_REASON_DATA_UPDATE 4

/** the element is included due to a periodic integrity report task */
#define IEC61850_REASON_INTEGRITY 8

/** the element is included due to a general interrogation by the client */
#define IEC61850_REASON_GI 16

/** the reason for inclusion is unknown (e.g. report is not configured to include reason-for-inclusion) */
#define IEC61850_REASON_UNKNOWN 32

/* Element encoding mask values for ClientReportControlBlock */

/** include the report ID into the setRCB request */
#define RCB_ELEMENT_RPT_ID            1

/** include the report enable element into the setRCB request */
#define RCB_ELEMENT_RPT_ENA           2

/** include the reservation element into the setRCB request (only available in unbuffered RCBs!) */
#define RCB_ELEMENT_RESV              4

/** include the data set element into the setRCB request */
#define RCB_ELEMENT_DATSET            8

/** include the configuration revision element into the setRCB request */
#define RCB_ELEMENT_CONF_REV         16

/** include the option fields element into the setRCB request */
#define RCB_ELEMENT_OPT_FLDS         32

/** include the bufTm (event buffering time) element into the setRCB request */
#define RCB_ELEMENT_BUF_TM           64

/** include the sequence number element into the setRCB request (should be used!) */
#define RCB_ELEMENT_SQ_NUM          128

/** include the trigger options element into the setRCB request */
#define RCB_ELEMENT_TRG_OPS         256

/** include the integrity period element into the setRCB request */
#define RCB_ELEMENT_INTG_PD         512

/** include the GI (general interrogation) element into the setRCB request */
#define RCB_ELEMENT_GI             1024

/** include the purge buffer element into the setRCB request (only available in buffered RCBs) */
#define RCB_ELEMENT_PURGE_BUF      2048

/** include the entry ID element into the setRCB request (only available in buffered RCBs) */
#define RCB_ELEMENT_ENTRY_ID       4096

/** include the time of entry element into the setRCB request (only available in buffered RCBs) */
#define RCB_ELEMENT_TIME_OF_ENTRY  8192

/** include the reservation time element into the setRCB request (only available in buffered RCBs) */
#define RCB_ELEMENT_RESV_TMS      16384

/** include the owner element into the setRCB request */
#define RCB_ELEMENT_OWNER         32768


/**
 * @brief 对连接服务器上的报表控制块 (RCB) 的属性具有写入权限
 * 请求的 RCB 必须通过其对象引用来指定（另请参阅 IedConnection_getRCBValues）
 * 所引用 RCB 的对象引用包含在提供的 ClientReportControlBlock 实例中
 * @param parametersMask 参数指定此请求需要设置远程 RCB 的哪些属性。您可以通过对定义的位值进行按位或运算来指定多个属性
 * @param singleRequest 指定与相应 MMS 写入请求的映射关系符合标准的服务器应接受两种格式。
 * 但有些服务器只接受一种格式。在这种情况下，此参数的值就显得尤为重要。
 */
LIB61850_API void
IedConnection_setRCBValues(IedConnection self, IedClientError* error, ClientReportControlBlock rcb,
        uint32_t parametersMask, bool singleRequest);

LIB61850_API uint32_t
IedConnection_setRCBValuesAsync(IedConnection self, IedClientError* error, ClientReportControlBlock rcb,
        uint32_t parametersMask, bool singleRequest, IedConnection_GenericServiceHandler handler, void* parameter);

/**
 * @brief 用于接收报告的回调函数
 * @param parameter a user provided parameter that is handed to the callback function
 * @param report 一个 ClientReport 实例，其中包含接收到的报告中的信息
 */
typedef void (*ReportCallbackFunction) (void* parameter, ClientReport report);


/**
 * @brief Install a report handler function for the specified report control block (RCB)
 * \note 替换报表处理程序时，只需调用此函数即可。无需单独调用IedConnection_uninstallReportHandler()函数
 * \note 请勿在 ReportCallbackFunction 内部调用此函数。否则会导致死锁
 */
LIB61850_API void
IedConnection_installReportHandler(IedConnection self, const char* rcbReference, const char* rptId, 
        ReportCallbackFunction handler, void* handlerParameter);

/**
 * @brief uninstall a report handler function for the specified report control block (RCB)
 * \note Do not call this function inside of the ReportCallbackFunction. Doing so will cause a deadlock.
 * @param self the connection object
 * @param rcbReference object reference of the report control block
 */
LIB61850_API void IedConnection_uninstallReportHandler(IedConnection self, const char* rcbReference);

/**
 * @brief 针对指定的报告控制块 (RCB) 触发一般查询 (GI) 报告
 * 必须先启用 RCB 并将 GI 设置为触发选项，才能执行此命令
 * @deprecated Use ClientReportControlBlock_setGI instead
 */
LIB61850_API void
IedConnection_triggerGIReport(IedConnection self, IedClientError* error, const char* rcbReference);


//////////////////////////////////////////////////////////
//           Access to received reports
//////////////////////////////////////////////////////////
/**
 * @brief 返回数据集名
 */
LIB61850_API const char* ClientReport_getDataSetName(ClientReport self);


/**
 * @brief 返回报告中接收到的数据集值
 * NOTE: 返回的 MmsValue 实例由库处理并且仅在 ClientReport 实例存在时有效！为避免并发问题，不应在报表回调处理程序之外使用它
 * @return 包含数据集值的 MmsValue 数组实例
 */
LIB61850_API MmsValue* ClientReport_getDataSetValues(ClientReport self);


/**
 * @return 返回与此 ClientReport 对象关联的服务器 RCB 的引用（名称）
 */
LIB61850_API char* ClientReport_getRcbReference(ClientReport self);

/**
 * @brief return RptId of the server RCB associated with this ClientReport object
 * @param self the ClientReport instance
 * @return report control block reference as string
 */
LIB61850_API char* ClientReport_getRptId(ClientReport self);

/**
 * @brief get the reason code (reason for inclusion) for a specific report data set element
 * @param self the ClientReport instance
 * @param elementIndex index of the data set element (starting with 0)
 * @return reason code for the inclusion of the specified element
 */
LIB61850_API ReasonForInclusion ClientReport_getReasonForInclusion(ClientReport self, int elementIndex);

LIB61850_API MmsValue* ClientReport_getEntryId(ClientReport self);

LIB61850_API bool ClientReport_hasTimestamp(ClientReport self);

LIB61850_API bool ClientReport_hasSeqNum(ClientReport self);

LIB61850_API uint16_t ClientReport_getSeqNum(ClientReport self);

LIB61850_API bool ClientReport_hasDataSetName(ClientReport self);

LIB61850_API bool ClientReport_hasReasonForInclusion(ClientReport self);

LIB61850_API bool ClientReport_hasConfRev(ClientReport self);

/**
 * NOTE: 如果报告中不存在配置修订版本，则返回值为未定义
 * @return self.confRev
 */
LIB61850_API uint32_t ClientReport_getConfRev(ClientReport self);

LIB61850_API bool ClientReport_hasBufOvfl(ClientReport self);

/**
 * @return ClientReport.bufOverflow
 */
LIB61850_API bool ClientReport_getBufOvfl(ClientReport self);

LIB61850_API bool ClientReport_hasDataReference(ClientReport self);

/**
 * @brief get the data-reference of the element of the report data set
 * This function will only return a non-NULL value if the received report contains data-references.
 * This can be determined by the ClientReport_hasDataReference function.
 * NOTE: The returned string is allocated and hold by the ClientReport instance and is only valid until
 * the ClientReport instance exists!
 * @param self the ClientReport instance
 * @param elementIndex  index of the data set element (starting with 0)
 * @param the data reference as string as provided by the report or NULL if the data reference is not available
 */
LIB61850_API const char* ClientReport_getDataReference(ClientReport self, int elementIndex);


/**
 * @return 时间戳，以毫秒为单位，自 1970 年 1 月 1 日 UTC 起计算
 */
LIB61850_API uint64_t ClientReport_getTimestamp(ClientReport self);

/**
 * @brief indicates if the report contains a sub sequence number and a more segments follow flags (for segmented reporting)
 * @param self the ClientReport instance
 * @returns true if the report contains sub-sequence-number and more-follows-flag, false otherwise
 */
LIB61850_API bool ClientReport_hasSubSeqNum(ClientReport self);

/**
 * @brief get the sub sequence number of the report (for segmented reporting)
 * Returns the sub sequence number of the report. This is 0 for the first report of a segmented report and
 * will be increased by one for each report segment.
 * @param self the ClientReport instance
 * @return the sub sequence number of the last received report message.
 */
LIB61850_API uint16_t ClientReport_getSubSeqNum(ClientReport self);


/* 获取接收到的报告段的更多段关注标志（用于分段报告）
 * 如果这是分段报表的一部分，并且后面还会有更多报表分段，则返回 true；
 * 如果当前报表不是分段报表，或者当前报表是分段报表的最后一个分段，则返回 false。
 */ 
LIB61850_API bool ClientReport_getMoreSeqmentsFollow(ClientReport self);

/**
 * @brief get the reason for inclusion of as a human readable string
 * @param reasonCode
 * @return the reason for inclusion as static human readable string
 */
LIB61850_API char* ReasonForInclusion_getValueAsString(ReasonForInclusion reasonCode);

//////////////////////////////////////////////////////////
//      ClientReportControlBlock access class
//////////////////////////////////////////////////////////
LIB61850_API ClientReportControlBlock ClientReportControlBlock_create(const char* rcbReference);

LIB61850_API void ClientReportControlBlock_destroy(ClientReportControlBlock self);

LIB61850_API char* ClientReportControlBlock_getObjectReference(ClientReportControlBlock self);

LIB61850_API bool ClientReportControlBlock_isBuffered(ClientReportControlBlock self);

LIB61850_API const char* ClientReportControlBlock_getRptId(ClientReportControlBlock self);

LIB61850_API void ClientReportControlBlock_setRptId(ClientReportControlBlock self, const char* rptId);

LIB61850_API bool ClientReportControlBlock_getRptEna(ClientReportControlBlock self);

LIB61850_API void ClientReportControlBlock_setRptEna(ClientReportControlBlock self, bool rptEna);

LIB61850_API bool ClientReportControlBlock_getResv(ClientReportControlBlock self);

LIB61850_API void ClientReportControlBlock_setResv(ClientReportControlBlock self, bool resv);

LIB61850_API const char* ClientReportControlBlock_getDataSetReference(ClientReportControlBlock self);


/**
 * @brief 设置RCB要观察的数据集
 * 数据集引用混合了 MMS 和 IEC 61850 语法！通常，引用格式为：LDName/LNName$DataSetName
 * 例如 "simpleIOGenericIO/LLN0$Events"
 * 通常情况下，数据集是在 LN0 逻辑节点中定义的，但这并非强制性要求.
 * Note: 由于数据集的更改，服务器将增加 RCB 的 confRev 属性
 */
LIB61850_API void
ClientReportControlBlock_setDataSetReference(ClientReportControlBlock self, const char* dataSetReference);

LIB61850_API uint32_t ClientReportControlBlock_getConfRev(ClientReportControlBlock self);

LIB61850_API int ClientReportControlBlock_getOptFlds(ClientReportControlBlock self);

LIB61850_API void ClientReportControlBlock_setOptFlds(ClientReportControlBlock self, int optFlds);

/**
 * @brief 获取 RCB 的 BufTm（缓冲时间）参数
 * 缓冲时间是指触发事件发生后，到实际发送报告之间等待的时间。它用于收集短时间内发生的事件，并将它们合并到一个报告中发送
 * @param self the RCB instance
 */
LIB61850_API uint32_t ClientReportControlBlock_getBufTm(ClientReportControlBlock self);

LIB61850_API void ClientReportControlBlock_setBufTm(ClientReportControlBlock self, uint32_t bufTm);

LIB61850_API uint16_t ClientReportControlBlock_getSqNum(ClientReportControlBlock self);

LIB61850_API int ClientReportControlBlock_getTrgOps(ClientReportControlBlock self);

LIB61850_API void ClientReportControlBlock_setTrgOps(ClientReportControlBlock self, int trgOps);

LIB61850_API uint32_t ClientReportControlBlock_getIntgPd(ClientReportControlBlock self);

LIB61850_API void ClientReportControlBlock_setIntgPd(ClientReportControlBlock self, uint32_t intgPd);

LIB61850_API bool ClientReportControlBlock_getGI(ClientReportControlBlock self);

LIB61850_API void ClientReportControlBlock_setGI(ClientReportControlBlock self, bool gi);

LIB61850_API bool ClientReportControlBlock_getPurgeBuf(ClientReportControlBlock self);

LIB61850_API void ClientReportControlBlock_setPurgeBuf(ClientReportControlBlock self, bool purgeBuf);

LIB61850_API  bool ClientReportControlBlock_hasResvTms(ClientReportControlBlock self);

LIB61850_API int16_t ClientReportControlBlock_getResvTms(ClientReportControlBlock self);

LIB61850_API void ClientReportControlBlock_setResvTms(ClientReportControlBlock self, int16_t resvTms);

LIB61850_API MmsValue* /* <MMS_OCTET_STRING> */
ClientReportControlBlock_getEntryId(ClientReportControlBlock self);

LIB61850_API void ClientReportControlBlock_setEntryId(ClientReportControlBlock self, MmsValue* entryId);

LIB61850_API uint64_t ClientReportControlBlock_getEntryTime(ClientReportControlBlock self);

LIB61850_API MmsValue* /* <MMS_OCTET_STRING> */
ClientReportControlBlock_getOwner(ClientReportControlBlock self);


//////////////////////////////////////////////////
//           Data set handling
//////////////////////////////////////////////////
/**
 * @brief 从服务器获取数据集值
 * 接收到的数据集值存储在 ClientDataSet 类型的容器对象中，该对象可在后续的读取请求中重复使用
 * @return 如果发生错误，则检索到的数据集实例值为 NULL
 */
LIB61850_API ClientDataSet
IedConnection_readDataSetValues(IedConnection self, IedClientError* error, const char* dataSetReference, ClientDataSet dataSet);

typedef void
(*IedConnection_ReadDataSetHandler) (uint32_t invokeId, void* parameter, IedClientError err, ClientDataSet dataSet);

LIB61850_API uint32_t
IedConnection_readDataSetValuesAsync(IedConnection self, IedClientError* error, const char* dataSetReference, 
                                ClientDataSet dataSet, IedConnection_ReadDataSetHandler handler, void* parameter);

/**
 * @brief 在连接的服务器设备上创建新数据集
 * 此函数在服务器上创建一个新的数据集 参数 dataSetReference 是要创建的新数据集的名称格式是 
 * LDName/LNodeName（用于永久域或 VMD 范围的数据集）或 @dataSetName（用于关联特定的数据集）
 * 如果引用中缺少 LDName 部分，则生成的数据集将是 VMD 范围的
 * @param dataSetElements 参数包含一个链表其中包含 FCD 或 FCDA 的对象引用。格式为 LDName/LNodeName.item(arrayIndex)component[FC]
 */
LIB61850_API void
IedConnection_createDataSet(IedConnection self, IedClientError* error, const char* dataSetReference, LinkedList /* char* */ dataSetElements);


/**
 * @return 请求的调用 ID
 */
LIB61850_API uint32_t
IedConnection_createDataSetAsync(IedConnection self, IedClientError* error, const char* dataSetReference, 
        LinkedList /* char* */ dataSetElements, IedConnection_GenericServiceHandler handler, void* parameter);


/**
 * @brief 在连接的服务器设备上删除可删除的数据集
 * @return 如果数据集已被删除，则为 true；否则为 false
 */
LIB61850_API bool
IedConnection_deleteDataSet(IedConnection self, IedClientError* error, const char* dataSetReference);

/**
 * @return the invoke ID of the request
 */
LIB61850_API uint32_t
IedConnection_deleteDataSetAsync(IedConnection self, IedClientError* error, const char* dataSetReference,
        IedConnection_GenericServiceHandler handler, void* parameter);

/**
 * @brief 读取服务器上某个指定数据集的成员列表（目录）
 * The return value contains a linked list containing the object references of FCDs or FCDAs. The format of
 * this object references is LDName/LNodeName.item(arrayIndex)component[FC].
 * e.g. "CTRL1/XCBR1.Pos.stVal[ST]"     "MEAS1/MMXU1.A.phsA.cVal.mag.f[MX]"
 * @return LinkedList containing the data set elements as char* strings.
 */
LIB61850_API LinkedList /* <char*> */
IedConnection_getDataSetDirectory(IedConnection self, IedClientError* error, const char* dataSetReference, bool* isDeletable);


/**
 * @brief GetDataSetDirectory 响应或超时回调
 * @param dataSetDirectory 包含 FCD 或 FCDA 对象引用的链表此对象引用的格式为 LDName/LNodeName.item(arrayIndex)component[FC]
 * @param isDeletable 这是一个输出参数，表示请求的数据集可以由客户端删除
 */
typedef void
(*IedConnection_GetDataSetDirectoryHandler) (uint32_t invokeId, void* parameter, IedClientError err, 
                                             LinkedList /* <char*> */ dataSetDirectory, bool isDeletable);

LIB61850_API uint32_t
IedConnection_getDataSetDirectoryAsync(IedConnection self, IedClientError* error, const char* dataSetReference,
        IedConnection_GetDataSetDirectoryHandler handler, void* parameter);


/**
 * @brief 将数据集值写入服务器
 * @param values 参数的元素数量必须与数据集中的成员数量相同
 * @param accessResult 返回参数包含每个数据集成员的值
 */
LIB61850_API void
IedConnection_writeDataSetValues(IedConnection self, IedClientError* error, const char* dataSetReference,
        LinkedList/*<MmsValue*>*/ values, /* OUTPUT */LinkedList* /* <MmsValue*> */accessResults);

/**
 * @brief Callback handler for asynchronous write data set values services (set data set)
 * @param invokeId the invoke ID of the service request
 * @param parameter used provided parameter
 * @param err the error code if an error occurs
 * @param accessResults the list of access results for the data set entries.
 */
typedef void
(*IedConnection_WriteDataSetHandler) (uint32_t invokeId, void* parameter, IedClientError err, LinkedList /* <MmsValue*> */accessResults);

LIB61850_API uint32_t
IedConnection_writeDataSetValuesAsync(IedConnection self, IedClientError* error, const char* dataSetReference,
        LinkedList/*<MmsValue*>*/ values, IedConnection_WriteDataSetHandler handler, void* parameter);


/////////////////////////////////////////////////////////////////////
//    Data set object (local representation of a data set)
/////////////////////////////////////////////////////////////////////
/**
 * @brief destroy an ClientDataSet instance. Has to be called by the application.
 * NOTE: ClientDataSet不能由应用程序直接创建只能通过 IedConnection_readDataSetValues 函数创建。因此没有公开的 ClientDataSet_create 函数
 */
LIB61850_API void ClientDataSet_destroy(ClientDataSet self);

/**
 * @brief 获取本地存储在 ClientDataSet 实例中的数据集值
 * 此函数返回指向本地存储的 ClientDataSet 实例的 MmsValue 实例的指针。MmsValue 实例的类型为 MMS_ARRAY，每个数据集成员对应一个数组元素
 * NOTE: This call does not invoke any interaction with the associated server. It will only provide access to already stored value. 
 * To update the values with the current values of the server the IecConnection_readDataSetValues function has to be called!
 * @return 将本地存储的数据集值作为 MMS_ARRAY 类型的 MmsValue 对象
 */
LIB61850_API MmsValue* ClientDataSet_getValues(ClientDataSet self);

/**
 * @brief Get the object reference of the data set
 */
LIB61850_API char* ClientDataSet_getReference(ClientDataSet self);

LIB61850_API int ClientDataSet_getDataSetSize(ClientDataSet self);



////////////////////////////////////////////////////////
//            Control service functions
////////////////////////////////////////////////////////
typedef struct sControlObjectClient* ControlObjectClient;

/**
 * @brief 创建一个新的客户端控件对象
 * 客户端控制对象用于处理可控数据对象的所有客户端功能。可控数据对象是可控 CDC（例如 SPC、DPC、APC 等）的实例
 * NOTE: 此函数会同步向服务器请求有关控制对象（例如 ctlModel）的信息。该函数会阻塞，直到这些请求返回或超时
 */
LIB61850_API ControlObjectClient ControlObjectClient_create(const char* objectReference, IedConnection connection);

LIB61850_API ControlObjectClient
ControlObjectClient_createEx(const char* objectReference, IedConnection connection, 
                             ControlModel ctlModel, MmsVariableSpecification* controlObjectSpec);

/**
 * @brief 销毁客户端控制对象实例并释放所有相关资源
 * NOTE: Can only be called before calling IedConnection_destroy! When calling IedConnection_destroy this
 * function will be called automatically.
 */
LIB61850_API void ControlObjectClient_destroy(ControlObjectClient self);

typedef enum
{
    CONTROL_ACTION_TYPE_SELECT = 0, /** < callback was invoked because of a select command */
    CONTROL_ACTION_TYPE_OPERATE = 1,  /** < callback was invoked because of an operate command */
    CONTROL_ACTION_TYPE_CANCEL = 2 /** < callback was invoked because of a cancel command */
} ControlActionType;

/**
 * @brief 收到命令终止消息时调用的回调处理程序
 */
typedef void
(*ControlObjectClient_ControlActionHandler) (uint32_t invokeId, void* parameter, IedClientError err, ControlActionType type, bool success);

/**
 * @brief Get the object reference of the control data object
 * @param self the control object instance to use
 * @return the object reference (string is valid only as long as the \ref ControlObjectClient instance exists).
 */
LIB61850_API const char* ControlObjectClient_getObjectReference(ControlObjectClient self);

/**
 * @brief Get the current control model (local representation) applied to the control object
 * @param self the control object instance to use
 * @return the current applied control model (\ref ControlModel)
 */
LIB61850_API ControlModel ControlObjectClient_getControlModel(ControlObjectClient self);

/**
 * @brief 设置应用的控制模型
 * NOTE: This function call will not change the server control model.
 * @param self the control object instance to use
 * @param ctlModel the new control model to apply
 */
LIB61850_API void
ControlObjectClient_setControlModel(ControlObjectClient self, ControlModel ctlModel);

/**
 * @brief 更改服务器的控制模型
 * NOTE: 并非所有服务器都支持此功能。相关信息可在服务器的 PIXIT 文件中查看，此功能还会设置此客户端控件实例所应用的控件模型
 */
LIB61850_API void ControlObjectClient_changeServerControlModel(ControlObjectClient self, ControlModel ctlModel);

/**
 * @brief Get the type of ctlVal.
 * This type is required for the ctlVal parameter of the \ref ControlObjectClient_operate
 * and \ref  ControlObjectClient_selectWithValue functions.
 * @param self the control object instance to use
 * @return MmsType required for the ctlVal value.
 */
LIB61850_API MmsType ControlObjectClient_getCtlValType(ControlObjectClient self);

/**
 * @brief Get the error code of the last synchronous control action (operate, select, select-with-value, cancel)
 * @param self the control object instance to use
 * @return the client error code
 */
LIB61850_API IedClientError ControlObjectClient_getLastError(ControlObjectClient self);

/**
 * @brief 向服务器发送操作命令
 * @param operTime the time when the command has to be executed (for time activated control). The value represents the local time of the
 *                 server in milliseconds since epoch. If this value is 0 the command will be executed instantly.
 * @return true if operation has been successful, false otherwise.
 */
LIB61850_API bool ControlObjectClient_operate(ControlObjectClient self, MmsValue* ctlVal, uint64_t operTime);

/**
 * @brief 向服务器发送选择命令
 * select命令仅用于 先选择后操作（正常安全）控制模型 (CONTROL_MODEL_SBO_NORMAL)。必须先发送 select 命令，然后才能使用 operactl 命令
 */
LIB61850_API bool ControlObjectClient_select(ControlObjectClient self);

/**
 * @brief 向服务器发送带有值的 select 命令
 * “选择值”命令仅用于“增强安全性的先选择后操作”控制模型 (CONTROL_MODEL_SBO_ENHANCED)。必须先发送“选择值”命令，才能使用“操作”命令
 */
LIB61850_API bool ControlObjectClient_selectWithValue(ControlObjectClient self, MmsValue* ctlVal);


/**
 * @brief 向服务器发送取消命令
 * cancel 命令可用于停止正在进行的操作（当服务器和应用程序支持此功能时）以及取消先前的选择命令
 * @return true if operation has been successful, false otherwise.
 */
LIB61850_API bool ControlObjectClient_cancel(ControlObjectClient self);


// ctlVal 控制值
LIB61850_API uint32_t
ControlObjectClient_operateAsync(ControlObjectClient self, IedClientError* err, 
                MmsValue* ctlVal, uint64_t operTime,
                ControlObjectClient_ControlActionHandler handler, void* parameter);

LIB61850_API uint32_t
ControlObjectClient_selectAsync(ControlObjectClient self, IedClientError* err, ControlObjectClient_ControlActionHandler handler, void* parameter);

LIB61850_API uint32_t
ControlObjectClient_selectWithValueAsync(ControlObjectClient self, IedClientError* err, MmsValue* ctlVal,
        ControlObjectClient_ControlActionHandler handler, void* parameter);

/**
 * @brief Send a cancel command to the server - async version
 * The cancel command can be used to stop an ongoing operation (when the server and application
 * support this) and to cancel a former select command.
 * @param self the control object instance to use
 * @param[out] err error code
 * @param handler the user provided callback handler
 * @param parameter user provided parameter that is passed to the callback handler
 * @return the invoke ID of the request
 */
LIB61850_API uint32_t
ControlObjectClient_cancelAsync(ControlObjectClient self, IedClientError* err, 
                                ControlObjectClient_ControlActionHandler handler, void* parameter);

LIB61850_API LastApplError ControlObjectClient_getLastApplError(ControlObjectClient self);

/**
 * @brief Send commands in test mode.
 * When the server supports test mode the commands that are sent with the test flag set
 * are not executed (will have no effect on the attached physical process).
 * @param self the control object instance to use
 * @param value value if the test flag (true = test mode).
 */
LIB61850_API void ControlObjectClient_setTestMode(ControlObjectClient self, bool value);

/**
 * @brief 设置控制命令的原点参数
 * origin 参数用于识别发送控制命令的客户端/应用程序，以便后续分析
 * @param orIdent originator identification can be an arbitrary string
 * @param orCat originator category (see \ref ORIGINATOR_CATEGORIES)
 */
LIB61850_API void
ControlObjectClient_setOrigin(ControlObjectClient self, const char* orIdent, int orCat);

/**
 * @brief 对于单个控制序列的所有命令（选择、操作、取消），使用恒定的 T 参数
 * NOTE: 某些不符合标准的服务器可能需要此功能才能接受操作/取消请求
 * @param self the ControlObjectClient instance
 * @param useContantT enable this behavior with true, disable with false
 */
LIB61850_API void ControlObjectClient_useConstantT(ControlObjectClient self, bool useConstantT);

/**
 * @deprecated use ControlObjectClient_setInterlockCheck instead
 */
LIB61850_API DEPRECATED void ControlObjectClient_enableInterlockCheck(ControlObjectClient self);

/**
 * @deprecated use ControlObjectClient_setSynchroCheck instead
 */
LIB61850_API DEPRECATED void ControlObjectClient_enableSynchroCheck(ControlObjectClient self);

/**
 * @deprecated Do not use (ctlNum is handled automatically by the library)! Intended for test purposes only.
 */
LIB61850_API DEPRECATED void ControlObjectClient_setCtlNum(ControlObjectClient self, uint8_t ctlNum);

/**
 * @brief Set the value of the interlock check flag when a control command is sent
 * @param self the ControlObjectClient instance
 * @param value if true the server will perform a interlock check if supported
 */
LIB61850_API void ControlObjectClient_setInterlockCheck(ControlObjectClient self, bool value);

/**
 * @brief 发送控制命令时，设置同步检查标志的值
 * @param value if true the server will perform a synchro check if supported
 */
LIB61850_API void ControlObjectClient_setSynchroCheck(ControlObjectClient self, bool value);


/**
 * @brief 当收到 CommandTermination+ 或 CommandTermination- 消息时，将调用此回调函数
 * 要区分 CommandTermination+ 和 CommandTermination-，请使用ControlObjectClient_getLastApplError函数
 * 对于 CommandTermination+ 消息 ControlObjectClient_getLastApplError的返回值设置了 error=CONTROL_ERROR_NO_ERROR 和 addCause=ADD_CAUSE_UNKNOWN
 * 当 addCause 不等于 ADD_CAUSE_UNKNOWN 时，客户端收到了 CommandTermination- 消息
 * NOTE: 不要在此回调函数内部调用ControlObjectClient_destroy！这样做会导致死锁
 */
typedef void (*CommandTerminationHandler) (void* parameter, ControlObjectClient controlClient);

/**
 * @brief 为该控制对象设置命令终止回调处理程序
 */
LIB61850_API void
ControlObjectClient_setCommandTerminationHandler(ControlObjectClient self, CommandTerminationHandler handler, void* handlerParameter);


///////////////////////////////////////////////////////////////////
//              Model discovery services
///////////////////////////////////////////////////////////////////
/**
 * @brief 从服务端获取 device model
 * 此函数从服务器检索完整的设备模型。该模型会被缓存，以便后续 API 调用可以浏览。此 API 调用映射到多个 ACSI 服务
 * @param self the connection object
 * @param error the error code if an error occurs
 */
LIB61850_API void IedConnection_getDeviceModelFromServer(IedConnection self, IedClientError* error);

/**
 * @brief Get the list of logical devices available at the server (DEPRECATED)
 * This function is mapped to the GetServerDirectory(LD) ACSI service.
 * NOTE: This function will call \ref IedConnection_getDeviceModelFromServer if no buffered data model
 * information is available. Otherwise it will use the buffered information.
 * @param self the connection object
 * @param error the error code if an error occurs
 * @return LinkedList with string elements representing the logical device names
 */
LIB61850_API LinkedList /*<char*>*/
IedConnection_getLogicalDeviceList(IedConnection self, IedClientError* error);

/**
 * @brief Get the list of logical devices or files available at the server
 * GetServerDirectory ACSI service implementation. This function will either return the list of
 * logical devices (LD) present at the server or the list of available files.
 * NOTE: When getFIleNames is false zhis function will call
 * \ref IedConnection_getDeviceModelFromServer if no buffered data model
 * information is available. Otherwise it will use the buffered information.
 * @param self the connection object
 * @param error the error code if an error occurs
 * @param getFileNames get list of files instead of logical device names (TO BE IMPLEMENTED)
 *
 * @return LinkedList with string elements representing the logical device names or file names
 */
LIB61850_API LinkedList /*<char*>*/
IedConnection_getServerDirectory(IedConnection self, IedClientError* error, bool getFileNames);

/**
 * @brief 获取逻辑设备的逻辑节点（LN）列表
 * GetLogicalDeviceDirectory ACSI 服务实现。
 * 返回逻辑设备中存在的逻辑节点名称列表。该列表以 LinkedList 类型的链表形式返回，元素为 C 风格的字符串
 * NOTE: 如果没有可用的缓冲数据模型信息，此函数将调用IedConnection_getDeviceModelFromServer。否则，它将使用缓冲信息
 * @param logicalDeviceName the name of the logical device (LD) of interest
 * @return 链表，其字符串元素表示逻辑节点名称
 */
LIB61850_API LinkedList /*<char*>*/
IedConnection_getLogicalDeviceDirectory(IedConnection self, IedClientError* error, const char* logicalDeviceName);

/**
 * @brief 返回给定逻辑节点的所有子 MMS 变量的列表
 * 此函数无法映射到任何 ACSI 服务。它是一个便捷函数，供希望显示代表逻辑节点的 MMS 命名变量的所有可用子节点列表的通用客户端使用
 * NOTE: 如果没有可用的缓冲数据模型信息，此函数将调用IedConnection_getDeviceModelFromServer。否则，它将使用缓冲信息
 */
LIB61850_API LinkedList /*<char*>*/
IedConnection_getLogicalNodeVariables(IedConnection self, IedClientError* error, const char* logicalNodeReference);

/**
 * @brief 返回给定逻辑节点 (LN) 的目录，其中包含指定 ACSI 类的元素
 * 与 ACSI 的描述不同，此函数并非总是向服务器发出请求。对于大多数 ACSI 类，它只是访问之前检索到的数据模型，
 * 或者在没有缓冲数据模型信息的情况下调用IedConnection_getDeviceModelFromServer。
 * 但 ACSI 类 ACSI_CLASS_DATASET 和 ACSI_CLASS_LOG 是个例外。这两个类始终会向服务器发出请求。
 * @param logicalNodeReference string that represents the LN reference
 * @param acsiClass specifies the ACSI class
 * @return 以 C 字符串形式，在 LinkedList 中列出指定 ACSI 类类型的所有逻辑节点元素
 */
LIB61850_API LinkedList /*<char*>*/
IedConnection_getLogicalNodeDirectory(IedConnection self, IedClientError* error,
		const char* logicalNodeReference, ACSIClass acsiClass);

/**
 * @brief 返回给定数据对象的目录（DO）
 * 实现了 GetDataDirectory ACSI 服务。该服务将返回所有数据属性或子数据对象的列表
 * NOTE: 如果没有可用的缓冲数据模型信息，此函数将调用IedConnection_getDeviceModelFromServer。否则，它将使用缓冲信息
 * @param dataReference string that represents the DO reference
 * @return LinkedList 中所有数据属性或子数据对象的 C 字符串列表
 */
LIB61850_API LinkedList /*<char*>*/
IedConnection_getDataDirectory(IedConnection self, IedClientError* error, const char* dataReference);

/**
 * @brief 返回给定数据对象的目录（DO）
 * @return list of all data attributes or sub data objects as C strings in a LinkedList
 */
LIB61850_API LinkedList /*<char*>*/
IedConnection_getDataDirectoryFC(IedConnection self, IedClientError* error, const char* dataReference);

/**
 * @brief 返回具有给定 FC 的给定数据对象/数据属性的目录
 * 实现了 GetDataDirectory ACSI 服务。该服务将返回一个 C 字符串列表，其中包含所有数据属性或子数据对象作为元素
 * NOTE: This function will call \ref IedConnection_getDeviceModelFromServer if no buffered data model
 * information is available. Otherwise it will use the buffered information.
 * WARNING: 从 1.0.3 版本开始，功能约束将不再附加到名称字符串
 * @param error the error code if an error occurs
 * @param dataReference string that represents the DO reference
 * @param fc the functional constraint
 * @return list of all data attributes or sub data objects as C strings in a LinkedList
 */
LIB61850_API LinkedList
IedConnection_getDataDirectoryByFC(IedConnection self, IedClientError* error, const char* dataReference, FunctionalConstraint fc);

/**
 * @brief 返回由 dataAttributeReference 和函数约束 fc 引用的数据属性的 MMS 变量类型规范。
 */
LIB61850_API MmsVariableSpecification*
IedConnection_getVariableSpecification(IedConnection self, IedClientError* error, const char* dataAttributeReference,
        FunctionalConstraint fc);

/**
 * @brief Get all variables of the logical device
 * NOTE: This function will return all MMS variables of the logical device (MMS domain). The result will be in the
 * MMS notation (like "GGIO1$ST$Ind1$stVal") and also contain the variables of control blocks.
 * @param[in] self the connection object
 * @param[out] error the error code if an error occurs
 * @param[in] ldName the logical device name
 * @return a \ref LinkedList with the MMS variable names as string. Has to be released by the caller
 */
LIB61850_API LinkedList
IedConnection_getLogicalDeviceVariables(IedConnection self, IedClientError* error, const char* ldName);

/**
 * @brief 获取逻辑设备的数据集名称
 * NOTE: 此函数将返回逻辑设备（MMS 域）的所有数据集名称（MMS 命名变量列表）。结果将采用 MMS 表示法（例如“LLN0$dataset1”）
 * @param[in] ldName the logical device name
 * @return 一个包含字符串格式数据集名称的链表。必须由调用者释放。
 */
LIB61850_API LinkedList
IedConnection_getLogicalDeviceDataSets(IedConnection self, IedClientError* error, const char* ldName);

/////////////////////////////////////////////////////////
//     Asynchronous model discovery functions
/////////////////////////////////////////////////////////
typedef void
(*IedConnection_GetNameListHandler) (uint32_t invokeId, void* parameter, IedClientError err, LinkedList nameList, bool moreFollows);

LIB61850_API uint32_t
IedConnection_getServerDirectoryAsync(IedConnection self, IedClientError* error, const char* continueAfter, LinkedList result,
        IedConnection_GetNameListHandler handler, void* parameter);

LIB61850_API uint32_t
IedConnection_getLogicalDeviceVariablesAsync(IedConnection self, IedClientError* error, 
                const char* ldName, const char* continueAfter, LinkedList result,
                IedConnection_GetNameListHandler handler, void* parameter);

LIB61850_API uint32_t
IedConnection_getLogicalDeviceDataSetsAsync(IedConnection self, IedClientError* error, 
                const char* ldName, const char* continueAfter, LinkedList result,
                IedConnection_GetNameListHandler handler, void* parameter);


typedef void
(*IedConnection_GetVariableSpecificationHandler) (uint32_t invokeId, void* parameter, IedClientError err, MmsVariableSpecification* spec);

LIB61850_API uint32_t
IedConnection_getVariableSpecificationAsync(IedConnection self, IedClientError* error, const char* dataAttributeReference,
        FunctionalConstraint fc, IedConnection_GetVariableSpecificationHandler handler, void* parameter);

/**
 * @brief Implementation of the QueryLogByTime ACSI service
 * 从服务器日志中读取日志条目。要读取的日志条目由起始时间和结束时间指定。
 * 如果完整时间范围无法放入一条msg中，则 moreFollows 标志将被设置为 true，以表明指定时间范围内还有更多条目可用
 * @param logReference log object reference in the form <LD name>/<LN name>$<log name>
 * @param startTime as millisecond UTC timestamp
 * @param endTime as millisecond UTC timestamp
 * @param moreFollows （输出值）表示还有更多符合规范的条目可用
 * @return 符合规范的 MmsJournalEntry 对象列表
 */
LIB61850_API LinkedList /* <MmsJournalEntry> */
IedConnection_queryLogByTime(IedConnection self, IedClientError* error, const char* logReference,
        uint64_t startTime, uint64_t endTime, bool* moreFollows);

/**
 * @brief Implementation of the QueryLogAfter ACSI service
 * 从服务器日志中读取指定条目 ID 和时间戳之后的日志条目
 * 如果完整时间范围无法放入一条彩信中，则 moreFollows 标志将设置为 true，表示指定时间范围内还有更多条目可用
 * @param logReference 日志对象引用格式为 <LD 名称>/<LN 名称>$<日志名称>
 * @param entryID 通常是最后接收到的条目的条目 ID
 * @param timeStamp 以毫秒为单位的 UTC 时间戳
 * @param moreFollows （输出值）表示还有更多符合规范的条目可用
 * @return 符合规范的 MmsJournalEntry 对象列表
 */
LIB61850_API LinkedList /* <MmsJournalEntry> */
IedConnection_queryLogAfter(IedConnection self, IedClientError* error, const char* logReference,
        MmsValue* entryID, uint64_t timeStamp, bool* moreFollows);


typedef void
(*IedConnection_QueryLogHandler) (uint32_t invokeId, void* parameter, IedClientError mmsError, 
                                  LinkedList /* <MmsJournalEntry> */ journalEntries, bool moreFollows);

LIB61850_API uint32_t
IedConnection_queryLogByTimeAsync(IedConnection self, IedClientError* error, const char* logReference,
        uint64_t startTime, uint64_t endTime, IedConnection_QueryLogHandler handler, void* parameter);

LIB61850_API uint32_t
IedConnection_queryLogAfterAsync(IedConnection self, IedClientError* error, const char* logReference,
        MmsValue* entryID, uint64_t timeStamp, IedConnection_QueryLogHandler handler, void* parameter);

typedef struct sFileDirectoryEntry* FileDirectoryEntry;

/// @deprecated Will be removed from API
LIB61850_API FileDirectoryEntry FileDirectoryEntry_create(const char* fileName, uint32_t fileSize, uint64_t lastModified);

/**
 * @brief Destroy a FileDirectoryEntry object (free all resources)
 * NOTE: Usually is called as a parameter of the \ref LinkedList_destroyDeep function.
 * @param self the FileDirectoryEntry object
 */
LIB61850_API void FileDirectoryEntry_destroy(FileDirectoryEntry self);

/**
 * @brief Get the name of the file
 *
 * @param self the FileDirectoryEntry object
 *
 * @return name of the file as null terminated string
 */
LIB61850_API const char*
FileDirectoryEntry_getFileName(FileDirectoryEntry self);

/**
 * @brief Get the file size in bytes
 *
 * @param self the FileDirectoryEntry object
 *
 * @return size of the file in bytes, or 0 if file size is unknown
 */
LIB61850_API uint32_t
FileDirectoryEntry_getFileSize(FileDirectoryEntry self);

/**
 * @brief Get the timestamp of last modification of the file
 *
 * @param self the FileDirectoryEntry object
 *
 * @return UTC timestamp in milliseconds
 */
LIB61850_API uint64_t
FileDirectoryEntry_getLastModified(FileDirectoryEntry self);


/**
 * @brief 返回指定文件目录的目录条目
 * 需要服务器支持文件服务
 * NOTE: 返回的链表必须由用户释放。您可以使用以下语句释放目录项列表：其中 fileNames 是该函数的返回值
 * LinkedList_destroyDeep(fileNames, (LinkedListValueDeleteFunction) FileDirectoryEntry_destroy);
 * @return 返回目录条目列表。返回类型为包含 FileDirectoryEntry 元素的 LinkedList
 */
LIB61850_API LinkedList /*<FileDirectoryEntry>*/
IedConnection_getFileDirectory(IedConnection self, IedClientError* error, const char* directoryName);


/**
 * @brief 返回由单个文件目录请求返回的指定文件目录的目录条目
 * 需要服务器支持文件服务
 * 此函数只会创建一个请求，结果可能仅限于单个 MMS PDU 所能容纳的目录。
 * 如果服务器包含更多目录条目，则会通过设置 moreFollows 变量（如果调用方提供了该变量）来指示。
 * 如果目录条目无法放入单个 MMS PDU 中则可以通过将 continueAfter 参数设置为接收到的列表中最后一个文件名的值来请求目录列表的下一部分
 * NOTE: 返回的链表必须由用户释放
 * @return the list of directory entries. The return type is a LinkedList with FileDirectoryEntry elements
 */
LIB61850_API LinkedList /*<FileDirectoryEntry>*/
IedConnection_getFileDirectoryEx(IedConnection self, IedClientError* error, 
                                const char* directoryName, const char* continueAfter, bool* moreFollows);

/**
 * @brief 获取文件目录服务的回调处理程序
 * 对于每个文件目录条目，此方法将调用一次；在最后一个条目之后，如果 `moreFollows = false`，则表示不会再有后续数据。
 * 如果发生错误，回调函数将被调用，但 `err != IED_ERROR_OK` 且 `moreFollows = false`
 * @param invokeId 请求的调用 ID
 * @param parameter user provided parameter
 * @param err error code in case of a problem, otherwise IED_ERROR_OK
 * @param filename 当前文件目录条目的文件名，如果没有更多条目，则为 NULL
 * @param size 当前文件目录项的文件大小（以字节为单位）
 * @param lastModified 当前文件目录条目的最后修改时间戳
 * @return 当请求必须停止时（不再调用回调函数），返回 false；否则返回 true
 */
typedef bool
(*IedConnection_FileDirectoryEntryHandler) (uint32_t invokeId, void* parameter, IedClientError err, 
                                char* filename, uint32_t size, uint64_t lastModfified, bool moreFollows);

/**
 * @brief 获取文件目录（单次请求）- 异步版本
 * 对于接收到的每个文件目录条目，都会调用提供的处理程序
 * NOTE: This will only cause a single MMS request. When the resulting file directory doesn't fit into
 * a single MMS PDU another request has to be sent indicating a continuation point with the continueAfter
 * parameter.
 * @param directoryName the name of the directory or NULL to get the entries of the root directory
 * @param continueAfter 以上次接收到的文件名作为继续操作的文件名，如果是第一次请求，则返回 NULL
 * @param handler the callback handler
 * @param parameter user provided callback parameter
 * @return 第一个文件目录请求的 invokeId
 */
LIB61850_API uint32_t
IedConnection_getFileDirectoryAsyncEx(IedConnection self, IedClientError* error, 
                        const char* directoryName, const char* continueAfter,
                        IedConnection_FileDirectoryEntryHandler handler, void* parameter);

/**
 * @brief 用户提供的处理程序用于接收 GetFile 请求的数据
 * 每当客户端从服务器接收到数据块时，都会调用此处理程序
 * API 用户必须先将数据复制到另一个位置才能返回。例如，该位置可以是客户端文件系统中的一个文件
 * @param parameter user provided parameter
 * @param buffer 指向包含接收数据的缓冲区的指针
 * @param bytesRead 缓冲区中可用的字节数
 * @return 如果客户端实现需要继续下载数据则返回 true；如果下载应该停止则返回 false。例如，如果由于资源不足导致文件无法在客户端存储。
 */
typedef bool (*IedClientGetFileHandler) (void* parameter, uint8_t* buffer, uint32_t bytesRead);


/**
 * @brief 实现 GetFile ACSI 服务
 * Download a file from the server.
 * @param fileName 要从服务器读取的文件名
 * @return 接收到的字节数
 */
LIB61850_API uint32_t
IedConnection_getFile(IedConnection self, IedClientError* error, const char* fileName, 
                      IedClientGetFileHandler handler, void* handlerParameter);


/**
 * @brief User provided handler to receive the data of the asynchronous GetFile request
 *
 * This handler will be invoked whenever the clients receives a data block from
 * the server. The API user has to copy the data to another location before returning.
 * The other location could for example be a file in the clients file system. When the
 * last data block is received the moreFollows parameter will be set to false.
 *
 * @param invokeId invoke ID of the message containing the received data
 * @param parameter user provided parameter passed to the callback
 * @param err error code in case of an error or IED_ERROR_OK
 * @param originalInvokeId the invoke ID of the original (first) request. This is usually the request to open the file.
 * @param buffer the buffer that contains the received file data
 * @param bytesRead the number of bytes read into the buffer
 * @param moreFollows indicates that more file data is following
 *
 * @return true, continue the file download when moreFollows is true, false, stop file download
 */
typedef bool
(*IedConnection_GetFileAsyncHandler) (uint32_t invokeId, void* parameter, IedClientError err, uint32_t originalInvokeId,
        uint8_t* buffer, uint32_t bytesRead, bool moreFollows);


/** @todo sungh 这个函数可以用于客户端获取服务端的数据模型配置文件（异步方式调用）
 * @brief GetFile ACSI 服务的实现 - 异步版本
 * 从服务器下载文件
 * NOTE: 此功能可能会发送多个请求消息，直到接收到完整文件或文件传输被取消为止。它会分配一个后台任务和一个待处理的调用时隙
 * @param fileName 要从服务器读取的文件名
 * @param hander 针对每个接收到的数据或错误消息调用的回调处理程序
 * @param parameter 用户提供的回调参数
 * @return 首次发送请求的 invokeId
 */
LIB61850_API uint32_t
IedConnection_getFileAsync(IedConnection self, IedClientError* error, const char* fileName, 
                           IedConnection_GetFileAsyncHandler handler, void* parameter);

/**
 * @brief 为 setFile 服务设置虚拟文件存储基本路径
 * 所有外部文件服务访问都将映射到相对于根目录的路径
 * NOTE: 此功能仅在 stack_config.h 中的 CONFIG_SET_FILESTORE_BASEPATH_AT_RUNTIME 选项设置后可用
 * @param basepath 新的虚拟文件存储基路径
 */
LIB61850_API void IedConnection_setFilestoreBasepath(IedConnection, const char* basepath);


/** @todo sungh 这个函数可以用于从客户端向服务端上传文件
 * @brief 实现 SetFile ACSI 服务
 * 将文件上传到服务器。该文件必须存在于本地 VMD 文件存储中
 * @param sourceFilename 本地（客户端）文件的文件名
 * @param destinationFilename 远程（服务端）文件的文件名
 */
LIB61850_API void
IedConnection_setFile(IedConnection self, IedClientError* error, const char* sourceFilename, const char* destinationFilename);


/**
 * @brief SetFile ACSI 服务的实现 - 异步版本
 * Upload a file to the server. The file has to be available in the local VMD filestore.
 * @param sourceFilename the filename of the local (client side) file
 * @param destinationFilename the filename of the remote (service side) file
 * @param handler 当收到获取文件响应时，将调用回调处理程序
 * @param parameter user provided callback parameter
 */
LIB61850_API uint32_t
IedConnection_setFileAsync(IedConnection self, IedClientError* error, 
                           const char* sourceFilename, const char* destinationFilename,
                           IedConnection_GenericServiceHandler handler, void* parameter);

/**
 * @brief Implementation of the DeleteFile ACSI service
 *
 * Delete a file at the server.
 *
 * @param self the connection object
 * @param error the error code if an error occurs
 * @param fileName the name of the file to delete
 */
LIB61850_API void
IedConnection_deleteFile(IedConnection self, IedClientError* error, const char* fileName);

/**
 * @brief Implementation of the DeleteFile ACSI service - asynchronous version
 *
 * Delete a file at the server.
 *
 * @param self the connection object
 * @param error the error code if an error occurs
 * @param fileName the name of the file to delete
 * @param handler callback handler that is called when the obtain file response has been received
 * @param parameter user provided callback parameter
 */
LIB61850_API uint32_t
IedConnection_deleteFileAsync(IedConnection self, IedClientError* error, const char* fileName,
        IedConnection_GenericServiceHandler handler, void* parameter);

#ifdef __cplusplus
}
#endif


#endif /* IEC61850_CLIENT_H_ */
