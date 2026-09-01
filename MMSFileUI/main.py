# -*- coding: utf-8 -*-
"""
main.py — MMS 文件浏览器 GUI

基于 libiec61850 动态库的 IEC 61850 MMS 文件服务图形界面：
  - 连接服务器后可直接浏览文件/目录（双击进入子目录）
  - 下载 / 上传 / 删除文件
  - 无需手动执行 dir 命令

运行:  python main.py
依赖:  Python 3.8+（仅标准库 tkinter），libiec61850 动态库
"""

import os
import sys
import threading
import queue
import time
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

from mms_client import MmsFileClient, MmsError, _find_library

APP_TITLE = "MMS 文件浏览器 (libiec61850)"


def fmt_size(n):
    """文件大小格式化"""
    if n < 1024:
        return "%d B" % n
    for unit in ("KB", "MB", "GB"):
        n /= 1024.0
        if n < 1024:
            return "%.1f %s" % (n, unit)
    return "%.1f TB" % n


def fmt_mtime(ms):
    """毫秒 UTC 时间戳格式化"""
    if not ms:
        return "-"
    try:
        return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(ms / 1000.0))
    except (ValueError, OverflowError):
        return "-"


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("960x620")
        self.minsize(760, 480)

        self.client = None
        self.current_path = ""          # 当前服务器端目录（"" 为根目录）
        self.entries = []               # 当前目录条目缓存
        self.busy = False               # 工作线程忙标志
        self.ui_queue = queue.Queue()   # 工作线程 -> UI 消息队列

        self._build_style()
        self._build_ui()
        self.after(100, self._poll_queue)

        lib = _find_library()
        if lib:
            self.set_status("已加载动态库: %s" % os.path.normpath(lib))
        else:
            self.set_status("未找到 iec61850 动态库！请先编译或将 dll 放到本程序目录", is_error=True)

    # ------------------------------------------------------------------
    # 界面构建
    # ------------------------------------------------------------------
    def _build_style(self):
        style = ttk.Style(self)
        style.theme_use("clam")

        BG = "#f4f6f9"
        ACCENT = "#2563eb"
        ACCENT_DARK = "#1d4ed8"

        self.configure(bg=BG)
        style.configure(".", background=BG, font=("Microsoft YaHei UI", 10))
        style.configure("TFrame", background=BG)
        style.configure("TLabel", background=BG, foreground="#1f2937")
        style.configure("Title.TLabel", font=("Microsoft YaHei UI", 11, "bold"), foreground="#111827")
        style.configure("Status.TLabel", background="#e5e7eb", foreground="#374151",
                        font=("Microsoft YaHei UI", 9))
        style.configure("Err.TLabel", background="#e5e7eb", foreground="#b91c1c",
                        font=("Microsoft YaHei UI", 9))

        style.configure("TButton", padding=(12, 5))
        style.configure("Primary.TButton", background=ACCENT, foreground="white",
                        bordercolor=ACCENT, focusthickness=0)
        style.map("Primary.TButton",
                  background=[("pressed", ACCENT_DARK), ("active", ACCENT_DARK)],
                  foreground=[("disabled", "#cccccc")],
                  bordercolor=[("pressed", ACCENT_DARK)])
        style.configure("Danger.TButton", foreground="#b91c1c")

        style.configure("TEntry", padding=4)

        style.configure("Treeview", rowheight=26, font=("Microsoft YaHei UI", 10),
                        background="white", fieldbackground="white", borderwidth=0)
        style.configure("Treeview.Heading", font=("Microsoft YaHei UI", 10, "bold"),
                        background="#e5e7eb", foreground="#111827", padding=(6, 6))
        style.map("Treeview", background=[("selected", "#dbeafe")],
                  foreground=[("selected", "#1e3a8a")])

    def _build_ui(self):
        # ---------- 顶部：连接栏 ----------
        top = ttk.Frame(self, padding=(12, 10, 12, 4))
        top.pack(fill="x")

        ttk.Label(top, text="MMS 文件浏览器", style="Title.TLabel").pack(side="left", padx=(0, 16))

        ttk.Label(top, text="服务器 IP:").pack(side="left")
        self.host_var = tk.StringVar(value="192.168.31.57")
        ttk.Entry(top, textvariable=self.host_var, width=18).pack(side="left", padx=(4, 10))

        ttk.Label(top, text="端口:").pack(side="left")
        self.port_var = tk.StringVar(value="102")
        ttk.Entry(top, textvariable=self.port_var, width=6).pack(side="left", padx=(4, 10))

        self.btn_connect = ttk.Button(top, text="连接", style="Primary.TButton", command=self.on_connect)
        self.btn_connect.pack(side="left", padx=(0, 6))

        self.btn_disconnect = ttk.Button(top, text="断开", command=self.on_disconnect, state="disabled")
        self.btn_disconnect.pack(side="left")

        # ---------- 路径栏 ----------
        pathbar = ttk.Frame(self, padding=(12, 6, 12, 4))
        pathbar.pack(fill="x")

        self.btn_up = ttk.Button(pathbar, text="⬆ 上级", command=self.on_go_up, state="disabled")
        self.btn_up.pack(side="left", padx=(0, 8))

        self.btn_refresh = ttk.Button(pathbar, text="⟳ 刷新", command=self.on_refresh, state="disabled")
        self.btn_refresh.pack(side="left", padx=(0, 8))

        self.path_var = tk.StringVar(value="/")
        ttk.Label(pathbar, textvariable=self.path_var, style="Title.TLabel").pack(side="left", padx=(8, 0))

        # ---------- 中部：文件列表 ----------
        mid = ttk.Frame(self, padding=(12, 4, 12, 4))
        mid.pack(fill="both", expand=True)

        columns = ("size", "mtime")
        self.tree = ttk.Treeview(mid, columns=columns, show="tree headings", selectmode="extended")
        self.tree.heading("#0", text="名称", anchor="w")
        self.tree.heading("size", text="大小", anchor="e")
        self.tree.heading("mtime", text="修改时间", anchor="w")
        self.tree.column("#0", width=420, minwidth=200)
        self.tree.column("size", width=110, anchor="e", stretch=False)
        self.tree.column("mtime", width=170, anchor="w", stretch=False)

        vsb = ttk.Scrollbar(mid, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=vsb.set)
        self.tree.pack(side="left", fill="both", expand=True)
        vsb.pack(side="right", fill="y")

        self.tree.bind("<Double-1>", self.on_double_click)
        self.tree.bind("<Return>", self.on_double_click)
        self.tree.bind("<<TreeviewSelect>>", lambda _e: self._update_nav_buttons())

        # ---------- 操作按钮栏 ----------
        actions = ttk.Frame(self, padding=(12, 4, 12, 6))
        actions.pack(fill="x")

        self.btn_download = ttk.Button(actions, text="⬇ 下载", style="Primary.TButton",
                                       command=self.on_download, state="disabled")
        self.btn_download.pack(side="left", padx=(0, 8))

        self.btn_upload = ttk.Button(actions, text="⬆ 上传", style="Primary.TButton",
                                     command=self.on_upload, state="disabled")
        self.btn_upload.pack(side="left", padx=(0, 8))

        self.btn_delete = ttk.Button(actions, text="🗑 删除", style="Danger.TButton",
                                     command=self.on_delete, state="disabled")
        self.btn_delete.pack(side="left", padx=(0, 8))

        # ---------- 底部：状态栏 ----------
        bottom = ttk.Frame(self, padding=(12, 2, 12, 8))
        bottom.pack(fill="x")
        self.status_var = tk.StringVar(value="就绪")
        self.status_label = ttk.Label(bottom, textvariable=self.status_var, style="Status.TLabel",
                                      anchor="w", padding=(8, 4))
        self.status_label.pack(fill="x")

    # ------------------------------------------------------------------
    # UI 辅助
    # ------------------------------------------------------------------
    def set_status(self, text, is_error=False):
        self.status_var.set(text)
        self.status_label.configure(style="Err.TLabel" if is_error else "Status.TLabel")

    def _update_nav_buttons(self):
        connected = bool(self.client and self.client.connected)
        state = "normal" if (connected and not self.busy) else "disabled"
        for b in (self.btn_refresh, self.btn_upload, self.btn_disconnect):
            b.configure(state=state)
        has_sel = bool(self.tree.selection()) and state == "normal"
        for b in (self.btn_download, self.btn_delete):
            b.configure(state="normal" if has_sel else "disabled")
        self.btn_up.configure(state="normal" if state == "normal" and self.current_path else "disabled")
        self.btn_connect.configure(state="disabled" if connected and not self.busy else "normal")

    def _run_in_thread(self, fn, after_ok=None, on_error=None):
        """在工作线程中执行阻塞式 MMS 操作，结果经队列回主线程

        :param fn: 阻塞操作，返回值将传给 after_ok(result)
        :param after_ok: 成功回调（主线程执行）
        :param on_error: 失败回调(错误消息)（主线程执行），缺省弹错误框
        """
        if self.busy:
            messagebox.showinfo(APP_TITLE, "有操作正在进行中，请稍候…")
            return
        self.busy = True
        self._update_nav_buttons()

        def worker():
            try:
                result = ("ok", fn())
            except MmsError as e:
                result = ("err", str(e))
            except Exception as e:  # noqa: BLE001
                result = ("err", "内部错误: %r" % e)
            self.ui_queue.put((result[0], result[1], after_ok, on_error))

        threading.Thread(target=worker, daemon=True).start()

    def _poll_queue(self):
        """轮询工作线程结果（主线程执行 UI 更新）"""
        try:
            while True:
                kind, payload, after_ok, on_error = self.ui_queue.get_nowait()
                self.busy = False
                if kind == "ok":
                    self.set_status("操作完成")
                    if after_ok:
                        after_ok(payload)
                    else:
                        self._update_nav_buttons()
                else:
                    if on_error:
                        on_error(payload)
                    else:
                        self.set_status(payload, is_error=True)
                        messagebox.showerror(APP_TITLE, payload)
                    self._update_nav_buttons()
        except queue.Empty:
            pass
        self.after(100, self._poll_queue)

    def set_status_safe(self, text):
        """供工作线程调用的状态更新（经主线程调度）"""
        try:
            self.after(0, self.set_status, text)
        except RuntimeError:
            pass

    # ------------------------------------------------------------------
    # 事件处理
    # ------------------------------------------------------------------
    def on_connect(self):
        host = self.host_var.get().strip()
        try:
            port = int(self.port_var.get().strip())
        except ValueError:
            messagebox.showwarning(APP_TITLE, "端口必须是数字")
            return
        if not host:
            messagebox.showwarning(APP_TITLE, "请输入服务器 IP")
            return

        self.set_status("正在连接 %s:%d …" % (host, port))

        def do_connect():
            self.client = MmsFileClient()
            self.client.connect(host, port)
            return self.client

        def after_ok(_client):
            self.set_status("已连接到 %s:%d" % (host, port))
            self.current_path = ""
            self.on_refresh()

        self._run_in_thread(do_connect, after_ok=after_ok)

    def on_disconnect(self):
        def do_disconnect():
            self.client.disconnect()
            self.client = None

        def after_ok():
            self.tree.delete(*self.tree.get_children())
            self.current_path = ""
            self.path_var.set("/")
            self.set_status("已断开连接")
            self._update_nav_buttons()

        self._run_in_thread(do_disconnect, after_ok=after_ok)

    def on_refresh(self):
        path = self.current_path
        self.set_status("正在获取目录 %s …" % (path or "/"))

        def do_list():
            return self.client.list_dir(path)

        def after_ok(entries):
            self.entries = entries
            self.tree.delete(*self.tree.get_children())
            for e in entries:
                self.tree.insert("", "end",
                                 text="📁 " + e["name"] if e.get("is_dir") else "📄 " + e["name"],
                                 values=(fmt_size(e["size"]), fmt_mtime(e["mtime"])))
            self.path_var.set("/" + path if path else "/")
            self.set_status("目录 %s 共 %d 项" % ("/" + path if path else "/", len(entries)))
            self._update_nav_buttons()

        self._run_in_thread(do_list, after_ok=after_ok)

    def on_go_up(self):
        if not self.current_path:
            return
        if "/" in self.current_path:
            self.current_path = self.current_path.rsplit("/", 1)[0]
        else:
            self.current_path = ""
        self.on_refresh()

    def on_double_click(self, _event):
        """双击：尝试进入子目录；若不是目录则仅提示"""
        sel = self.tree.selection()
        if not sel or not (self.client and self.client.connected):
            return
        name = self.tree.item(sel[0], "text").split(" ", 1)[-1]
        sub = (self.current_path + "/" + name) if self.current_path else name

        def do_probe():
            return self.client.list_dir(sub)

        def after_ok(_entries):
            self.current_path = sub
            self.on_refresh()

        def on_error(_msg):
            self.set_status("“%s” 可能是文件（双击无效），可使用下载/删除操作" % name)

        self._run_in_thread(do_probe, after_ok=after_ok, on_error=on_error)

    def on_download(self):
        sel = self.tree.selection()
        if not sel:
            return
        name = self.tree.item(sel[0], "text").split(" ", 1)[-1]
        remote = (self.current_path + "/" + name) if self.current_path else name

        local = filedialog.asksaveasfilename(title="保存文件", initialfile=name,
                                             parent=self)
        if not local:
            return

        self.set_status("正在下载 %s …" % remote)

        def do_download():
            self.client.download(remote, local,
                                 progress_cb=lambda n: self.set_status_safe(
                                     "已接收 %s (%d bytes)" % (name, n)))
            return local

        def after_ok(path):
            self.set_status("下载完成: %s" % path)
            messagebox.showinfo(APP_TITLE, "下载完成！\n%s" % path)

        self._run_in_thread(do_download, after_ok=after_ok)

    def on_upload(self):
        local = filedialog.askopenfilename(title="选择要上传的文件", parent=self)
        if not local:
            return
        remote_name = os.path.basename(local.replace("\\", "/"))
        remote = (self.current_path + "/" + remote_name) if self.current_path else remote_name

        self.set_status("正在上传 %s -> %s …" % (local, remote))

        def do_upload():
            self.client.upload(local, remote)
            return remote

        def after_ok(path):
            self.set_status("上传完成: %s" % path)
            messagebox.showinfo(APP_TITLE, "上传完成！")
            self.on_refresh()

        self._run_in_thread(do_upload, after_ok=after_ok)

    def on_delete(self):
        sel = self.tree.selection()
        if not sel:
            return
        name = self.tree.item(sel[0], "text").split(" ", 1)[-1]
        remote = (self.current_path + "/" + name) if self.current_path else name

        if not messagebox.askyesno(APP_TITLE, "确定要删除服务器上的 “%s” 吗？\n此操作不可恢复！" % remote):
            return

        self.set_status("正在删除 %s …" % remote)

        def do_delete():
            self.client.delete(remote)

        def after_ok():
            self.set_status("已删除: %s" % remote)
            self.on_refresh()

        self._run_in_thread(do_delete, after_ok=after_ok)


def main():
    app = App()
    app.mainloop()


if __name__ == "__main__":
    main()




