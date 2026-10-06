"""Camera sources and person detectors for the occupant count.

Privacy: frames are processed in memory only. Nothing here saves an image or
sends one anywhere (CIS-SYS-FUN-024); only the number of people leaves the
program.
"""

from typing import Any


class CameraError(Exception):
    """The camera could not be opened or read."""


# --------------------------------------------------------------------------
# Camera sources
# --------------------------------------------------------------------------

class PiCameraSource:
    """Raspberry Pi Camera Module 3 through picamera2 (libcamera)."""

    def __init__(self, width: int = 640, height: int = 480) -> None:
        try:
            from picamera2 import Picamera2
        except ImportError as exc:
            raise CameraError("picamera2 is not installed "
                              "(sudo apt install python3-picamera2)") from exc
        try:
            self._cam = Picamera2()
            config = self._cam.create_video_configuration(
                main={"size": (width, height), "format": "RGB888"})
            self._cam.configure(config)
            self._cam.start()
        except Exception as exc:                       # no camera, cable, busy ...
            raise CameraError(f"cannot start the camera: {exc}") from exc

        # Camera Module 3 has autofocus: keep it focusing continuously.
        # (Ignored on cameras without autofocus.)
        try:
            from libcamera import controls
            self._cam.set_controls({"AfMode": controls.AfModeEnum.Continuous})
        except Exception:
            pass

    def read(self) -> Any:
        """Returns one frame as a numpy array. picamera2's "RGB888" format is in
        BGR pixel order, which is what OpenCV expects."""
        try:
            return self._cam.capture_array()
        except Exception as exc:
            raise CameraError(f"cannot read a frame: {exc}") from exc

    def close(self) -> None:
        try:
            self._cam.stop()
            self._cam.close()
        except Exception:
            pass


class CvSource:
    """Any OpenCV source: a USB camera index (0, 1, ...) or a video file path.
    Handy for trying the detector on a PC."""

    def __init__(self, source: str) -> None:
        import cv2
        self._cv2 = cv2
        self._cap = cv2.VideoCapture(int(source) if source.isdigit() else source)
        if not self._cap.isOpened():
            raise CameraError(f"cannot open the video source {source!r}")

    def read(self) -> Any:
        ok, frame = self._cap.read()
        if not ok:
            raise CameraError("cannot read a frame")
        return frame

    def close(self) -> None:
        self._cap.release()


# --------------------------------------------------------------------------
# Detectors: each has count(frame) -> number of people in the picture
# --------------------------------------------------------------------------

class HogDetector:
    """OpenCV's built-in HOG + SVM pedestrian detector. No model file needed,
    but it is made for standing people seen head to toe: it often misses seated
    or partly visible people. Use it to check the setup; use YoloDetector for
    the real cabin view."""

    name = "hog"

    def __init__(self, min_weight: float = 0.4) -> None:
        import cv2
        import numpy as np
        self._cv2 = cv2
        self._np = np
        self._min_weight = min_weight
        self._hog = cv2.HOGDescriptor()
        self._hog.setSVMDetector(cv2.HOGDescriptor_getDefaultPeopleDetector())

    def count(self, frame: Any) -> int:
        cv2, np = self._cv2, self._np
        h, w = frame.shape[:2]
        if w > 640:                                    # smaller picture = faster
            frame = cv2.resize(frame, (640, int(h * 640 / w)))
        rects, weights = self._hog.detectMultiScale(
            frame, winStride=(8, 8), padding=(8, 8), scale=1.05)
        if len(rects) == 0:
            return 0
        boxes = [[int(x), int(y), int(bw), int(bh)] for (x, y, bw, bh) in rects]
        scores = [float(s) for s in np.ravel(weights)]
        keep = cv2.dnn.NMSBoxes(boxes, scores, self._min_weight, 0.4)
        return len(keep)


class YoloDetector:
    """YOLO (ultralytics) person detector. Much better for people sitting in a
    car. The weights file is downloaded automatically the first time."""

    name = "yolo"

    def __init__(self, model: str = "yolov8n.pt", conf: float = 0.4, imgsz: int = 320) -> None:
        from ultralytics import YOLO
        self._model = YOLO(model)
        self._conf = conf
        self._imgsz = imgsz

    def count(self, frame: Any) -> int:
        # class 0 = person in the COCO dataset
        results = self._model.predict(frame, classes=[0], conf=self._conf,
                                      imgsz=self._imgsz, verbose=False)
        return len(results[0].boxes)


def make_detector(kind: str):
    """kind: 'yolo', 'hog' or 'auto' (yolo if it is installed, otherwise hog)."""
    if kind == "hog":
        return HogDetector()
    if kind == "yolo":
        return YoloDetector()
    try:
        return YoloDetector()
    except ImportError:
        return HogDetector()
