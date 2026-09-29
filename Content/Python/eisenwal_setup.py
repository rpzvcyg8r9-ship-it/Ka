# -*- coding: utf-8 -*-
"""
EISENWAL - Projekt-Setup im Unreal Editor.

Erzeugt alles, was sich nicht als Text im Git-Repo speichern laesst:
Ordner, Materialien, DataTables (aus den CSV-Dateien) und das Level L_Ozean_Main.
Jeder Schritt ist idempotent: bereits vorhandene Assets werden nicht ueberschrieben
(ausser DataTables, die immer frisch aus den CSVs geladen werden -> Balancing per CSV).

Aufruf: Editor-Menue "Eisenwal > Projekt einrichten" oder
        Tools > Execute Python Script > Content/Python/eisenwal_setup.py
"""
import os
import unreal

ROOT = "/Game/Eisenwal"
ARCHIVE = "/Game/_ALT_Archiv"
MAP_PATH = ROOT + "/Maps/L_Ozean_Main"

FOLDERS = [
    "Maps", "Blueprints", "Materials", "Meshes", "Data", "UI", "FX", "Audio", "Input",
]

# (CSV-Datei, Asset-Name, C++-Zeilenstruktur)
DATA_TABLES = [
    ("DT_Seegang.csv", "DT_Seegang", "/Script/Eisenwal.EisenwalSeaStateRow"),
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[Eisenwal] " + msg)


def warn(msg):
    unreal.log_warning("[Eisenwal] " + msg)


# ---------------------------------------------------------------------------------------------
# Ordner / Archiv
# ---------------------------------------------------------------------------------------------

def create_folders():
    for f in FOLDERS:
        path = "{}/{}".format(ROOT, f)
        if not eal.does_directory_exist(path):
            eal.make_directory(path)
    log("Ordnerstruktur ok.")


def archive_old_assets():
    """Verschiebt alles ausserhalb von /Game/Eisenwal nach /Game/_ALT_Archiv (nichts wird geloescht)."""
    keep = (ROOT, ARCHIVE, "/Game/Python", "/Game/Developers", "/Game/Collections")
    moved = 0
    for asset in eal.list_assets("/Game", recursive=True, include_folder=False):
        package = asset.split(".")[0]
        if package.startswith(keep):
            continue
        target = ARCHIVE + package[len("/Game"):]
        if eal.does_asset_exist(target):
            continue
        if eal.rename_asset(package, target):
            moved += 1
    log("{} alte Assets nach {} verschoben.".format(moved, ARCHIVE))


# ---------------------------------------------------------------------------------------------
# Material-Helfer
# ---------------------------------------------------------------------------------------------

def _new_material(name, folder="Materials"):
    path = "{}/{}".format(ROOT, folder)
    full = "{}/{}".format(path, name)
    if eal.does_asset_exist(full):
        return None
    return asset_tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())


def _scalar(mat, name, value, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def _vector(mat, name, rgb, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return e


def _custom(mat, code, inputs, output_type, x, y, desc="Custom"):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    e.set_editor_property("code", code)
    e.set_editor_property("description", desc)
    e.set_editor_property("output_type", output_type)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    e.set_editor_property("inputs", ins)
    return e


def _lerp(mat, a, b, alpha, x, y, a_out="", b_out="", alpha_out=""):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    mel.connect_material_expressions(a, a_out, e, "A")
    mel.connect_material_expressions(b, b_out, e, "B")
    mel.connect_material_expressions(alpha, alpha_out, e, "Alpha")
    return e


def _save(asset):
    eal.save_loaded_asset(asset)


# ---------------------------------------------------------------------------------------------
# Materialien
# ---------------------------------------------------------------------------------------------

RIPPLE_HLSL = """
float2 p = UV * 8.0;
float2 d = float2(0.0, 0.0);
d += float2(cos(p.x * 1.7 + T * 1.3), cos(p.y * 1.3 - T * 1.1)) * 0.10;
d += float2(cos((p.x + p.y) * 3.1 + T * 2.2), cos((p.x - p.y) * 2.7 - T * 1.9)) * 0.06;
d += float2(cos(p.x * 7.3 - p.y * 5.1 + T * 3.7), cos(p.y * 6.9 + p.x * 4.3 - T * 3.1)) * 0.035;
return normalize(float3(d * Strength, 1.0));
"""


def create_ocean_material():
    """M_Ozean: Vertex-Farbe R = Schaum, G = Wellenkamm. Kleine Riffel als prozedurale Normale."""
    mat = _new_material("M_Ozean")
    if mat is None:
        return
    mat.set_editor_property("two_sided", True)

    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1400, 0)
    deep = _vector(mat, "TiefesWasser", (0.003, 0.018, 0.030), -1400, -400)
    crest = _vector(mat, "Wellenkamm", (0.020, 0.110, 0.120), -1400, -250)
    foam = _vector(mat, "Schaum", (0.80, 0.85, 0.88), -1400, -100)

    water = _lerp(mat, deep, crest, vc, -1000, -300, alpha_out="G")
    base = _lerp(mat, water, foam, vc, -700, -200, alpha_out="R")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = _scalar(mat, "Rauheit", 0.04, -1000, 100)
    foam_rough = _scalar(mat, "SchaumRauheit", 0.7, -1000, 200)
    r = _lerp(mat, rough, foam_rough, vc, -700, 150, alpha_out="R")
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)

    spec = _scalar(mat, "Spekular", 0.7, -700, 300)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 450)
    t = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1400, 550)
    strength = _scalar(mat, "RiffelStaerke", 1.0, -1400, 650)
    ripple = _custom(mat, RIPPLE_HLSL, ["UV", "T", "Strength"],
                     unreal.CustomMaterialOutputType.CMOT_FLOAT3, -1000, 500, "Riffel-Normale")
    mel.connect_material_expressions(uv, "", ripple, "UV")
    mel.connect_material_expressions(t, "", ripple, "T")
    mel.connect_material_expressions(strength, "", ripple, "Strength")
    mel.connect_material_property(ripple, "", unreal.MaterialProperty.MP_NORMAL)

    mel.recompile_material(mat)
    _save(mat)
    log("M_Ozean erzeugt.")


STARS_HLSL = """
float3 d = normalize(Dir);
float3 p = d * Density;
float3 c = floor(p);
float3 f = frac(p) - 0.5;
float h = frac(sin(dot(c, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float s = step(1.0 - Chance, h) * saturate(1.0 - length(f) * 3.0);
float twinkle = 0.75 + 0.25 * sin(T * (2.0 + h * 5.0) + h * 40.0);
return s * s * (0.4 + h) * twinkle;
"""


def create_star_material():
    mat = _new_material("M_Sterne")
    if mat is None:
        return
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    n = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 0)
    t = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -900, 100)
    density = _scalar(mat, "Dichte", 380.0, -900, 200)
    chance = _scalar(mat, "Anteil", 0.025, -900, 300)
    stars = _custom(mat, STARS_HLSL, ["Dir", "T", "Density", "Chance"],
                    unreal.CustomMaterialOutputType.CMOT_FLOAT1, -600, 100, "Sterne")
    mel.connect_material_expressions(n, "", stars, "Dir")
    mel.connect_material_expressions(t, "", stars, "T")
    mel.connect_material_expressions(density, "", stars, "Density")
    mel.connect_material_expressions(chance, "", stars, "Chance")

    bright = _scalar(mat, "Helligkeit", 4.0, -600, 300)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 150)
    mel.connect_material_expressions(stars, "", mul, "A")
    mel.connect_material_expressions(bright, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    _save(mat)
    log("M_Sterne erzeugt.")


def create_color_material(name, rgb, roughness=0.6, metallic=0.0, emissive=0.0):
    """Einfaches Platzhalter-Material mit Parameter 'Farbe' (fuer Schiff, Inseln, Effekte)."""
    mat = _new_material(name)
    if mat is None:
        return
    col = _vector(mat, "Farbe", rgb, -600, 0)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = _scalar(mat, "Rauheit", roughness, -600, 150)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    m = _scalar(mat, "Metallisch", metallic, -600, 250)
    mel.connect_material_property(m, "", unreal.MaterialProperty.MP_METALLIC)
    if emissive > 0.0:
        e = _scalar(mat, "Leuchten", emissive, -600, 350)
        mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 300)
        mel.connect_material_expressions(col, "", mul, "A")
        mel.connect_material_expressions(e, "", mul, "B")
        mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    _save(mat)
    log(name + " erzeugt.")


def create_materials():
    create_ocean_material()
    create_star_material()


# ---------------------------------------------------------------------------------------------
# DataTables
# ---------------------------------------------------------------------------------------------

def import_data_tables():
    csv_dir = os.path.join(unreal.Paths.project_content_dir(), "Eisenwal", "Data", "CSV")
    for csv_name, asset_name, struct_path in DATA_TABLES:
        csv_file = os.path.join(csv_dir, csv_name)
        if not os.path.isfile(csv_file):
            warn("CSV fehlt: " + csv_file)
            continue
        struct = unreal.load_object(None, struct_path)
        if struct is None:
            warn("Struktur nicht gefunden (C++ kompiliert?): " + struct_path)
            continue
        full = "{}/Data/{}".format(ROOT, asset_name)
        if eal.does_asset_exist(full):
            table = eal.load_asset(full)
        else:
            factory = unreal.DataTableFactory()
            factory.set_editor_property("struct", struct)
            table = asset_tools.create_asset(asset_name, ROOT + "/Data", unreal.DataTable, factory)
        if table is None:
            warn("Konnte DataTable nicht anlegen: " + asset_name)
            continue
        if unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, csv_file):
            _save(table)
            log("{} aus {} geladen.".format(asset_name, csv_name))
        else:
            warn("CSV-Import fehlgeschlagen: " + csv_name)


# ---------------------------------------------------------------------------------------------
# Level
# ---------------------------------------------------------------------------------------------

def _find_actor(cls):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    for a in actors:
        if isinstance(a, cls):
            return a
    return None


def _ensure_actor(class_path, location, label):
    cls = unreal.load_class(None, class_path)
    if cls is None:
        warn("Klasse nicht gefunden (C++ kompiliert?): " + class_path)
        return None
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    for a in actors:
        if a.get_class() == cls:
            return a
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
        cls, location, unreal.Rotator(0, 0, 0))
    if actor:
        actor.set_actor_label(label)
    return actor


def create_main_level():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if eal.does_asset_exist(MAP_PATH):
        les.load_level(MAP_PATH)
    else:
        if not les.new_level(MAP_PATH):
            warn("Level konnte nicht erzeugt werden: " + MAP_PATH)
            return
        log("Level L_Ozean_Main erzeugt.")

    _ensure_actor("/Script/Eisenwal.EisenwalSky", unreal.Vector(0, 0, 0), "Himmel")
    _ensure_actor("/Script/Eisenwal.EisenwalOcean", unreal.Vector(0, 0, 0), "Ozean")
    for step in LEVEL_STEPS:
        step()
    les.save_current_level()
    log("L_Ozean_Main gespeichert.")


# Spaetere Phasen haengen hier weitere Level-Schritte an (Inseln, Hafen, Schiff-Start ...).
LEVEL_STEPS = []


# ---------------------------------------------------------------------------------------------
# Einstieg
# ---------------------------------------------------------------------------------------------

def run_all():
    with unreal.ScopedSlowTask(4, "EISENWAL wird eingerichtet ...") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "Ordner")
        create_folders()
        task.enter_progress_frame(1, "Materialien")
        create_materials()
        task.enter_progress_frame(1, "DataTables")
        import_data_tables()
        task.enter_progress_frame(1, "Level")
        create_main_level()
    log("Setup fertig.")


if __name__ == "__main__":
    run_all()
