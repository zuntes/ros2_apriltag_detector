# Simple creator of QR codes for Gazebo

### 1. Install requirements

```bash
>>> pip install -r requirements.txt
```

### 2. Run script

```bash
>>> python qr.py
Model name: model_name
Text: encoded
Width of QR code (empty for 0.44):

Model:  /home/user/model_editor_models/qr_model_name
Code:   encoded
Width:  0.44
```

### 3. Add to Gazebo

Move folder `output/qrmodel_name` to any Gazebo models folder, for example, `/home/user/model_editor_models/`. Don't forget to reload Gazebo.

---

### Config file

Use `config.py` file to configure models output directory and models width.
