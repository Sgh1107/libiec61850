# -*- coding: utf-8 -*-
"""
mms_client.py

基于 ctypes 封装 libiec61850 动态库，实现 MMS 文件服务的客户端操作：
  - 连接 / 断开 IEC 61850 服务器
  - 列出服务器文件目录（支持子目录导航）
  - 下载文件（带进度回调）
  - 上传文件
  - 删除文件

参考示例: examples/iec61850_client_example_files/file-tool.c
"""

import os
import ctypes
from ctypes import (CFUNCTYPE, POINTER, byref, c_bool, c_char_p, c_int,
                    c_uint8, c_uint32, c_uint64, c_void_p)

# ---------------------------------------------------------------------------
# 常量（对应 IedClientError 枚举）
# ---------------------------------------------------------------------------
IED_ERROR_OK = 0

ERROR_MESSAGES = {
    0:  "成功",
    1:  "一般错误",
    2:  "参数无效",
    3:  "超时",
    4:  "连接被拒绝",
    5:  "连接失败",
    6:  "连接已关闭",
    7:  "连接已中断",
    8:  "服务未实现",
    9:  "服务端不支持",
    10: "超时(超时时间太短)",
    11: "访问被禁止",
    12: "对象不存在",
    13: "对象已存在",
    14: "对象访问被拒绝",
    15: "对象无效",
    16: "实例不存在",
    17: "实例被锁定",
    18: "资源不可用",
    19: "服务端返回异常",
    20: "内部错误(请检查报文)",
}

# ---------------------------------------------------------------------------
# C 结构体定义
# ---------------------------------------------------------------------------


class LinkedList(ctypes.Structure):
    """对应 libiec61850 的 LinkedList 结构: {void* data; LinkedList* next;}"""
    pass


LinkedList._fields_ = [
    ("data", c_void_p),
    ("next", POINTER(LinkedList)),
]

IedConnection = c_void_p
IedClientError = c_int

# 文件下载回调: bool (*)(void* parameter, uint8_t* buffer, uint32_t bytesRead)
DOWNLOAD_CALLBACK = CFUNCTYPE(c_bool, c_void_p, POINTER(c_uint8), c_uint32)
# LinkedList_destroyDeep 的值删除回调
VALUE_DELETE_FUNC = CFUNCTYPE(None, c_void_p)


def _find_library():
    """按常见位置查找 iec61850 动态库"""
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(here, "..", "libiec61850-1.6", "build_win_vs2022", "src", "Debug", "iec61850.dll"),
        os.path.join(here, "..", "libiec61850-1.6", "build_win_vs2022", "src", "Release", "iec61850.dll"),
        os.path.join(here, "..", "libiec61850-1.6", "build2", "src", "Debug", "iec61850.dll"),
        os.path.join(here, "..", "libiec61850-1.6", "build", "src", "Debug", "iec61850.dll"),
        os.path.join(here, "iec61850.dll"),
        os.path.join(here, "..", "libiec61850-1.6", "build", "src", "libiec61850.so"),
        os.path.join(here, "libiec61850.so"),
        "iec61850.dll",
        "libiec61850.so",
    ]
    for cand in candidates:
        cand2 = os.path.normpath(cand)
        if os.path.isfile(cand2):
            return cand2
    return None


def _to_str(b):
    """将 C 字符串(bytes)转为 str，兼容 UTF-8/GBK"""
    if b is None:
        return ""
    if isinstance(b, str):
        return b
    for enc in ("utf-8", "gbk"):
        try:
            return b.decode(enc)
        except UnicodeDecodeError:
            continue
    return b.decode("utf-8", errors="replace")
def error_str(code):
    """把 IedClientError 错误码转成中文描述"""
    return "%s (code=%d)" % (ERROR_MESSAGES.get(code, "未知错误"), code)


class MmsError(Exception):
    """MMS 操作异常"""

    def __init__(self, message, code=None):
        self.code = code
        super().__init__(message)


class MmsFileClient:
    """MMS 文件服务客户端（阻塞式 API，请在工作线程中调用）"""

    def __init__(self, lib_path=None):
        path = lib_path or _find_library()
        if not path:
            raise FileNotFoundError(
                "未找到 iec61850 动态库，请先编译 libiec61850，"
                "或通过 MmsFileClient(lib_path=...) 指定库文件路径")

        self.lib = ctypes.CDLL(path)
        self.lib_path = path
        self._bind_functions()
        self.con = None

    # ------------------------------------------------------------------
    # 函数签名绑定
    # ------------------------------------------------------------------
    def _bind_functions(self):
        L = self.lib

        L.IedConnection_create.restype = IedConnection
        L.IedConnection_create.argtypes = []

        L.IedConnection_destroy.restype = None
        L.IedConnection_destroy.argtypes = [IedConnection]

        L.IedConnection_connect.restype = None
        L.IedConnection_connect.argtypes = [IedConnection, POINTER(IedClientError), c_char_p, c_int]

        L.IedConnection_abort.restype = None
        L.IedConnection_abort.argtypes = [IedConnection, POINTER(IedClientError)]

        L.IedConnection_getFileDirectory.restype = POINTER(LinkedList)
        L.IedConnection_getFileDirectory.argtypes = [IedConnection, POINTER(IedClientError), c_char_p]

        L.IedConnection_getFile.restype = None
        L.IedConnection_getFile.argtypes = [IedConnection, POINTER(IedClientError), c_char_p,
                                            DOWNLOAD_CALLBACK, c_void_p]

        L.IedConnection_setFilestoreBasepath.restype = None
        L.IedConnection_setFilestoreBasepath.argtypes = [IedConnection, c_char_p]

        L.IedConnection_setFile.restype = None
        L.IedConnection_setFile.argtypes = [IedConnection, POINTER(IedClientError), c_char_p, c_char_p]

        L.IedConnection_deleteFile.restype = None
        L.IedConnection_deleteFile.argtypes = [IedConnection, POINTER(IedClientError), c_char_p]

        L.LinkedList_destroyDeep.restype = None
        L.LinkedList_destroyDeep.argtypes = [POINTER(LinkedList), VALUE_DELETE_FUNC]

        L.FileDirectoryEntry_destroy.restype = None
        L.FileDirectoryEntry_destroy.argtypes = [c_void_p]

        L.FileDirectoryEntry_getFileName.restype = c_char_p
        L.FileDirectoryEntry_getFileName.argtypes = [c_void_p]

        L.FileDirectoryEntry_getFileSize.restype = c_uint32
        L.FileDirectoryEntry_getFileSize.argtypes = [c_void_p]

        L.FileDirectoryEntry_getLastModified.restype = c_uint64
        L.FileDirectoryEntry_getLastModified.argtypes = [c_void_p]

    # ------------------------------------------------------------------
    # 连接管理
    # ------------------------------------------------------------------
    def connect(self, host, port=102):
        """连接服务器。失败抛出 MmsError"""
        if self.con is not None:
            self.disconnect()

        con = self.lib.IedConnection_create()
        err = IedClientError()
        self.lib.IedConnection_connect(con, byref(err), host.encode("utf-8"), int(port))
        if err.value != IED_ERROR_OK:
            self.lib.IedConnection_destroy(con)
            raise MmsError("连接 %s:%s 失败: %s" % (host, port, error_str(err.value)))

        self.con = con
        return True

    def disconnect(self):
        if self.con is not None:
            err = IedClientError()
            self.lib.IedConnection_abort(self.con, byref(err))
            self.lib.IedConnection_destroy(self.con)
            self.con = None

    @property
    def connected(self):
        return self.con is not None

    # ------------------------------------------------------------------
    # 文件服务
    # ------------------------------------------------------------------
    def list_dir(self, dirname=""):
        """列出目录内容。dirname 为 "" 表示根目录。返回 list of dict {name, size, mtime}"""
        if self.con is None:
            raise MmsError("未连接到服务器")

        err = IedClientError()
        dirname_c = dirname.encode("utf-8") if dirname else None

        lst = self.lib.IedConnection_getFileDirectory(self.con, byref(err), dirname_c)
        if err.value != IED_ERROR_OK:
            raise MmsError(error_str(err.value), err.value)

        entries = []
        node = lst
        while node:
            entry = node.contents.data
            if entry:
                name = _to_str(self.lib.FileDirectoryEntry_getFileName(entry))
                size = int(self.lib.FileDirectoryEntry_getFileSize(entry))
                mtime = int(self.lib.FileDirectoryEntry_getLastModified(entry))
                entries.append({"name": name, "size": size, "mtime": mtime})
            node = node.contents.next

        deleter = VALUE_DELETE_FUNC(self.lib.FileDirectoryEntry_destroy)
        self.lib.LinkedList_destroyDeep(lst, deleter)
        return entries

    def download(self, remote_filename, local_filename, progress_cb=None):
        """从服务器下载文件。progress_cb(received_bytes) 用于进度显示"""
        if self.con is None:
            raise MmsError("未连接到服务器")

        received = [0]

        @DOWNLOAD_CALLBACK
        def handler(_param, buffer, nbytes):
            try:
                with open(local_filename, "ab") as fp:
                    fp.write(ctypes.string_at(buffer, nbytes))
            except OSError:
                return False
            received[0] += int(nbytes)
            if progress_cb:
                try:
                    progress_cb(received[0])
                except Exception:
                    pass
            return True

        try:
            os.remove(local_filename)
        except OSError:
            pass

        err = IedClientError()
        self.lib.IedConnection_getFile(self.con, byref(err),
                                       remote_filename.encode("utf-8"),
                                       handler, None)
        if err.value != IED_ERROR_OK:
            raise MmsError(error_str(err.value), err.value)

    def upload(self, local_filename, remote_filename=None):
        """上传本地文件到服务器。remote_filename 默认取本地文件名"""
        if self.con is None:
            raise MmsError("未连接到服务器")

        local_filename = os.path.abspath(local_filename)
        if remote_filename is None:
            remote_filename = os.path.basename(local_filename)

        base_dir = os.path.dirname(local_filename)
        if not base_dir.endswith(("/", "\\")):
            base_dir += "/"

        self.lib.IedConnection_setFilestoreBasepath(self.con, base_dir.encode("utf-8"))

        err = IedClientError()
        self.lib.IedConnection_setFile(self.con, byref(err),
                                       os.path.basename(local_filename).encode("utf-8"),
                                       remote_filename.encode("utf-8"))
        if err.value != IED_ERROR_OK:
            raise MmsError(error_str(err.value), err.value)

    def delete(self, remote_filename):
        """删除服务器上的文件"""
        if self.con is None:
            raise MmsError("未连接到服务器")

        err = IedClientError()
        self.lib.IedConnection_deleteFile(self.con, byref(err), remote_filename.encode("utf-8"))
        if err.value != IED_ERROR_OK:
            raise MmsError(error_str(err.value), err.value)

