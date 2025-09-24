# version: Python3
# dobot_pick_place_yellow.py
# ----------------------------------------------------------
# Dobot Magician Lite + OpenCV
# - 시작: 홈 이동
# - 카메라에서 노란색 감지 → 픽셀→도봇(X,Y) 변환
# - 스테이징(250,0,50) → 타깃 상부 → 픽업 Z에서 그리퍼 닫기(2초 대기)
# - 드롭(280,-182,11) → 그리퍼 열기 → 홈
# - 화면에 노란색이 없어질 때까지 반복
# ----------------------------------------------------------

from DobotEDU import *
import time, os
import numpy as np
import cv2
from typing import Tuple, Optional

# ----------------- 전역 변수 -----------------
_H_PIX2DOBOT = None  # 보정식(3x3 호모그래피) 저장용
CAL_FILE = "H_pix2dobot.npy"  # 보정행렬 저장 파일

# ----------------- 카메라/색상 파라미터 -----------------
CAM_INDEX = 1
FRAME_W, FRAME_H = 1280, 720
FOURCC_MJPG = True

YELLOW_LOWER = (15, 100, 100)
YELLOW_UPPER = (35, 255, 255)
MIN_AREA     = 800
MORPH_K      = 5

SCAN_TIMEOUT_SEC   = 3.0
STABLE_MIN_FRAMES  = 2

# ----------------- 로봇 포즈 -----------------
SAFE_Z      = 50.0
PICK_Z      = -4.0
DROP_POSE   = (280.0, -182.0, 100.0, 100.0)
STAGE_POSE  = (250.0, 0.0, 50.0, 0.0)

WAIT_HOME   = 3.0
WAIT_MOVE   = 1.5
WAIT_PICK_HOLD = 2.0

# ----------------- 보정용 예시 좌표 -----------------
CAL_PIX   = [(269,268), (376,491), (835,283), (723,502)]
CAL_DOBOT = [(340, 83), (302, 62), (338,-17), (300,  0)]


# ==================================================
#              픽셀 → 도봇 좌표 변환
# ==================================================
def _compute_homography(src_xy, dst_xy):
    src_xy = np.asarray(src_xy, dtype=float)
    dst_xy = np.asarray(dst_xy, dtype=float)
    assert src_xy.shape[0] >= 4 and dst_xy.shape[0] >= 4 and src_xy.shape[0] == dst_xy.shape[0]

    A = []
    for (x, y), (X, Y) in zip(src_xy, dst_xy):
        A.append([ x, y, 1, 0, 0, 0, -x*X, -y*X, -X ])
        A.append([ 0, 0, 0, x, y, 1, -x*Y, -y*Y, -Y ])
    A = np.asarray(A, dtype=float)

    _, _, Vt = np.linalg.svd(A)
    h = Vt[-1]
    H = h.reshape(3, 3)
    return H


def set_calibration(pix_pts, dobot_pts):
    global _H_PIX2DOBOT
    _H_PIX2DOBOT = _compute_homography(pix_pts, dobot_pts)
    return _H_PIX2DOBOT


def save_calibration():
    if _H_PIX2DOBOT is not None:
        np.save(CAL_FILE, _H_PIX2DOBOT)
        print(f"[CAL] saved: {os.path.abspath(CAL_FILE)}")


def load_calibration() -> bool:
    global _H_PIX2DOBOT
    if os.path.exists(CAL_FILE):
        _H_PIX2DOBOT = np.load(CAL_FILE)
        print(f"[CAL] loaded: {os.path.abspath(CAL_FILE)}")
        return True
    return False


def pix_to_dobot_xy(x, y):
    if _H_PIX2DOBOT is None:
        raise RuntimeError("Calibration not set. Call set_calibration(pix_pts, dobot_pts) first.")
    v = np.array([x, y, 1.0], dtype=float)
    w = _H_PIX2DOBOT @ v
    if abs(w[2]) < 1e-9:
        raise ValueError("Homography mapping failed (w ~ 0)")
    return float(w[0] / w[2]), float(w[1] / w[2])


# ==================================================
#              노란색 탐지 (가장 큰 한 개)
# ==================================================
def find_largest_yellow_centroid(cap) -> Optional[Tuple[int, int]]:
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (MORPH_K, MORPH_K))
    t0 = time.time(); stable = 0; last = None

    while time.time() - t0 < SCAN_TIMEOUT_SEC:
        ok, frame = cap.read()
        if not ok:
            continue

        blur = cv2.GaussianBlur(frame, (5,5), 0)
        hsv  = cv2.cvtColor(blur, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, np.array(YELLOW_LOWER), np.array(YELLOW_UPPER))
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=2)

        cnts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        best, best_area = None, 0
        for c in cnts:
            a = cv2.contourArea(c)
            if a < MIN_AREA:
                continue
            if a > best_area:
                best_area = a
                best = c

        if best is None:
            stable = 0; last = None
            continue

        M = cv2.moments(best)
        if M["m00"] == 0:
            continue
        cx = int(M["m10"]/M["m00"])
        cy = int(M["m01"]/M["m00"])

        if last is not None and abs(cx-last[0]) <= 2 and abs(cy-last[1]) <= 2:
            stable += 1
        else:
            stable = 1
            last = (cx, cy)

        if stable >= STABLE_MIN_FRAMES:
            print(f"[YELLOW] target pixel=({cx},{cy}) area={best_area:.0f}")
            return (cx, cy)

    return None


# ==================================================
#              로봇 제어 유틸
# ==================================================
def move_ptp(x, y, z, r=0.0, wait=WAIT_MOVE):
    m_lite.set_ptpcmd(ptp_mode=0, x=float(x), y=float(y), z=float(z), r=float(r))
    time.sleep(wait)


def gripper(on: int, wait=0.5):
    """
    그리퍼 제어: on=1(닫기/집기), on=0(열기/놓기)
    enable=1로 고정(전원 항상 ON)
    """
    m_lite.set_endeffector_gripper(enable=True, on=bool(on))
    time.sleep(wait)


def go_home(wait=WAIT_HOME):
    m_lite.set_homecmd()
    time.sleep(wait)


# ==================================================
#              메인 루프
# ==================================================
def main():
    cap = cv2.VideoCapture(CAM_INDEX, cv2.CAP_DSHOW)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  FRAME_W)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_H)
    if FOURCC_MJPG:
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))

    if not cap.isOpened():
        raise RuntimeError("Failed to open camera. Change CAM_INDEX or device.")

    print("[ROBOT] Go HOME at start")
    go_home()

    # 그리퍼 전원 ON + 기본 열림
    gripper(0)

    # 보정행렬 로드 or 최초 계산
    if not load_calibration():
        print("[CAL] No saved H. Computing from provided pairs...")
        set_calibration(CAL_PIX, CAL_DOBOT)
        save_calibration()

    while True:
        target = find_largest_yellow_centroid(cap)
        if target is None:
            print("[INFO] No yellow detected. Done.")
            time.sleep(2.0)
            continue

        cx, cy = target
        X, Y = pix_to_dobot_xy(cx, cy)
        print(f"[MAP] pixel=({cx},{cy}) -> dobot=({X:.1f},{Y:.1f})")

        # 1) 스테이징
        move_ptp(*STAGE_POSE)
        gripper(0)

        # 2) 픽업
        move_ptp(X, Y, SAFE_Z, r=0.0)
        move_ptp(X, Y, PICK_Z, r=0.0)
        gripper(1)
        time.sleep(WAIT_PICK_HOLD)
        move_ptp(X, Y, SAFE_Z, r=0.0)

        # 3) 드롭
        move_ptp(*DROP_POSE)
        gripper(0)

        # 4) 홈 복귀
        print("[ROBOT] Return HOME")
        go_home()

    cap.release()
    m_lite.set_endeffector_gripper(enable=0, on=0)
    print("[DONE] Script finished.")


if __name__ == "__main__":
    try:
        _has = all(hasattr(m_lite, f) for f in [
            "set_homecmd", "set_ptpcmd", "set_endeffector_gripper"
        ])
    except NameError:
        _has = False

    if not _has:
        raise RuntimeError(
            "m_lite 객체를 찾지 못했습니다. DobotLab의 Python Lab에서 실행하세요."
        )

    main()

