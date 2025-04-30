import pathlib


# Parent directory (builds paths)
BASE_DIR = pathlib.Path(__file__).resolve().parent

# Standart width of QR code
PLATE_WIDTH = 0.44

# Path to the models output
MODELS_DIR = BASE_DIR / 'output'
# MODELS_DIR = BASE_DIR.parent / 'model_editor_models'
