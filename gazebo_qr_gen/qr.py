import config
import pyqrcode
import string


def get_safe_name(s: str) -> str:
    return ''.join((i for i in s if i in string.ascii_letters + string.digits + '_'))


MODEL_NAME = f'qr_{get_safe_name(input("Model name: "))}'
TEXT = get_safe_name(input('Text: '))
WIDTH = input(f'Width of QR code (empty for {config.PLATE_WIDTH}): ')
try:
    WIDTH = float(WIDTH)
except ValueError:
    WIDTH = config.PLATE_WIDTH
MODEL_DIR = config.MODELS_DIR / MODEL_NAME
assert not MODEL_DIR.exists(), (
    f'This model ({MODEL_NAME}) is also created. Remove directory `output/{MODEL_NAME}` before!'
)
(MODEL_DIR / 'materials' / 'textures').mkdir(parents=True)
(MODEL_DIR / 'materials' / 'scripts').mkdir(parents=True)
pyqrcode.create(TEXT).png(MODEL_DIR / 'materials' / 'textures' / f'{MODEL_NAME}.png', scale=1)
with open(MODEL_DIR / 'model.config', 'x') as config_file:
    config_file.write(
f'''<?xml version="1.0"?>
<model>
    <name>{MODEL_NAME}</name>
    <version>1.0</version>
    <sdf version="1.5">model.sdf</sdf>
    <author>
        <name>QR code Generator script</name>
        <email>me@mkme.ml</email>
    </author>
    <description>{MODEL_NAME}</description>
</model>'''
    )
with open(MODEL_DIR / 'model.sdf', 'x') as model_file:
    model_file.write(
f'''<?xml version="1.0"?>
<sdf version="1.5">
    <model name="{MODEL_NAME}">
        <static>true</static>
        <link name="{MODEL_NAME}_link">
            <pose>0 0 1e-3 0 0 1.5707963267948966</pose>
            <visual name="visual_{MODEL_NAME}">
                <cast_shadows>false</cast_shadows>
                <geometry><box><size>{WIDTH} {WIDTH} 1e-3</size></box></geometry>
                <material>
                    <script>
                        <uri>model://{MODEL_NAME}/materials/scripts</uri>
                        <uri>model://{MODEL_NAME}/materials/textures</uri>
                        <name>{MODEL_NAME}/{MODEL_NAME}</name>
                    </script>
                </material>
            </visual>
        </link>
    </model>
</sdf>'''
    )
with open(MODEL_DIR / 'materials' / 'scripts' / 'model.material', 'x') as material_file:
    material_file.write(
f'''material {MODEL_NAME}/{MODEL_NAME}
{{
    technique
    {{
        pass
        {{
            texture_unit
            {{
                texture {MODEL_NAME}.png
                filtering none
                scale 1.0 1.0
            }}
        }}
    }}
}}'''
    )
print(f'\nModel:\t{MODEL_DIR.__str__()}\nCode:\t{TEXT}\nWidth:\t{WIDTH}')
