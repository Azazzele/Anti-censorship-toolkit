import os
import subprocess
import threading
import time
import tkinter as tk
from tkinter import messagebox, ttk


class CiraTunnelController:

    def __init__(self, root):
        self.root = root
        self.root.title("Cira Tunnel Controller")
        self.root.geometry("500x520")
        self.root.resizable(False, True)

        self.client_process = None
        self.is_active = False

        self._create_ui()

    def _create_ui(self):
        self.root.configure(bg="#121212")

        style = ttk.Style()
        style.theme_use("clam")
        style.configure(
            "TLabel", background="#121212", foreground="#00FF00", font=("Consolas", 10)
        )

        title_label = tk.Label(
            self.root,
            text="CENSORED NO // TUNNEL CONTROL",
            bg="#121212",
            fg="#00FF00",
            font=("Consolas", 14, "bold"),
        )
        title_label.pack(pady=10)

        status_frame = tk.LabelFrame(
            self.root,
            text=" SYSTEM STATUS ",
            bg="#121212",
            fg="#00FF00",
            font=("Consolas", 9),
            padx=10,
            pady=5,
        )
        status_frame.pack(fill="x", padx=20, pady=5)

        self.status_indicator = tk.Label(
            status_frame,
            text="● TUNNEL DISCONNECTED",
            bg="#121212",
            fg="#FF0000",
            font=("Consolas", 12, "bold"),
        )
        self.status_indicator.pack(anchor="w")

        self.details_label = tk.Label(
            status_frame,
            text="Waiting for operator initialization...",
            bg="#121212",
            fg="#888888",
            font=("Consolas", 9),
        )
        self.details_label.pack(anchor="w", pady=2)

        log_frame = tk.LabelFrame(
            self.root,
            text=" INTERCOM LOGS ",
            bg="#121212",
            fg="#00FF00",
            font=("Consolas", 9),
        )
        log_frame.pack(fill="x", padx=20, pady=5)

        self.log_text = tk.Text(
            log_frame,
            bg="#1a1a1a",
            fg="#00FF00",
            insertbackground="#00FF00",
            font=("Consolas", 8),
            height=14,
            state="disabled",
        )
        self.log_text.pack(fill="x", padx=5, pady=5)

        self.btn_toggle = tk.Button(
            self.root,
            text="ACTIVATE TUNNEL",
            font=("Consolas", 11, "bold"),
            bg="#003300",
            fg="#00FF00",
            activebackground="#005500",
            activeforeground="#00FF00",
            relief="flat",
            command=self.toggle_tunnel,
        )
        self.btn_toggle.pack(fill="x", padx=20, pady=15)

    def write_log(self, text):
        self.log_text.config(state="normal")
        self.log_text.insert(tk.END, text + "\n")
        self.log_text.see(tk.END)
        self.log_text.config(state="disabled")

    def toggle_tunnel(self):
        if not self.is_active:
            self.start_tunnel()
        else:
            self.stop_tunnel()

    def _read_stream(self, stream, prefix):
        for line in iter(stream.readline, b""):
            try:
                clean_line = line.decode("utf-8", errors="ignore").strip()
                if clean_line:
                    self.write_log(f"[{prefix}] {clean_line}")
            except Exception:
                pass
        stream.close()

    def _set_windows_proxy(self, enable=True):
        """АВТОМАТИЗАЦИЯ СЕТИ: Python сам управляет прокси Windows в фоне."""
        try:
            if enable:
                # Включаем системный прокси на 127.0.0.1:8080
                cmd1 = "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name ProxyServer -Value '127.0.0.1:8080'"
                cmd2 = "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name ProxyEnable -Value 1"
                cmd3 = "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name ProxyOverride -Value '<local>'"
                full_cmd = f"powershell -Command \"{cmd1}; {cmd2}; {cmd3}\""
                subprocess.run(full_cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                self.write_log("[SYSTEM] Windows routing proxy automatically ENGAGED.")
            else:
                # Полностью отключаем прокси при остановке туннеля
                cmd = "Set-ItemProperty -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings' -Name ProxyEnable -Value 0"
                full_cmd = f"powershell -Command \"{cmd}\""
                subprocess.run(full_cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                self.write_log("[SYSTEM] Windows routing proxy automatically DISENGAGED.")
        except Exception as e:
            self.write_log(f"[SYSTEM ERROR] Failed to change network settings: {str(e)}")

    def _kill_zombie_processes(self):
        try:
            # Убиваем вообще всё, что связано с проектом, включая старые имена
            subprocess.run("taskkill /f /im client.exe", shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            subprocess.run("taskkill /f /im protocol.exe", shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            subprocess.run("taskkill /f /im ark_client_v5.exe", shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            subprocess.run("taskkill /f /im ark_server_v5.exe", shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(0.3)
        except Exception:
            pass

    def start_tunnel(self):
        self._kill_zombie_processes()

        self.server_process = None
        self.client_process = None

        from pathlib import Path
        current_dir = Path(__file__).resolve().parent
        root_dir = current_dir.parent
        os.chdir(str(root_dir))

        # СТРОГО 5-Я ВЕРСИЯ, КАК ТЫ И ХОЧЕШЬ
        self.server_path = str(root_dir / "ark_server_v5.exe")
        self.client_path = str(root_dir / "ark_client_v5.exe")
        self.updater_path = str(root_dir / "proxy_updater.py")

        self.write_log(f"[SYSTEM] Working directory shifted to: {root_dir}")
        self.write_log(f"[SYSTEM] Target client path: {self.client_path}")

        self.is_active = True
        self.status_indicator.config(text="● INITIALIZING NETWORK NODES...", fg="#FFFF00")
        self.details_label.config(text="Scraping global proxies baseline. Please wait...", fg="#FFFF00")
        self.btn_toggle.config(text="STOP INITIALIZATION", bg="#330000", fg="#FF0000")

        self.tunnel_thread = threading.Thread(target=self._async_tunnel_launcher, daemon=True, name="CiraLauncherThread")
        self.tunnel_thread.start()


    def _async_tunnel_launcher(self):
        """Фоновый рабочий поток: обновляет прокси и последовательно запускает софты."""
        try:
            self.write_log("[SYSTEM] Fetching 30+ edge networks. Validating delay...")
            
            # Запускаем proxy_updater.py и ждем, пока он сгенерирует proxies.json
            updater_proc = subprocess.Popen(
                f"python \"{self.updater_path}\"", 
                shell=True, 
                stdout=subprocess.PIPE, 
                stderr=subprocess.STDOUT,
                bufsize=0
            )
            
            # Перенаправляем логи чекера прокси прямо в окно интерфейса
            self._read_stream(updater_proc.stdout, "HARVESTER")
            updater_proc.wait()
            
            # Проверяем, не нажал ли оператор отмену во время сборки прокси
            if not self.is_active:
                return

            if not os.path.exists(self.server_path) or not os.path.exists(self.client_path):
                self.root.after(0, lambda: messagebox.showerror(
                    "File Error", 
                    f"Critical error: Binaries missing!\n{self.server_path}\n{self.client_path}"
                ))
                self.stop_tunnel()
                return

            self.write_log("[SYSTEM] Proxy baseline generated. Booting local cluster...")

            # 1. Запуск локального сервера деобфускации protocol.exe (наш мост)
            self.server_process = subprocess.Popen(
                [self.server_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=0,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
            )
            
            threading.Thread(
                target=self._read_stream,
                args=(self.server_process.stdout, "SERVER"),
                daemon=True,
                name="ServerLogThread",
            ).start()

            time.sleep(0.5)

            # 2. Запуск локального прокси-клиента фильтрации client.exe
            self.client_process = subprocess.Popen(
                [self.client_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=0,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
            )

            threading.Thread(
                target=self._read_stream,
                args=(self.client_process.stdout, "CLIENT"),
                daemon=True,
                name="ClientLogThread",
            ).start()

            # Автоматическая конфигурация сетевого адаптера Windows
            self._set_windows_proxy(enable=True)

            # Обновляем визуальный статус системы (безопасно для Tkinter через основной поток)
            self.root.after(0, self._update_ui_to_active)

        except Exception as e:
            self.write_log(f"[LAUNCH ERROR] {str(e)}")
            self.stop_tunnel()

    def _update_ui_to_active(self):
        """Обновляет элементы интерфейса при успешной активации."""
        if self.is_active:
            self.status_indicator.config(
                text="● BRIDGE ACTIVE // TRAFFIC OBFUSCATED", fg="#00FF00"
            )
            self.details_label.config(
                text="Proxy active. Banned sites are now automatically routed through Ark.",
                fg="#00FF00",
            )
            self.btn_toggle.config(
                text="DEACTIVATE TUNNEL", bg="#330000", fg="#FF0000"
            )

    def stop_tunnel(self):
        self.write_log("[SYSTEM] Deactivating bridge. Closing sockets...")

        # Возвращаем интернет Windows в стандартный режим работы
        self._set_windows_proxy(enable=False)

        # Сбрасываем флаг активности (поток лончера поймет, что нужно прерваться)
        self.is_active = False

        if self.client_process:
            try: self.client_process.terminate()
            except Exception: pass
            self.client_process = None

        if self.server_process:
            try: self.server_process.terminate()
            except Exception: pass
            self.server_process = None

        self._kill_zombie_processes()

        # Возвращаем элементы интерфейса в дефолтное состояние
        self.status_indicator.config(text="● SYSTEM OFF", fg="#FF0000")
        self.details_label.config(
            text="Waiting for operator initialization...", fg="#888888"
        )
        self.btn_toggle.config(
            text="ACTIVATE TUNNEL", bg="#003300", fg="#00FF00"
        )
        self.write_log("[SYSTEM] Bridge stopped successfully. Session closed.")

if __name__ == "__main__":
    root = tk.Tk()
    app = CiraTunnelController(root)
    # Метод WM_DELETE_WINDOW гарантирует отключение прокси, даже если окно закрыли на крестик
    root.protocol("WM_DELETE_WINDOW", lambda: [app.stop_tunnel(), root.destroy()])
    root.mainloop()
