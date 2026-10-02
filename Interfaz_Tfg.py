import tkinter as tk
import customtkinter as ctk
import serial
import threading
import time

# ==============================================================================
# CONFIGURACIÓN DEL PUERTO SERIE
# ==============================================================================
PUERTO_SERIAL = "COM3"  
BAUD_RATE = 115200

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

class AppMatrizFotovoltaica(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("TFG - Panel de Control Sincronizado")
        self.geometry("650x680")
        self.arduino = None

        self.nombres_config = {
            0: "Circuito Abierto", 17: "Solo Panel 1", 6: "Solo Panel 2",
            26: "Paneles en Serie", 23: "Paneles en Paralelo",
            99: "Error Forzado"
        }

        # Nombres actualizados para los relés (H5 reemplazado por H4)
        self.nombres_reles = [
            ("H1 (Relé 1)", 1),
            ("H2 (Relé 2)", 2),
            ("H3 (Relé 3)", 3),
            ("H4 (Relé 4)", 4),
            ("L2 (Relé 5)", 5)
        ]

        self.dict_botones_manual = {}
        self.switches_reles = []

        # --- CABECERA VISUAL ---
        self.titulo = ctk.CTkLabel(self, text="Matriz de Reconfiguración de Paneles Solares", 
                                   font=ctk.CTkFont(size=20, weight="bold"))
        self.titulo.pack(pady=(15, 5))

        # --- REPLICADOR OLED GLOBAL ---
        self.marco_oled = ctk.CTkFrame(self, width=480, height=120, fg_color="#0a0a0a", corner_radius=10)
        self.marco_oled.pack(pady=5, padx=20)
        self.marco_oled.pack_propagate(False)

        texto_inicial = "--- REPLICADOR OLED ---\nModo: AUTOMÁTICO\nEscuchando transmisiones LoRa..."
        self.txt_oled = ctk.CTkLabel(self.marco_oled, text=texto_inicial,
                                     font=ctk.CTkFont(family="Consolas", size=14, weight="bold"),
                                     text_color="#00ffcc", anchor="w", justify="left")
        self.txt_oled.pack(fill="both", expand=True, padx=20, pady=10)

        self.lbl_estado = ctk.CTkLabel(self, text="Estado: AUTOMÁTICO. Escuchando emisor LoRa...", font=ctk.CTkFont(size=12))
        self.lbl_estado.pack(pady=5)

        # --- SISTEMA DE PANTALLAS (TABVIEW DE 3 PESTAÑAS) ---
        self.tabview = ctk.CTkTabview(self, width=600, height=420)
        self.tabview.pack(pady=10, padx=20, fill="both", expand=True)
        
        self.tab_auto = self.tabview.add("Modo Automático (LoRa)")
        self.tab_manual = self.tabview.add("Modo Manual (Interfaz PC)")
        self.tab_botonera = self.tabview.add("Botonera Relés")

        self.configurar_pantalla_automatica()
        self.configurar_pantalla_manual()
        self.configurar_pantalla_botonera()

        self.conectar_serial()
        
        self.hilo_escucha = threading.Thread(target=self.escuchar_serial, daemon=True)
        self.hilo_escucha.start()

    def conectar_serial(self):
        try:
            self.arduino = serial.Serial(PUERTO_SERIAL, BAUD_RATE, timeout=0.1)
            self.arduino.setDTR(False)
            time.sleep(0.1)
            self.arduino.setDTR(True)
            self.arduino.reset_input_buffer()
            print(f"[OK] Conectado exitosamente al T-Beam en el puerto: {PUERTO_SERIAL}")
        except Exception as e:
            print(f"[ERROR] No se pudo abrir el puerto {PUERTO_SERIAL}. Detalles: {e}")

    def enviar_comando(self, comando):
        if self.arduino and self.arduino.is_open:
            self.arduino.write(f"{comando}\n".encode())

    # ==========================================================================
    # PESTAÑA 1: MODO AUTOMÁTICO
    # ==========================================================================
    def configurar_pantalla_automatica(self):
        self.btn_modo_auto = ctk.CTkButton(self.tab_auto, text="ACTIVAR MODO AUTOMÁTICO (LORA)", 
                                           fg_color="#2ecc71", hover_color="#27ae60",
                                           font=ctk.CTkFont(size=13, weight="bold"),
                                           command=self.activar_modo_auto_fisico)
        self.btn_modo_auto.pack(pady=30)

    def activar_modo_auto_fisico(self):
        self.enviar_comando("SET_AUTO")
        self.lbl_estado.configure(text="Estado: AUTOMÁTICO. Escuchando emisor LoRa...", text_color="#2ecc71")
        
        # Bloquear opciones manuales y de relés al activar automático
        self.bloquear_controles_manuales(True)
        self.bloquear_controles_botonera(True)
        self.resaltar_boton_manual(None)

    # ==========================================================================
    # PESTAÑA 2: MODO MANUAL
    # ==========================================================================
    def configurar_pantalla_manual(self):
        self.btn_modo_manual = ctk.CTkButton(self.tab_manual, text="ACTIVAR MODO MANUAL (DESACTIVA RADIO)", 
                                           fg_color="#e67e22", hover_color="#d35400",
                                           font=ctk.CTkFont(size=13, weight="bold"),
                                           command=self.activar_modo_manual_fisico)
        self.btn_modo_manual.pack(pady=10)

        lbl_instrucciones = ctk.CTkLabel(self.tab_manual, text="Forzar configuración física de relés:", font=ctk.CTkFont(size=12, weight="bold"))
        lbl_instrucciones.pack(pady=5)

        botones_config = [
            ("Circuito Abierto", 0, "#7f8c8d"),
            ("Solo Panel 1", 17, "#2980b9"),
            ("Solo Panel 2", 6, "#27ae60"),
            ("Paneles en Serie", 26, "#8e44ad"),
            ("Paneles en Paralelo", 23, "#d35400"),
            ("Error Forzado", 99, "#c0392b")
        ]

        for texto, valor, color in botones_config:
            btn = ctk.CTkButton(
                self.tab_manual, 
                text=texto, 
                fg_color=color, 
                font=ctk.CTkFont(size=12, weight="bold"),
                border_width=0,
                border_color="#FFFFFF",
                state="disabled",  # Inicia desactivado
                command=lambda v=valor: self.enviar_comando_manual(v)
            )
            btn.pack(pady=4, fill="x", padx=120)
            self.dict_botones_manual[valor] = btn

    def activar_modo_manual_fisico(self):
        self.enviar_comando("SET_MANUAL")
        self.lbl_estado.configure(text="Estado: MODO MANUAL ACTIVO. Radio bloqueada.", text_color="#e67e22")
        
        # Habilitar botones manuales y bloquear los relés individuales
        self.bloquear_controles_manuales(False)
        self.bloquear_controles_botonera(True)

    def bloquear_controles_manuales(self, bloquear):
        estado = "disabled" if bloquear else "normal"
        for btn in self.dict_botones_manual.values():
            btn.configure(state=estado)

    def enviar_comando_manual(self, valor):
        self.enviar_comando(f"MANUAL:{valor}")
        nombre_limpio = self.nombres_config.get(valor, "Desconocida")
        
        color_texto = "#e74c3c" if valor == 99 else "#f39c12"
        self.lbl_estado.configure(text=f"Estado: MODO MANUAL ACTIVO -> {nombre_limpio}", text_color=color_texto)
        self.resaltar_boton_manual(valor)

    def resaltar_boton_manual(self, valor_activo):
        for val, btn in self.dict_botones_manual.items():
            if val == valor_activo:
                btn.configure(border_width=3)
            else:
                btn.configure(border_width=0)

    # ==========================================================================
    # PESTAÑA 3: BOTONERA RELÉS
    # ==========================================================================
    def configurar_pantalla_botonera(self):
        self.btn_modo_botonera = ctk.CTkButton(self.tab_botonera, text="ACTIVAR MODO BOTONERA (DESACTIVA RADIO)", 
                                             fg_color="#2980b9", hover_color="#1c5980",
                                             font=ctk.CTkFont(size=13, weight="bold"),
                                             command=self.activar_modo_botonera_fisico)
        self.btn_modo_botonera.pack(pady=10)

        lbl_instrucciones = ctk.CTkLabel(self.tab_botonera, text="Control Individual por Relé:", font=ctk.CTkFont(size=12, weight="bold"))
        lbl_instrucciones.pack(pady=5)

        for nombre, num_rele in self.nombres_reles:
            switch = ctk.CTkSwitch(
                self.tab_botonera, 
                text=nombre, 
                font=ctk.CTkFont(size=13, weight="bold"),
                state="disabled",  # Inicia desactivado
                command=lambda idx=num_rele, label=nombre: self.alternar_rele(idx, label)
            )
            switch.pack(pady=6, padx=160, anchor="w")
            self.switches_reles.append((num_rele, switch))

    def activar_modo_botonera_fisico(self):
        self.enviar_comando("SET_MANUAL")
        self.lbl_estado.configure(text="Estado: MODO BOTONERA RELÉS ACTIVO. Radio bloqueada.", text_color="#2980b9")
        
        # Habilitar relés individuales y bloquear las opciones del menú manual
        self.bloquear_controles_botonera(False)
        self.bloquear_controles_manuales(True)
        self.resaltar_boton_manual(None)

    def bloquear_controles_botonera(self, bloquear):
        estado = "disabled" if bloquear else "normal"
        for _, switch in self.switches_reles:
            switch.configure(state=estado)

    def alternar_rele(self, numero_rele, nombre_etiqueta):
        switch_obj = next(sw for idx, sw in self.switches_reles if idx == numero_rele)
        estado = switch_obj.get()
        
        self.enviar_comando(f"RELE:{numero_rele}:{estado}")
        self.lbl_estado.configure(text=f"Estado: MODO BOTONERA -> {nombre_etiqueta} = {'ON' if estado == 1 else 'OFF'}", text_color="#3498db")

    # ==========================================================================
    # HILO SERIE Y COMUNICACIÓN
    # ==========================================================================
    def escuchar_serial(self):
        while True:
            if self.arduino and self.arduino.is_open:
                try:
                    if self.arduino.in_waiting > 0:
                        linea_raw = self.arduino.readline()
                        if linea_raw:
                            linea = linea_raw.decode('utf-8', errors='ignore').strip()
                            
                            if linea.startswith("OLED_DATA|"):
                                partes = linea.split("|")
                                if len(partes) >= 5:
                                    texto_formateado = (
                                        f"{partes[1]}\n"  
                                        f"{partes[2]}\n"  
                                        f"{partes[3]}\n"  
                                        f"{partes[4]}"    
                                    )
                                    self.after(0, self.actualizar_interfaz_pantalla, texto_formateado)
                except Exception as e:
                    print(f"[ERROR LECTURA]: {e}")
            time.sleep(0.02)

    def actualizar_interfaz_pantalla(self, texto_formateado):
        self.txt_oled.configure(text=texto_formateado)
        self.update_idletasks()

if __name__ == "__main__":
    app = AppMatrizFotovoltaica()
    app.mainloop()