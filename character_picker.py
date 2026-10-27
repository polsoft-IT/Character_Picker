import sys
import tkinter as tk
from tkinter import ttk
import json
import os
import subprocess
import pyperclip

def resource_path(relative_path: str) -> str:
    """Zwraca absolutną ścieżkę do zasobu.
    Działa zarówno w trybie deweloperskim jak i w skompilowanym exe (PyInstaller).
    """
    if hasattr(sys, '_MEIPASS'):
        # PyInstaller rozpakowuje zasoby do tymczasowego katalogu _MEIPASS
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), relative_path)

class CharacterPicker:
    def __init__(self, root):
        self.root = root
        self.root.title("Character Picker")
        # Ikona w title barze (lewy górny róg) – działa w dev i w skompilowanym exe
        icon_path = resource_path("character.ico")
        if os.path.exists(icon_path):
            try:
                self.root.iconbitmap(icon_path)
            except Exception:
                pass  # Fallback – brak ikony nie przerywa działania aplikacji
        self.root.resizable(False, False)
        
        # Ścieżka do pliku z ustawieniami
        self.settings_file = "character_settings.json"
        
        # Domyślne znaki
        self.default_chars = [
            '™', '№', '₠', '₧', '§',
            '©', '®', '℗', '✓', '✕',
            '⌁', '↯', '♪', '∞', '↳',
            '⌜', '⌝', '⌞', '⌟', '±',
            '‼', '⁇', '⁈', '←', '→'
        ]
        
        # Wczytaj ustawienia
        self.chars = self.load_settings()
        
        # Flaga trybu edycji
        self.editing_mode = False
        self.edit_entries = []
        
        # Stwórz interfejs
        self.create_ui()
        
    def load_settings(self):
        """Wczytuje ustawienia z pliku lub zwraca domyślne"""
        if os.path.exists(self.settings_file):
            try:
                with open(self.settings_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    # Upewnij się że mamy wystarczająco dużo miejsc
                    while len(data) < 25:
                        data.append('')
                    return data[:25]
            except:
                pass
        return self.default_chars.copy()
    
    def save_settings(self):
        """Zapisuje ustawienia do pliku"""
        with open(self.settings_file, 'w', encoding='utf-8') as f:
            json.dump(self.chars, f, ensure_ascii=False, indent=2)
    
    def create_ui(self):
        """Tworzy główny interfejs użytkownika"""
        # Główna ramka
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Tabela 5x5
        self.char_buttons = []
        for i in range(5):
            for j in range(5):
                idx = i * 5 + j
                btn = ttk.Button(main_frame, text=self.chars[idx], width=5, 
                                command=lambda idx=idx: self.on_char_click(idx))
                btn.grid(row=i, column=j, padx=2, pady=2, ipadx=5, ipady=5)
                self.char_buttons.append(btn)
        
        # Separator
        ttk.Separator(main_frame, orient='horizontal').grid(row=5, column=0, columnspan=5, sticky=(tk.W, tk.E), pady=10)
        
        # Pasek narzędzi - skonfigurowany aby wyśrodkować przyciski
        toolbar = ttk.Frame(main_frame)
        toolbar.grid(row=6, column=0, columnspan=5, sticky=(tk.W, tk.E))
        # Dodajemy puste kolumny po bokach które się rozciągają, aby przyciski były wyśrodkowane
        toolbar.grid_columnconfigure(0, weight=1)
        toolbar.grid_columnconfigure(5, weight=1)
        
        # Edit mode button
        self.edit_btn = ttk.Button(toolbar, text="✏️ Edit", command=self.toggle_edit_mode)
        self.edit_btn.grid(row=0, column=1, padx=2, pady=2)
        
        # Cancel edit button (hidden by default)
        self.cancel_btn = ttk.Button(toolbar, text="❌ Cancel", command=self.cancel_edit_mode)
        self.cancel_btn.grid_remove()  # Hidden initially
        
        # System character map button (Windows: charmap)
        self.charmap_btn = ttk.Button(toolbar, text="⌨️ Character Map", command=self.open_system_charmap)
        self.charmap_btn.grid(row=0, column=3, padx=2, pady=2)
        

        
    def on_char_click(self, index):
        """Obsługa kliknięcia w znak - kopiuje do schowka"""
        char = self.chars[index]
        if char and not self.editing_mode:
            pyperclip.copy(char)
            # Wizualna informacja
            self.char_buttons[index].state(['pressed'])
            self.root.after(100, lambda: self.char_buttons[index].state(['!pressed']))
    
    def toggle_edit_mode(self):
        """Toggles character edit mode"""
        self.editing_mode = not self.editing_mode
        
        if self.editing_mode:
            # Enter edit mode - replace buttons with text fields
            self.edit_entries = []
            for i, btn in enumerate(self.char_buttons):
                # Hide the button
                btn.grid_remove()
                
                # Create edit field
                entry = ttk.Entry(btn.master, width=5)
                entry.insert(0, self.chars[i])
                entry.grid(row=i//5, column=i%5, padx=2, pady=2, ipadx=5, ipady=5)
                self.edit_entries.append(entry)
            
            self.edit_btn.config(text="💾 Save")
            # Show Cancel button next to the Save button
            self.cancel_btn.grid(row=0, column=2, padx=2, pady=2)
            # Hide Character Map button
            self.charmap_btn.grid_remove()
        else:
            # Wyjdź z trybu edycji - zapisz zmiany
            for i, entry in enumerate(self.edit_entries):
                new_char = entry.get().strip()
                self.chars[i] = new_char[:1]  # Tylko pierwszy znak
                entry.destroy()
                # Przywróć przycisk
                self.char_buttons[i].config(text=self.chars[i])
                self.char_buttons[i].grid()
            
            # Zapisz ustawienia
            self.save_settings()
            self.edit_btn.config(text="✏️ Edit")
            # Ukryj przycisk Anuluj
            self.cancel_btn.grid_remove()
            # Pokaż ponownie przycisk System
            self.charmap_btn.grid(row=0, column=3, padx=2, pady=2)
    
    def open_system_charmap(self):
        """Opens Windows system character map (charmap.exe)"""
        try:
            subprocess.Popen(['charmap.exe'])
        except:
            pass  # If launch fails, do nothing
    
    def cancel_edit_mode(self):
        """Cancels edit mode - restores original values without saving changes"""
        # Exit edit mode without saving changes
        for i, entry in enumerate(self.edit_entries):
            entry.destroy()
            # Restore button with original value (we don't modify self.chars)
            self.char_buttons[i].config(text=self.chars[i])
            self.char_buttons[i].grid()
        
        # Exit edit mode
        self.editing_mode = False
        self.edit_entries = []
        self.edit_btn.config(text="✏️ Edit")
        # Hide Cancel button
        self.cancel_btn.grid_remove()
        # Show Character Map button again
        self.charmap_btn.grid(row=0, column=3, padx=2, pady=2)
    

if __name__ == "__main__":
    root = tk.Tk()
    app = CharacterPicker(root)
    root.mainloop()