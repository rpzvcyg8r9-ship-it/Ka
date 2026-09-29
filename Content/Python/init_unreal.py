# -*- coding: utf-8 -*-
"""
Wird vom Unreal Editor beim Start automatisch ausgefuehrt.
- Fuegt das Menue "Eisenwal" in die Hauptmenueleiste ein.
- Richtet das Projekt beim allerersten Start automatisch ein (wenn L_Ozean_Main fehlt).
"""
import importlib
import unreal

OWNER = "EisenwalTools"


def _run(func_name):
    import eisenwal_setup
    importlib.reload(eisenwal_setup)
    getattr(eisenwal_setup, func_name)()


@unreal.uclass()
class EisenwalMenuEntry(unreal.ToolMenuEntryScript):
    func_name = unreal.uproperty(str)

    @unreal.ufunction(override=True)
    def execute(self, context):
        _run(self.func_name)


def _add_entry(menu, name, label, tooltip, func_name):
    entry = EisenwalMenuEntry()
    entry.func_name = func_name
    entry.init_entry(OWNER, menu.get_name(), "Eisenwal", name, label, tooltip)
    menu.add_menu_entry_object(entry)


def register_menu():
    menus = unreal.ToolMenus.get()
    main = menus.find_menu("LevelEditor.MainMenu")
    if main is None:
        return
    sub = main.add_sub_menu(OWNER, "", "Eisenwal", "Eisenwal", "EISENWAL Werkzeuge")
    sub.add_section("Eisenwal", "Eisenwal")
    _add_entry(sub, "Setup", "Projekt einrichten / aktualisieren",
               "Ordner, Materialien, DataTables und Level erzeugen bzw. aktualisieren", "run_all")
    _add_entry(sub, "DataTables", "DataTables aus CSV neu laden",
               "Alle Balancing-Tabellen aus Content/Eisenwal/Data/CSV neu einlesen", "import_data_tables")
    _add_entry(sub, "Archiv", "Alte Assets archivieren",
               "Alles ausserhalb von /Game/Eisenwal nach /Game/_ALT_Archiv verschieben", "archive_old_assets")
    menus.refresh_all_widgets()


_tick_handle = None
_ticks = 0


def _first_start_check(delta):
    global _tick_handle, _ticks
    _ticks += 1
    if _ticks < 30:  # ein paar Frames warten, bis der Editor fertig geladen ist
        return
    unreal.unregister_slate_post_tick_callback(_tick_handle)
    import eisenwal_setup
    if not unreal.EditorAssetLibrary.does_asset_exist(eisenwal_setup.MAP_PATH):
        unreal.log("[Eisenwal] Erster Start erkannt - richte Projekt automatisch ein ...")
        _run("run_all")


try:
    register_menu()
    _tick_handle = unreal.register_slate_post_tick_callback(_first_start_check)
except Exception as exc:  # Editor-Start darf nie an diesem Skript scheitern
    unreal.log_warning("[Eisenwal] init_unreal.py: {}".format(exc))
