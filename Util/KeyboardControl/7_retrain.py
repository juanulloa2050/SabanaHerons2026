"""
7_retrain.py — Reentrenamiento del modelo YOLO de detección de balón.

Convierte las sesiones recolectadas con 15_watch_nao.py (Pascal VOC) al
formato YOLO, luego entrena con ultralytics y exporta a ONNX.

Uso:
    python 7_retrain.py                          # usa data/sessions/ por defecto
    python 7_retrain.py --sessions /ruta/sesion  # sesión específica
    python 7_retrain.py --imgsz 320              # resolución del modelo (320 recomendado)
    python 7_retrain.py --base-model yolo_ball_best1.onnx  # fine-tune desde modelo existente

Problemas que corrige vs. el modelo de 160x160:
    - Larga distancia: resolución 320x320 mantiene el balón visible (4–8px → 8–16px)
    - Rotación: augmentación de motion blur durante entrenamiento

Requiere:
    pip install ultralytics
"""

import argparse
import os
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

import cv2
import numpy as np
import yaml

# ── Argumentos ────────────────────────────────────────────────────────────────

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--sessions",    default="data/sessions",
                   help="Directorio raíz con carpetas manual_*/images + annotations")
    p.add_argument("--output",      default="data/yolo_dataset",
                   help="Directorio destino del dataset en formato YOLO")
    p.add_argument("--model",       default="yolov8n.pt",
                   help="Modelo base de ultralytics (yolov8n.pt recomendado para NAO)")
    p.add_argument("--base-model",  default=None,
                   help="ONNX existente para fine-tune (opcional, extrae pesos si es .pt)")
    p.add_argument("--imgsz",       default=320, type=int,
                   help="Resolución de entrenamiento (320 recomendado, 160 mínimo)")
    p.add_argument("--epochs",      default=80, type=int)
    p.add_argument("--val-split",   default=0.15, type=float,
                   help="Fracción de datos para validación")
    p.add_argument("--export-only", action="store_true",
                   help="Solo exportar modelo ya entrenado en runs/detect/last")
    return p.parse_args()

# ── Conversión Pascal VOC → YOLO ──────────────────────────────────────────────

def voc_to_yolo(xml_path: Path, img_w: int, img_h: int) -> list[str]:
    """Devuelve lista de líneas en formato YOLO (clase cx cy w h, normalizados)."""
    tree = ET.parse(xml_path)
    root = tree.getroot()
    lines = []
    for obj in root.findall("object"):
        name = obj.find("name").text.strip().lower()
        if name not in ("trionda", "ball"):
            continue
        bb = obj.find("bndbox")
        xmin = float(bb.find("xmin").text)
        ymin = float(bb.find("ymin").text)
        xmax = float(bb.find("xmax").text)
        ymax = float(bb.find("ymax").text)
        cx = ((xmin + xmax) / 2) / img_w
        cy = ((ymin + ymax) / 2) / img_h
        bw = (xmax - xmin) / img_w
        bh = (ymax - ymin) / img_h
        cx, cy, bw, bh = (max(0., min(1., v)) for v in (cx, cy, bw, bh))
        lines.append(f"0 {cx:.6f} {cy:.6f} {bw:.6f} {bh:.6f}")
    return lines


def build_yolo_dataset(sessions_dir: Path, output_dir: Path, val_split: float):
    sessions_dir = sessions_dir.expanduser().resolve()
    output_dir   = output_dir.expanduser().resolve()

    img_out_train = output_dir / "images" / "train"
    img_out_val   = output_dir / "images" / "val"
    lbl_out_train = output_dir / "labels" / "train"
    lbl_out_val   = output_dir / "labels" / "val"
    for d in (img_out_train, img_out_val, lbl_out_train, lbl_out_val):
        d.mkdir(parents=True, exist_ok=True)

    all_pairs: list[tuple[Path, Path | None]] = []

    for session in sorted(sessions_dir.iterdir()):
        img_dir = session / "images"
        ann_dir = session / "annotations"
        if not img_dir.exists():
            continue
        for img_path in sorted(img_dir.glob("*.jpg")):
            xml_path = ann_dir / (img_path.stem + ".xml")
            all_pairs.append((img_path, xml_path if xml_path.exists() else None))

    if not all_pairs:
        raise RuntimeError(f"No se encontraron imágenes en {sessions_dir}")

    rng = np.random.default_rng(42)
    rng.shuffle(all_pairs)  # type: ignore[arg-type]
    n_val = max(1, int(len(all_pairs) * val_split))
    val_set  = set(range(len(all_pairs) - n_val, len(all_pairs)))

    n_pos = n_neg = 0
    for i, (img_path, xml_path) in enumerate(all_pairs):
        split     = "val" if i in val_set else "train"
        img_out   = (img_out_val   if split == "val" else img_out_train)   / img_path.name
        lbl_out   = (lbl_out_val   if split == "val" else lbl_out_train)   / (img_path.stem + ".txt")

        shutil.copy2(img_path, img_out)

        if xml_path is not None:
            img = cv2.imread(str(img_path))
            h, w = img.shape[:2] if img is not None else (480, 640)
            lines = voc_to_yolo(xml_path, w, h)
            lbl_out.write_text("\n".join(lines))
            if lines:
                n_pos += 1
            else:
                n_neg += 1
        else:
            lbl_out.write_text("")
            n_neg += 1

    data_yaml = output_dir / "data.yaml"
    data_yaml.write_text(yaml.dump({
        "path":  str(output_dir),
        "train": "images/train",
        "val":   "images/val",
        "nc":    1,
        "names": ["trionda"],
    }))

    print(f"Dataset: {n_pos} positivos, {n_neg} negativos → {output_dir}")
    return data_yaml

# ── Entrenamiento ─────────────────────────────────────────────────────────────

def train(data_yaml: Path, model_name: str, imgsz: int, epochs: int):
    from ultralytics import YOLO

    model = YOLO(model_name)

    model.train(
        data=str(data_yaml),
        epochs=epochs,
        imgsz=imgsz,
        batch=16,
        workers=4,
        patience=20,

        # ── Augmentaciones clave para los dos problemas ──────────────────────
        # Larga distancia: mosaic agrupa objetos pequeños y los pone en distintas
        # escalas, forzando al modelo a aprender a detectar balones minúsculos.
        mosaic=1.0,
        scale=0.6,        # zoom-out agresivo: simula balón muy lejano
        copy_paste=0.2,   # pega el balón en fondos distintos a distintas escalas

        # Rotación / motion blur: simula el blur causado por la rotación de
        # cabeza y cuerpo del robot durante la búsqueda activa.
        blur=2.0,         # motion blur sintético (0–3, más alto = más blur)
        degrees=0.0,      # no rotar la imagen (la cámara no rota, rota el robot)

        # Condiciones de cancha: variaciones de iluminación del gimnasio.
        hsv_h=0.015,
        hsv_s=0.7,
        hsv_v=0.5,
        fliplr=0.5,

        # Hiperparámetros de optimización
        lr0=0.005,
        lrf=0.01,
        warmup_epochs=3,
        close_mosaic=10,

        project="runs/detect",
        name="ball_retrain",
        exist_ok=True,
        verbose=True,
    )

    best = Path("runs/detect/ball_retrain/weights/best.pt")
    print(f"\nMejor modelo: {best}")
    return best

# ── Exportar a ONNX ───────────────────────────────────────────────────────────

def export_onnx(pt_path: Path, imgsz: int) -> Path:
    from ultralytics import YOLO

    model = YOLO(str(pt_path))
    model.export(
        format="onnx",
        imgsz=imgsz,
        simplify=True,
        opset=12,
        dynamic=False,
    )
    onnx_path = pt_path.with_suffix(".onnx")
    print(f"\nONNX exportado: {onnx_path}")
    return onnx_path

# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    args = parse_args()

    if args.export_only:
        pt = Path("runs/detect/ball_retrain/weights/best.pt")
        if not pt.exists():
            raise FileNotFoundError(f"No existe {pt} — entrena primero sin --export-only")
        onnx = export_onnx(pt, args.imgsz)
    else:
        data_yaml = build_yolo_dataset(
            Path(args.sessions),
            Path(args.output),
            args.val_split,
        )
        best_pt = train(data_yaml, args.model, args.imgsz, args.epochs)
        onnx    = export_onnx(best_pt, args.imgsz)

    dest = Path(__file__).parent.parent.parent / \
           "Config/NeuralNets/BallDetector/yolo_ball_best1.onnx"
    if dest.exists():
        answer = input(f"\n¿Copiar {onnx.name} a {dest}? [s/N] ").strip().lower()
        if answer == "s":
            shutil.copy2(onnx, dest)
            print(f"Modelo actualizado en {dest}")
    else:
        print(f"\nCopia manual: cp {onnx} {dest}")


if __name__ == "__main__":
    main()
